#pragma once

#include <QString>
#include <QStringList>

class QCoreApplication;

namespace lumacore {

struct GuiOptions {
    QString daemonSocketPath;
    bool autoStartDaemon {false};
};

[[nodiscard]] GuiOptions parseGuiOptions(QCoreApplication& application);
[[nodiscard]] GuiOptions parseGuiOptionsArguments(const QStringList& arguments, QString* errorMessage = nullptr);

} // namespace lumacore
