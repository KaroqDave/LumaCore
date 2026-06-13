#include "core/ActivityLog.h"

namespace lumacore {

namespace {

constexpr int kMaxStoredLines = 500;

} // namespace

QString logLevelToString(LogLevel level)
{
    switch (level) {
    case LogLevel::Debug:
        return QStringLiteral("DBG");
    case LogLevel::Info:
        return QStringLiteral("INF");
    case LogLevel::Warning:
        return QStringLiteral("WRN");
    case LogLevel::Error:
        return QStringLiteral("ERR");
    }

    return QStringLiteral("INF");
}

QString logCategoryToString(LogCategory category)
{
    switch (category) {
    case LogCategory::System:
        return QStringLiteral("System");
    case LogCategory::Device:
        return QStringLiteral("Device");
    case LogCategory::Zone:
        return QStringLiteral("Zone");
    case LogCategory::Effect:
        return QStringLiteral("Effect");
    case LogCategory::Profile:
        return QStringLiteral("Profile");
    case LogCategory::Permission:
        return QStringLiteral("Permission");
    case LogCategory::Backend:
        return QStringLiteral("Backend");
    }

    return QStringLiteral("System");
}

QString LogEntry::formatted() const
{
    return QStringLiteral("[%1] %2 [%3] %4")
        .arg(timestamp.toString(QStringLiteral("HH:mm:ss")),
             logLevelToString(level),
             logCategoryToString(category),
             message);
}

ActivityLog::ActivityLog(QObject* parent)
    : QObject(parent)
    , m_maxLineCount(kMaxStoredLines)
{
}

void ActivityLog::log(LogLevel level, LogCategory category, const QString& message)
{
    if (message.trimmed().isEmpty()) {
        return;
    }

    LogEntry entry {
        QDateTime::currentDateTime(),
        level,
        category,
        message.trimmed(),
    };

    m_lines.append(entry.formatted());
    while (m_lines.size() > m_maxLineCount) {
        m_lines.removeFirst();
    }

    mirrorToConsole(entry);
    emit entryAdded(entry);
}

void ActivityLog::debug(LogCategory category, const QString& message)
{
    log(LogLevel::Debug, category, message);
}

void ActivityLog::info(LogCategory category, const QString& message)
{
    log(LogLevel::Info, category, message);
}

void ActivityLog::warning(LogCategory category, const QString& message)
{
    log(LogLevel::Warning, category, message);
}

void ActivityLog::error(LogCategory category, const QString& message)
{
    log(LogLevel::Error, category, message);
}

QStringList ActivityLog::formattedLines() const
{
    return m_lines;
}

void ActivityLog::mirrorToConsole(const LogEntry& entry) const
{
    const QString formatted = entry.formatted();

    switch (entry.level) {
    case LogLevel::Debug:
        qDebug().noquote() << formatted;
        break;
    case LogLevel::Info:
        qInfo().noquote() << formatted;
        break;
    case LogLevel::Warning:
        qWarning().noquote() << formatted;
        break;
    case LogLevel::Error:
        qCritical().noquote() << formatted;
        break;
    }
}

} // namespace lumacore
