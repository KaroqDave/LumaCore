// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/PortablePaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>

#ifdef Q_OS_LINUX
#include <unistd.h>
#endif

namespace lumacore {

namespace {

QString cleanedAbsolutePath(const QString& path)
{
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

QString executableRelativeDataRoot()
{
    QString baseDirectory = QCoreApplication::applicationDirPath().trimmed();
    if (baseDirectory.isEmpty()) {
        baseDirectory = QDir::currentPath();
    }

    return QDir::cleanPath(QDir(baseDirectory).filePath(QStringLiteral("data")));
}

QString standardDataRoot()
{
#ifdef Q_OS_LINUX
    if (::geteuid() == 0) {
        return QStringLiteral("/var/lib/lumacore");
    }
#endif

    QString dataRoot = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (dataRoot.isEmpty()) {
        const QString genericDataRoot = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
        if (!genericDataRoot.isEmpty()) {
            dataRoot = QDir(genericDataRoot).filePath(QStringLiteral("LumaCore"));
        }
    }
    if (dataRoot.isEmpty()) {
        dataRoot = QDir(QDir::homePath()).filePath(QStringLiteral(".local/share/LumaCore"));
    }
    return cleanedAbsolutePath(dataRoot);
}

#ifndef Q_OS_WIN
bool isSystemBinaryDirectory(const QString& directory)
{
    const QString path = cleanedAbsolutePath(directory);
    return path == QStringLiteral("/bin")
        || path == QStringLiteral("/sbin")
        || path == QStringLiteral("/usr/bin")
        || path == QStringLiteral("/usr/sbin")
        || path == QStringLiteral("/usr/local/bin")
        || path == QStringLiteral("/usr/local/sbin");
}
#endif

bool useExecutableRelativeStorage()
{
#ifdef Q_OS_WIN
    return true;
#else
    return !isSystemBinaryDirectory(QCoreApplication::applicationDirPath());
#endif
}

void removeLegacyQtPipelineCaches(const QString& portableRoot)
{
    const QString legacyCachePath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (legacyCachePath.isEmpty()) {
        return;
    }

    const QString legacyCacheDirectory = QDir::cleanPath(QFileInfo(legacyCachePath).absoluteFilePath());
    const QString normalizedPortableRoot = QDir::cleanPath(QFileInfo(portableRoot).absoluteFilePath());
    // Only skip when the cache directory is the portable root itself or a child
    // of it. A plain startsWith() would also match a sibling that merely shares
    // a name prefix (e.g. ".../data" vs ".../database"). QDir::cleanPath()
    // normalizes separators to '/', so the boundary check uses '/'.
    const bool insidePortableRoot =
        legacyCacheDirectory.compare(normalizedPortableRoot, Qt::CaseInsensitive) == 0
        || legacyCacheDirectory.startsWith(normalizedPortableRoot + QLatin1Char('/'), Qt::CaseInsensitive);
    if (insidePortableRoot) {
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
    return useExecutableRelativeStorage() ? executableRelativeDataRoot() : standardDataRoot();
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
