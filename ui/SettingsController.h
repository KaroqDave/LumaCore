#pragma once

#include <QObject>
#include <QSettings>
#include <QString>

namespace lumacore {

class SettingsController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool animationsEnabled READ animationsEnabled WRITE setAnimationsEnabled NOTIFY animationsEnabledChanged)
    Q_PROPERTY(bool startMinimized READ startMinimized WRITE setStartMinimized NOTIFY startMinimizedChanged)
    Q_PROPERTY(bool applyOnLaunch READ applyOnLaunch WRITE setApplyOnLaunch NOTIFY applyOnLaunchChanged)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)

public:
    explicit SettingsController(QObject* parent = nullptr);

    [[nodiscard]] bool animationsEnabled() const;
    void setAnimationsEnabled(bool enabled);

    [[nodiscard]] bool startMinimized() const;
    void setStartMinimized(bool enabled);

    [[nodiscard]] bool applyOnLaunch() const;
    void setApplyOnLaunch(bool enabled);

    [[nodiscard]] QString theme() const;
    void setTheme(const QString& theme);

signals:
    void animationsEnabledChanged();
    void startMinimizedChanged();
    void applyOnLaunchChanged();
    void themeChanged();

private:
    void load();

    QSettings m_settings;
    bool m_animationsEnabled {true};
    bool m_startMinimized {false};
    bool m_applyOnLaunch {false};
    QString m_theme {QStringLiteral("Dark")};
};

} // namespace lumacore
