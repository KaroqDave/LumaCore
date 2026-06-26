// SPDX-License-Identifier: GPL-2.0-or-later

#include "app/GuiOptions.h"
#include "daemon/DaemonOptions.h"
#include "ipc/DaemonProtocol.h"

#include <QCoreApplication>
#include <QDebug>
#include <QtGlobal>

namespace {

bool require(bool condition, const char* message)
{
    if (!condition) {
        qCritical().noquote() << message;
    }
    return condition;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);

    QString error;
    const lumacore::GuiOptions guiOptions = lumacore::parseGuiOptionsArguments({
        QStringLiteral("lumacore"),
        QStringLiteral("--socket"),
        QStringLiteral("custom-endpoint"),
        QStringLiteral("--no-auto-start-daemon"),
    }, &error);
    if (!require(error.isEmpty(), "GUI options should parse without errors")
        || !require(guiOptions.daemonSocketPath == QStringLiteral("custom-endpoint"), "GUI socket option should be retained")
        || !require(!guiOptions.autoStartDaemon, "GUI no-auto-start option should disable daemon startup")) {
        return 1;
    }

    const lumacore::GuiOptions defaultGuiOptions =
        lumacore::parseGuiOptionsArguments({QStringLiteral("lumacore")}, &error);
#ifdef Q_OS_WIN
    if (!require(defaultGuiOptions.autoStartDaemon, "Windows GUI should auto-start the daemon by default")) {
        return 1;
    }
#else
    if (!require(!defaultGuiOptions.autoStartDaemon, "non-Windows GUI should preserve manual daemon startup")) {
        return 1;
    }
#endif

    const lumacore::DaemonOptions daemonOptions = lumacore::parseDaemonOptionsArguments({
        QStringLiteral("lumacore-daemon"),
        QStringLiteral("--socket"),
        QStringLiteral("custom-endpoint"),
        QStringLiteral("--backend"),
        QStringLiteral("mock"),
        QStringLiteral("--allow-unprivileged"),
        QStringLiteral("--exit-on-disconnect"),
    }, &error);
    if (!require(error.isEmpty(), "daemon options should parse without errors")
        || !require(daemonOptions.socketPath == QStringLiteral("custom-endpoint"), "daemon socket option should be retained")
        || !require(daemonOptions.backendId == QStringLiteral("mock"), "daemon backend option should be retained")
        || !require(daemonOptions.allowUnprivileged, "daemon development privilege option should be retained")
        || !require(daemonOptions.exitOnDisconnect, "daemon idle exit option should be retained")) {
        return 1;
    }

#ifdef Q_OS_WIN
    const QString defaultWindowsSocketPath = lumacore::defaultDaemonSocketPath();
    if (!require(
            defaultWindowsSocketPath.startsWith(QStringLiteral("lumacore-daemon-v1-"))
                && defaultWindowsSocketPath.size() > QStringLiteral("lumacore-daemon-v1-").size(),
            "Windows should use a versioned, user-scoped local server name"
        )) {
        return 1;
    }
#else
    if (!require(
            lumacore::defaultDaemonSocketPath() == QStringLiteral("/run/lumacore/lumacore.sock"),
            "Linux should retain the packaged Unix socket path"
        )) {
        return 1;
    }
#endif

    return 0;
}
