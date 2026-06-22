#include "backends/mock/MockBackend.h"
#include "core/DeviceManager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

namespace {

bool require(bool condition, const char* message)
{
    if (!condition) {
        qCritical().noquote() << message;
    }
    return condition;
}

class CurrentDirectoryGuard
{
public:
    CurrentDirectoryGuard()
        : m_originalPath(QDir::currentPath())
    {
    }

    ~CurrentDirectoryGuard()
    {
        QDir::setCurrent(m_originalPath);
    }

private:
    QString m_originalPath;
};

bool writeProfileFile(const QString& path, const QByteArray& contents)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly | QIODevice::Text)
        && file.write(contents) == contents.size();
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    Q_UNUSED(app)

    QTemporaryDir temporaryDirectory;
    CurrentDirectoryGuard currentDirectoryGuard;
    if (!require(temporaryDirectory.isValid(), "temporary profile directory should be available")
        || !require(QDir::setCurrent(temporaryDirectory.path()), "test should enter the temporary directory")) {
        return 1;
    }

    const QString profilesDirectory = temporaryDirectory.filePath(QStringLiteral("app-data/profiles"));
    QDir().mkpath(QStringLiteral("profiles"));
    if (!require(
            writeProfileFile(QStringLiteral("profiles/legacy.json"), QByteArrayLiteral("{}")),
            "legacy profile fixture should be written"
        )) {
        return 1;
    }

    lumacore::DeviceManager manager(nullptr, profilesDirectory);
    manager.registerBackend(std::make_unique<lumacore::MockBackend>());
    manager.initializeBackends(QStringLiteral("mock"));
    if (!require(manager.deviceCount() == 1, "mock backend should load one device")
        || !require(
            manager.profilesDirectoryPath() == QDir::cleanPath(QFileInfo(profilesDirectory).absoluteFilePath()),
            "profiles should use the configured application-data directory"
        )
        || !require(
            QFile::exists(QDir(profilesDirectory).filePath(QStringLiteral("legacy.json"))),
            "legacy profiles should migrate when the application-data directory is first created"
        )) {
        return 1;
    }

    QFile::remove(QDir(profilesDirectory).filePath(QStringLiteral("legacy.json")));
    const lumacore::ProfileStore reopenedStore(profilesDirectory);
    if (!require(
            reopenedStore.names().isEmpty(),
            "legacy profiles should not return after migration once the new directory exists"
        )) {
        return 1;
    }

    const lumacore::RgbColor savedColor(17, 34, 51);
    const lumacore::RgbColor changedColor(170, 187, 204);
    if (!require(manager.setZoneStaticColor(0, 0, savedColor), "test color should apply before saving")) {
        return 1;
    }

    QString errorMessage;
    if (!require(manager.saveProfile(QStringLiteral("  My Profile  "), &errorMessage), "profile should save")
        || !require(errorMessage.isEmpty(), "successful save should not report an error")
        || !require(manager.profileNames() == QStringList {QStringLiteral("My_Profile")}, "profile name should be sanitized")
        || !require(
            QFile::exists(QDir(profilesDirectory).filePath(QStringLiteral("My_Profile.json"))),
            "sanitized profile file should exist in application data"
        )) {
        return 1;
    }

    if (!require(manager.setZoneStaticColor(0, 0, changedColor), "test color should change after saving")
        || !require(manager.loadProfile(QStringLiteral("My_Profile"), &errorMessage), "saved profile should load")
        || !require(
            manager.deviceAt(0)->zones().at(0).effect().color() == savedColor,
            "loading should restore the saved zone color"
        )) {
        return 1;
    }

    const QString exportedProfilePath = temporaryDirectory.filePath(QStringLiteral("exports/My_Profile.json"));
    QDir().mkpath(QFileInfo(exportedProfilePath).absolutePath());
    if (!require(
            manager.exportProfile(QStringLiteral("My_Profile"), exportedProfilePath, &errorMessage),
            "profile should export"
        )
        || !require(QFile::exists(exportedProfilePath), "exported profile should exist")) {
        return 1;
    }

    if (!require(
            manager.renameProfile(QStringLiteral("My_Profile"), QStringLiteral("Renamed Profile"), &errorMessage),
            "profile should rename"
        )
        || !require(manager.profileNames() == QStringList {QStringLiteral("Renamed_Profile")}, "renamed profile should be listed")
        || !require(
            manager.importProfile(exportedProfilePath, nullptr, &errorMessage),
            "exported profile should import"
        )
        || !require(
            manager.profileNames() == QStringList {QStringLiteral("My_Profile"), QStringLiteral("Renamed_Profile")},
            "imported profile should use its embedded name"
        )
        || !require(
            !manager.importProfile(exportedProfilePath, nullptr, &errorMessage),
            "import should reject an existing profile name"
        )
        || !require(
            errorMessage == QStringLiteral("Profile 'My_Profile' already exists."),
            "duplicate import should explain the conflict"
        )
        || !require(manager.deleteProfile(QStringLiteral("Renamed_Profile"), &errorMessage), "renamed profile should delete")
        || !require(manager.deleteProfile(QStringLiteral("My_Profile"), &errorMessage), "imported profile should delete")
        || !require(manager.profileNames().isEmpty(), "deleted profile should no longer be listed")) {
        return 1;
    }

    if (!require(
            !manager.deleteProfile(QStringLiteral("missing"), &errorMessage),
            "deleting a missing profile should fail"
        )
        || !require(
            errorMessage == QStringLiteral("Profile 'missing' does not exist."),
            "missing profile deletion should preserve its public error text"
        )) {
        return 1;
    }

    const QJsonObject compatibilityProfile {
        {QStringLiteral("formatVersion"), 1},
        {QStringLiteral("application"), QStringLiteral("LumaCore")},
        {QStringLiteral("profileName"), QStringLiteral("compatibility")},
        {QStringLiteral("devices"), QJsonArray {
            QJsonObject {
                {QStringLiteral("id"), manager.deviceAt(0)->id()},
                {QStringLiteral("name"), manager.deviceAt(0)->name()},
                {QStringLiteral("zones"), QJsonArray {
                    QJsonObject {
                        {QStringLiteral("index"), 0},
                        {QStringLiteral("name"), manager.deviceAt(0)->zones().at(0).name()},
                        {QStringLiteral("effect"), QJsonObject {
                            {QStringLiteral("type"), QStringLiteral("Static")},
                            {QStringLiteral("color"), QStringLiteral("#112233")},
                        }},
                    },
                    QJsonObject {
                        {QStringLiteral("index"), 1},
                        {QStringLiteral("name"), manager.deviceAt(0)->zones().at(1).name()},
                        {QStringLiteral("effect"), QJsonObject {
                            {QStringLiteral("type"), QStringLiteral("Sparkle")},
                            {QStringLiteral("color"), QStringLiteral("#445566")},
                        }},
                    },
                    QJsonObject {
                        {QStringLiteral("index"), 999},
                        {QStringLiteral("name"), QStringLiteral("Missing Zone")},
                        {QStringLiteral("effect"), QJsonObject {
                            {QStringLiteral("type"), QStringLiteral("Static")},
                            {QStringLiteral("color"), QStringLiteral("#778899")},
                        }},
                    },
                    QJsonObject {
                        {QStringLiteral("index"), 2},
                        {QStringLiteral("name"), manager.deviceAt(0)->zones().at(2).name()},
                        {QStringLiteral("effect"), QJsonObject {
                            {QStringLiteral("type"), QStringLiteral("Static")},
                            {QStringLiteral("color"), QStringLiteral("not-a-color")},
                        }},
                    },
                }},
            },
            QJsonObject {
                {QStringLiteral("id"), QStringLiteral("missing-device")},
                {QStringLiteral("name"), QStringLiteral("Missing Device")},
                {QStringLiteral("zones"), QJsonArray {
                    QJsonObject {
                        {QStringLiteral("index"), 0},
                        {QStringLiteral("name"), QStringLiteral("Missing Device Zone")},
                    },
                }},
            },
        }},
    };
    if (!require(
            writeProfileFile(
                QDir(profilesDirectory).filePath(QStringLiteral("compatibility.json")),
                QJsonDocument(compatibilityProfile).toJson(QJsonDocument::Compact)
            ),
            "compatibility profile fixture should be written"
        )) {
        return 1;
    }

    const QVariantMap compatibility = manager.profileCompatibility(QStringLiteral("compatibility"));
    if (!require(compatibility.value(QStringLiteral("valid")).toBool(), "compatibility report should be valid")
        || !require(compatibility.value(QStringLiteral("canApply")).toBool(), "one compatible zone should be applicable")
        || !require(compatibility.value(QStringLiteral("totalZones")).toInt() == 5, "report should count stored zones")
        || !require(compatibility.value(QStringLiteral("matchedZones")).toInt() == 3, "report should count matched zones")
        || !require(compatibility.value(QStringLiteral("applicableZones")).toInt() == 1, "report should count applicable zones")
        || !require(compatibility.value(QStringLiteral("missingDevices")).toInt() == 1, "report should count missing devices")
        || !require(compatibility.value(QStringLiteral("missingZones")).toInt() == 2, "report should count missing zones")
        || !require(compatibility.value(QStringLiteral("invalidZones")).toInt() == 1, "report should count invalid effect payloads")
        || !require(
            compatibility.value(QStringLiteral("unsupportedEffects")).toInt() == 1,
            "report should count unsupported effects"
        )) {
        return 1;
    }

    const QVariantMap applyReport = manager.applyProfileWithReport(QStringLiteral("compatibility"));
    if (!require(applyReport.value(QStringLiteral("success")).toBool(), "partial application should report success")
        || !require(applyReport.value(QStringLiteral("partial")).toBool(), "skipped zones should mark the result partial")
        || !require(applyReport.value(QStringLiteral("appliedZones")).toInt() == 1, "one zone should apply")
        || !require(applyReport.value(QStringLiteral("skippedZones")).toInt() == 4, "four zones should be skipped")
        || !require(
            applyReport.value(QStringLiteral("missingDeviceZones")).toInt() == 1,
            "result should count zones on missing devices"
        )
        || !require(applyReport.value(QStringLiteral("missingZones")).toInt() == 1, "result should count missing zones")
        || !require(applyReport.value(QStringLiteral("invalidZones")).toInt() == 2, "result should count invalid effects and colors")
        || !require(applyReport.value(QStringLiteral("failedZones")).toInt() == 0, "compatible mock writes should succeed")) {
        return 1;
    }

    if (!require(
            writeProfileFile(
                QDir(profilesDirectory).filePath(QStringLiteral("malformed.json")),
                QByteArrayLiteral("{not-json")
            ),
            "malformed profile fixture should be written"
        )
        || !require(!manager.loadProfile(QStringLiteral("malformed"), &errorMessage), "malformed JSON should be rejected")
        || !require(errorMessage.contains(QStringLiteral("Invalid profile JSON")), "malformed JSON should report a parse error")) {
        return 1;
    }

    const QString invalidImportPath = temporaryDirectory.filePath(QStringLiteral("invalid-import.json"));
    if (!require(
            writeProfileFile(invalidImportPath, QByteArrayLiteral("{\"profileName\":\"invalid\"}")),
            "invalid import fixture should be written"
        )
        || !require(
            !manager.importProfile(invalidImportPath, nullptr, &errorMessage),
            "profile import should require a devices array"
        )
        || !require(
            errorMessage == QStringLiteral("Invalid LumaCore profile: missing devices array."),
            "invalid import should report the missing profile structure"
        )) {
        return 1;
    }

    const QJsonObject unmatchedProfile {
        {QStringLiteral("formatVersion"), 1},
        {QStringLiteral("application"), QStringLiteral("LumaCore")},
        {QStringLiteral("profileName"), QStringLiteral("unmatched")},
        {QStringLiteral("devices"), QJsonArray {
            QJsonObject {
                {QStringLiteral("id"), QStringLiteral("missing-device")},
                {QStringLiteral("zones"), QJsonArray {}},
            },
        }},
    };
    if (!require(
            writeProfileFile(
                QDir(profilesDirectory).filePath(QStringLiteral("unmatched.json")),
                QJsonDocument(unmatchedProfile).toJson(QJsonDocument::Compact)
            ),
            "unmatched profile fixture should be written"
        )
        || !require(!manager.loadProfile(QStringLiteral("unmatched"), &errorMessage), "unmatched profile should be rejected")
        || !require(
            errorMessage.contains(QStringLiteral("did not match any available device zones")),
            "unmatched profile should explain that no zones matched"
        )) {
        return 1;
    }

    return 0;
}
