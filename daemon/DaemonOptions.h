// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QString>
#include <QStringList>

#include <optional>

class QCoreApplication;

namespace lumacore {

struct DaemonOptions {
    QString socketPath;
    QString backendId;
    bool allowUnprivileged {false};
    bool exitOnDisconnect {false};
    // When set, overrides the platform default dry-run state at startup. The GUI
    // passes its persisted preference here so a GUI-launched daemon starts in the
    // correct state instead of relying on a post-connect sync round-trip.
    std::optional<bool> dryRunEnabled;
};

[[nodiscard]] DaemonOptions parseDaemonOptions(QCoreApplication& application);
[[nodiscard]] DaemonOptions parseDaemonOptionsArguments(const QStringList& arguments, QString* errorMessage = nullptr);

} // namespace lumacore
