// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QStringList>

namespace lumacore {

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error,
};

enum class LogCategory {
    System,
    Device,
    Zone,
    Effect,
    Profile,
    Permission,
    Backend,
};

struct LogEntry {
    QDateTime timestamp;
    LogLevel level {LogLevel::Info};
    LogCategory category {LogCategory::System};
    QString message;

    [[nodiscard]] QString formatted() const;
};

[[nodiscard]] QString logLevelToString(LogLevel level);
[[nodiscard]] QString logCategoryToString(LogCategory category);

class ActivityLog final : public QObject
{
    Q_OBJECT

public:
    explicit ActivityLog(QObject* parent = nullptr);

    void log(LogLevel level, LogCategory category, const QString& message);
    void debug(LogCategory category, const QString& message);
    void info(LogCategory category, const QString& message);
    void warning(LogCategory category, const QString& message);
    void error(LogCategory category, const QString& message);

    [[nodiscard]] QStringList formattedLines() const;
    [[nodiscard]] int maxLineCount() const { return m_maxLineCount; }

signals:
    void entryAdded(const LogEntry& entry);

private:
    void mirrorToConsole(const LogEntry& entry) const;

    QStringList m_lines;
    int m_maxLineCount {500};
};

} // namespace lumacore
