// SPDX-License-Identifier: GPL-2.0-or-later

// Adversarial-input coverage for ProfileStore's file boundary. ProfilePersistence
// Test exercises the happy path (save/load/rename/export/import/delete,
// sanitization, legacy migration, compatibility reporting), but never feeds
// ProfileStore a corrupt file, a non-object document, a structurally invalid
// profile, or a hostile profileName. Profiles are user-supplied JSON on disk and
// can be hand-edited or imported from anywhere, so load() and importFile() must
// reject malformed input with a clear error instead of crashing, and a
// path-traversal profileName must never let an import escape the profiles
// directory. These are the file-at-rest analogue of the daemon wire-protocol
// hardening.

#include "core/ProfileStore.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QTemporaryDir>

namespace {

using namespace lumacore;

int g_failures = 0;

void check(bool condition, const char* message)
{
    if (!condition) {
        qCritical().noquote() << "FAIL:" << message;
        ++g_failures;
    }
}

bool writeFileBytes(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.write(bytes);
    file.close();
    return true;
}

QByteArray profileJson(const QJsonObject& object)
{
    return QJsonDocument(object).toJson(QJsonDocument::Compact);
}

void testNormalizeNameNeutralizesHostileNames()
{
    // The sanitizer collapses every run of characters outside [A-Za-z0-9_-] to a
    // single underscore, so path separators and dot-dot segments cannot survive.
    check(
        ProfileStore::normalizeName(QStringLiteral("../../etc/passwd")) == QStringLiteral("_etc_passwd"),
        "a path-traversal name must collapse its separators and dots to underscores"
    );
    check(
        ProfileStore::normalizeName(QStringLiteral("a/b\\c")) == QStringLiteral("a_b_c"),
        "forward and back slashes must both be replaced"
    );
    check(
        ProfileStore::normalizeName(QStringLiteral("....")) == QStringLiteral("_"),
        "a name of only dots must collapse to a single underscore"
    );
    check(
        ProfileStore::normalizeName(QString()) == QStringLiteral("default"),
        "an empty name must fall back to 'default'"
    );
    check(
        ProfileStore::normalizeName(QStringLiteral("   ")) == QStringLiteral("default"),
        "a whitespace-only name must fall back to 'default'"
    );
    check(
        ProfileStore::normalizeName(QStringLiteral("safe-name_1")) == QStringLiteral("safe-name_1"),
        "an already-safe name must pass through unchanged"
    );
    // The sanitized name never contains a path separator or a traversal segment.
    const QString hostile = ProfileStore::normalizeName(QStringLiteral("../../pwned"));
    check(
        !hostile.contains(QLatin1Char('/')) && !hostile.contains(QLatin1Char('\\'))
            && !hostile.contains(QStringLiteral("..")),
        "a normalized name must never carry a separator or dot-dot segment"
    );
}

void testLoadRejectsMalformedFiles(const ProfileStore& store)
{
    QJsonObject loaded;
    QString error;

    check(
        !store.load(QStringLiteral("does-not-exist"), &loaded, &error)
            && error.contains(QStringLiteral("Could not open profile")),
        "loading a missing profile must fail with an open error"
    );

    check(writeFileBytes(store.filePath(QStringLiteral("broken")), QByteArrayLiteral("{ not valid json ")),
        "broken profile fixture should be written");
    error.clear();
    check(
        !store.load(QStringLiteral("broken"), &loaded, &error)
            && error.contains(QStringLiteral("Invalid profile JSON")),
        "loading syntactically invalid JSON must be rejected"
    );

    check(writeFileBytes(store.filePath(QStringLiteral("array")), QByteArrayLiteral("[1, 2, 3]")),
        "array profile fixture should be written");
    error.clear();
    check(
        !store.load(QStringLiteral("array"), &loaded, &error)
            && error.contains(QStringLiteral("Invalid profile JSON")),
        "loading a valid JSON array (non-object) must be rejected"
    );

    check(writeFileBytes(store.filePath(QStringLiteral("empty")), QByteArray()),
        "empty profile fixture should be written");
    error.clear();
    check(
        !store.load(QStringLiteral("empty"), &loaded, &error)
            && error.contains(QStringLiteral("Invalid profile JSON")),
        "loading an empty file must be rejected"
    );

    // Control: a well-formed object loads and round-trips.
    check(
        writeFileBytes(
            store.filePath(QStringLiteral("good")),
            profileJson({{QStringLiteral("profileName"), QStringLiteral("good")}, {QStringLiteral("devices"), QJsonArray {}}})
        ),
        "good profile fixture should be written"
    );
    loaded = {};
    error.clear();
    check(
        store.load(QStringLiteral("good"), &loaded, &error)
            && error.isEmpty()
            && loaded.value(QStringLiteral("devices")).isArray(),
        "control: a well-formed profile object must load"
    );
}

void testImportRejectsMalformedSources(const ProfileStore& store, const QString& importDir)
{
    QString importedName;
    QString error;

    check(
        !store.importFile(QDir(importDir).filePath(QStringLiteral("missing.json")), &importedName, &error)
            && error.contains(QStringLiteral("Could not open profile import")),
        "importing a missing source file must fail with an open error"
    );

    const QString brokenPath = QDir(importDir).filePath(QStringLiteral("broken.json"));
    check(writeFileBytes(brokenPath, QByteArrayLiteral("{ oops")), "broken import fixture should be written");
    error.clear();
    check(
        !store.importFile(brokenPath, &importedName, &error)
            && error.contains(QStringLiteral("Invalid profile JSON")),
        "importing invalid JSON must be rejected"
    );

    const QString noDevicesPath = QDir(importDir).filePath(QStringLiteral("no-devices.json"));
    check(
        writeFileBytes(noDevicesPath, profileJson({{QStringLiteral("profileName"), QStringLiteral("x")}})),
        "devices-less import fixture should be written"
    );
    error.clear();
    check(
        !store.importFile(noDevicesPath, &importedName, &error)
            && error.contains(QStringLiteral("missing devices array")),
        "importing a profile without a devices array must be rejected"
    );

    const QString wrongTypePath = QDir(importDir).filePath(QStringLiteral("wrong-type.json"));
    check(
        writeFileBytes(wrongTypePath, profileJson({{QStringLiteral("devices"), QStringLiteral("not-an-array")}})),
        "wrong-typed devices import fixture should be written"
    );
    error.clear();
    check(
        !store.importFile(wrongTypePath, &importedName, &error)
            && error.contains(QStringLiteral("missing devices array")),
        "importing a profile whose devices field is not an array must be rejected"
    );
}

void testImportSucceedsAndIsIdempotentlyGuarded(const ProfileStore& store, const QString& importDir)
{
    const QString validPath = QDir(importDir).filePath(QStringLiteral("valid.json"));
    check(
        writeFileBytes(
            validPath,
            profileJson({{QStringLiteral("profileName"), QStringLiteral("Imported Profile")}, {QStringLiteral("devices"), QJsonArray {}}})
        ),
        "valid import fixture should be written"
    );
    QString importedName;
    QString error;
    check(
        store.importFile(validPath, &importedName, &error)
            && importedName == QStringLiteral("Imported_Profile")
            && store.names().contains(QStringLiteral("Imported_Profile")),
        "a valid profile must import under a sanitized name and be listed"
    );

    // Re-importing the same normalized name must be refused, not silently
    // overwritten.
    error.clear();
    check(
        !store.importFile(validPath, nullptr, &error)
            && error.contains(QStringLiteral("already exists")),
        "re-importing an existing profile name must be refused"
    );

    // With no profileName field, the source file's base name is used.
    const QString fallbackPath = QDir(importDir).filePath(QStringLiteral("fallback-source.json"));
    check(
        writeFileBytes(fallbackPath, profileJson({{QStringLiteral("devices"), QJsonArray {}}})),
        "fallback-name import fixture should be written"
    );
    importedName.clear();
    error.clear();
    check(
        store.importFile(fallbackPath, &importedName, &error)
            && importedName == QStringLiteral("fallback-source"),
        "a profile without a profileName must import under its source file base name"
    );
}

void testImportCannotEscapeProfilesDirectory(const ProfileStore& store, const QString& importDir)
{
    // A hostile profileName that tries to traverse out of the profiles directory
    // must be sanitized so the written file stays inside the directory.
    const QString traversalPath = QDir(importDir).filePath(QStringLiteral("traversal.json"));
    check(
        writeFileBytes(
            traversalPath,
            profileJson({{QStringLiteral("profileName"), QStringLiteral("../../pwned")}, {QStringLiteral("devices"), QJsonArray {}}})
        ),
        "traversal import fixture should be written"
    );

    QString importedName;
    QString error;
    const bool imported = store.importFile(traversalPath, &importedName, &error);
    check(imported, "a traversal-named profile should still import under a sanitized name");
    check(
        !importedName.contains(QLatin1Char('/')) && !importedName.contains(QLatin1Char('\\'))
            && !importedName.contains(QStringLiteral("..")),
        "the imported name must be stripped of separators and dot-dot segments"
    );

    // The written file must live directly inside the profiles directory.
    const QString writtenPath = store.filePath(importedName);
    const QString writtenParent = QFileInfo(writtenPath).absolutePath();
    const QString storeRoot = QFileInfo(store.directoryPath()).absoluteFilePath();
    check(
        QDir(writtenParent) == QDir(storeRoot) && QFile::exists(writtenPath),
        "the imported profile must be written inside the profiles directory"
    );

    // The naive traversal target (two levels above the profiles directory) must
    // not have been created.
    const QString escapeTarget = QDir(storeRoot).filePath(QStringLiteral("../../pwned.json"));
    check(
        !QFile::exists(QDir::cleanPath(escapeTarget)),
        "no file may be created outside the profiles directory by a traversal name"
    );
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    Q_UNUSED(application)

    QTemporaryDir storeDir;
    QTemporaryDir importDir;
    if (!storeDir.isValid() || !importDir.isValid()) {
        qCritical().noquote() << "FAIL: temporary directories should be available";
        return 1;
    }

    const ProfileStore store(storeDir.path());

    testNormalizeNameNeutralizesHostileNames();
    testLoadRejectsMalformedFiles(store);
    testImportRejectsMalformedSources(store, importDir.path());
    testImportSucceedsAndIsIdempotentlyGuarded(store, importDir.path());
    testImportCannotEscapeProfilesDirectory(store, importDir.path());

    if (g_failures != 0) {
        qCritical().noquote() << g_failures << "profile store import check(s) failed";
        return 1;
    }
    return 0;
}
