#include "core/PortablePaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

namespace lumacore {

namespace {

void removeLegacyQtPipelineCaches(const QString& portableRoot)
{
    const QString legacyCachePath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (legacyCachePath.isEmpty()) {
        return;
    }

    const QString legacyCacheDirectory = QDir::cleanPath(QFileInfo(legacyCachePath).absoluteFilePath());
    const QString normalizedPortableRoot = QDir::cleanPath(QFileInfo(portableRoot).absoluteFilePath());
    if (legacyCacheDirectory.startsWith(normalizedPortableRoot, Qt::CaseInsensitive)) {
        return;
    }

    QDir cacheDirectory(legacyCacheDirectory);
    if (!cacheDirectory.exists()) {
        return;
    }

    const QFileInfoList pipelineCaches = cacheDirectory.entryInfoList(
        {QStringLiteral("qtpipelinecache-*")},
        QDir::Dirs | QDir::NoDotAndDotDot
    );
    for (const QFileInfo& pipelineCache : pipelineCaches) {
        QDir(pipelineCache.absoluteFilePath()).removeRecursively();
    }

    if (cacheDirectory.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).isEmpty()) {
        cacheDirectory.rmdir(QStringLiteral("."));
    }
}

} // namespace

QString portableDataRoot()
{
    QString baseDirectory = QCoreApplication::applicationDirPath().trimmed();
    if (baseDirectory.isEmpty()) {
        baseDirectory = QDir::currentPath();
    }

    return QDir::cleanPath(QDir(baseDirectory).filePath(QStringLiteral("data")));
}

QString portableSettingsDirectory()
{
    return QDir::cleanPath(QDir(portableDataRoot()).filePath(QStringLiteral("settings")));
}

QString portableProfilesDirectory()
{
    return QDir::cleanPath(QDir(portableDataRoot()).filePath(QStringLiteral("profiles")));
}

QString portableCacheDirectory()
{
    return QDir::cleanPath(QDir(portableDataRoot()).filePath(QStringLiteral("cache")));
}

void configurePortableStorage()
{
    const QString dataRoot = portableDataRoot();
    const QString settingsDirectory = portableSettingsDirectory();
    const QString profilesDirectory = portableProfilesDirectory();
    const QString cacheDirectory = portableCacheDirectory();

    QDir().mkpath(settingsDirectory);
    QDir().mkpath(profilesDirectory);
    QDir().mkpath(cacheDirectory);

    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDirectory);
    QSettings::setPath(QSettings::IniFormat, QSettings::SystemScope, settingsDirectory);

    const QString qmlCacheDirectory = QDir(cacheDirectory).filePath(QStringLiteral("qml"));
    QDir().mkpath(qmlCacheDirectory);
    qputenv("QML_DISK_CACHE_PATH", QDir::toNativeSeparators(qmlCacheDirectory).toUtf8());

    const QString pipelineCacheDirectory = QDir(cacheDirectory).filePath(QStringLiteral("qt-pipeline"));
    QDir().mkpath(pipelineCacheDirectory);
    const QString pipelineCacheFile = QDir(pipelineCacheDirectory).filePath(QStringLiteral("lumacore-qtquick-rhi"));
    const QByteArray nativePipelineCacheFile = QDir::toNativeSeparators(pipelineCacheFile).toUtf8();
    qputenv("QSG_RHI_PIPELINE_CACHE_LOAD", nativePipelineCacheFile);
    qputenv("QSG_RHI_PIPELINE_CACHE_SAVE", nativePipelineCacheFile);

    removeLegacyQtPipelineCaches(dataRoot);
}

} // namespace lumacore
