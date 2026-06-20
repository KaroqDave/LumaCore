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

    lumacore::DeviceManager manager;
    manager.registerBackend(std::make_unique<lumacore::MockBackend>());
    manager.initializeBackends(QStringLiteral("mock"));
    if (!require(manager.deviceCount() == 1, "mock backend should load one device")) {
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
        || !require(QFile::exists(QStringLiteral("profiles/My_Profile.json")), "sanitized profile file should exist")) {
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

    if (!require(
            manager.renameProfile(QStringLiteral("My_Profile"), QStringLiteral("Renamed Profile"), &errorMessage),
            "profile should rename"
        )
        || !require(manager.profileNames() == QStringList {QStringLiteral("Renamed_Profile")}, "renamed profile should be listed")
        || !require(manager.deleteProfile(QStringLiteral("Renamed_Profile"), &errorMessage), "profile should delete")
        || !require(manager.profileNames().isEmpty(), "deleted profile should no longer be listed")) {
        return 1;
    }

    QDir().mkpath(QStringLiteral("profiles"));
    if (!require(
            writeProfileFile(QStringLiteral("profiles/malformed.json"), QByteArrayLiteral("{not-json")),
            "malformed profile fixture should be written"
        )
        || !require(!manager.loadProfile(QStringLiteral("malformed"), &errorMessage), "malformed JSON should be rejected")
        || !require(errorMessage.contains(QStringLiteral("Invalid profile JSON")), "malformed JSON should report a parse error")) {
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
                QStringLiteral("profiles/unmatched.json"),
                QJsonDocument(unmatchedProfile).toJson(QJsonDocument::Compact)
            ),
            "unmatched profile fixture should be written"
        )
        || !require(!manager.loadProfile(QStringLiteral("unmatched"), &errorMessage), "unmatched profile should be rejected")
        || !require(
            errorMessage.contains(QStringLiteral("did not match any available mock zones")),
            "unmatched profile should explain that no zones matched"
        )) {
        return 1;
    }

    return 0;
}
