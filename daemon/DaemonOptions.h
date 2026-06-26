// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QString>
#include <QStringList>

class QCoreApplication;

namespace lumacore {

struct DaemonOptions {
    QString socketPath;
    QString backendId;
    bool allowUnprivileged {false};
    bool exitOnDisconnect {false};
};

[[nodiscard]] DaemonOptions parseDaemonOptions(QCoreApplication& application);
[[nodiscard]] DaemonOptions parseDaemonOptionsArguments(const QStringList& arguments, QString* errorMessage = nullptr);

} // namespace lumacore
