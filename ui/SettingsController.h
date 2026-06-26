// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>
#include <QSettings>
#include <QString>

namespace lumacore {

class SettingsController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool animationsEnabled READ animationsEnabled WRITE setAnimationsEnabled NOTIFY animationsEnabledChanged)
    Q_PROPERTY(bool reduceVrrFlicker READ reduceVrrFlicker WRITE setReduceVrrFlicker NOTIFY reduceVrrFlickerChanged)
    Q_PROPERTY(bool startMinimized READ startMinimized WRITE setStartMinimized NOTIFY startMinimizedChanged)
    Q_PROPERTY(bool closeToTray READ closeToTray WRITE setCloseToTray NOTIFY closeToTrayChanged)
    Q_PROPERTY(bool trayAvailable READ trayAvailable NOTIFY trayAvailableChanged)
    Q_PROPERTY(bool applyOnLaunch READ applyOnLaunch WRITE setApplyOnLaunch NOTIFY applyOnLaunchChanged)
    Q_PROPERTY(bool scheduledProfileEnabled READ scheduledProfileEnabled WRITE setScheduledProfileEnabled NOTIFY scheduledProfileEnabledChanged)
    Q_PROPERTY(QString scheduledProfile READ scheduledProfile WRITE setScheduledProfile NOTIFY scheduledProfileChanged)
    Q_PROPERTY(QString scheduledProfileTime READ scheduledProfileTime WRITE setScheduledProfileTime NOTIFY scheduledProfileTimeChanged)
    Q_PROPERTY(bool dryRunEnabled READ dryRunEnabled WRITE setDryRunEnabled NOTIFY dryRunEnabledChanged)
    Q_PROPERTY(QString theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(QString activeProfile READ activeProfile WRITE setActiveProfile NOTIFY activeProfileChanged)

public:
    explicit SettingsController(QObject* parent = nullptr);

    [[nodiscard]] bool animationsEnabled() const;
    void setAnimationsEnabled(bool enabled);

    [[nodiscard]] bool reduceVrrFlicker() const;
    void setReduceVrrFlicker(bool enabled);

    [[nodiscard]] bool startMinimized() const;
    void setStartMinimized(bool enabled);

    [[nodiscard]] bool closeToTray() const;
    void setCloseToTray(bool enabled);
    [[nodiscard]] bool trayAvailable() const;
    void setTrayAvailable(bool available);

    [[nodiscard]] bool applyOnLaunch() const;
    void setApplyOnLaunch(bool enabled);

    [[nodiscard]] bool scheduledProfileEnabled() const;
    void setScheduledProfileEnabled(bool enabled);
    [[nodiscard]] QString scheduledProfile() const;
    void setScheduledProfile(const QString& profileName);
    [[nodiscard]] QString scheduledProfileTime() const;
    void setScheduledProfileTime(const QString& time);

    [[nodiscard]] bool dryRunEnabled() const;
    void setDryRunEnabled(bool enabled);

    [[nodiscard]] QString theme() const;
    void setTheme(const QString& theme);

    [[nodiscard]] QString activeProfile() const;
    void setActiveProfile(const QString& profileName);

signals:
    void animationsEnabledChanged();
    void reduceVrrFlickerChanged();
    void startMinimizedChanged();
    void closeToTrayChanged();
    void trayAvailableChanged();
    void applyOnLaunchChanged();
    void scheduledProfileEnabledChanged();
    void scheduledProfileChanged();
    void scheduledProfileTimeChanged();
    void dryRunEnabledChanged();
    void themeChanged();
    void activeProfileChanged();

private:
    void load();

    QSettings m_settings;
    bool m_animationsEnabled {true};
    bool m_reduceVrrFlicker {true};
    bool m_startMinimized {false};
    bool m_closeToTray {false};
    bool m_trayAvailable {false};
    bool m_applyOnLaunch {false};
    bool m_scheduledProfileEnabled {false};
    bool m_dryRunEnabled {false};
    QString m_theme {QStringLiteral("Dark")};
    QString m_activeProfile;
    QString m_scheduledProfile;
    QString m_scheduledProfileTime {QStringLiteral("18:00")};
};

} // namespace lumacore
