// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QString>
#include <QStringList>

class QCoreApplication;

namespace lumacore {

struct GuiOptions {
    QString daemonSocketPath;
    QString mockScenarioId;
    bool autoStartDaemon {false};
    // When set, the GUI loads the QML interface once headlessly and exits with a
    // status code instead of entering the event loop. Used by the qml_smoke test
    // to catch QML load and binding regressions that qmllint cannot see.
    bool selfTest {false};
};

[[nodiscard]] GuiOptions parseGuiOptions(QCoreApplication& application);
[[nodiscard]] GuiOptions parseGuiOptionsArguments(const QStringList& arguments, QString* errorMessage = nullptr);

} // namespace lumacore
