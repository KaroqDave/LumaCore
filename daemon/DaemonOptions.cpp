// SPDX-License-Identifier: GPL-2.0-or-later

#include "daemon/DaemonOptions.h"

#include "ipc/DaemonProtocol.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>

namespace lumacore {

namespace {

void configureParser(QCommandLineParser& parser)
{
    parser.setApplicationDescription(QStringLiteral("Privileged LumaCore RGB backend daemon."));
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption socketOption(
        QStringList {QStringLiteral("s"), QStringLiteral("socket")},
        QStringLiteral("Local daemon endpoint name or path."),
        QStringLiteral("endpoint"),
        defaultDaemonSocketPath()
    );
    const QCommandLineOption allowUnprivilegedOption(
        QStringLiteral("allow-unprivileged"),
        QStringLiteral("Allow running the daemon without root privileges for development and tests.")
    );
    const QCommandLineOption backendOption(
        QStringList {QStringLiteral("b"), QStringLiteral("backend")},
        QStringLiteral("Backend id to activate. Use 'auto' to prefer ASUS hardware, then Linux discovery, then mock."),
        QStringLiteral("id"),
        QStringLiteral("auto")
    );
    const QCommandLineOption exitOnDisconnectOption(
        QStringLiteral("exit-on-disconnect"),
        QStringLiteral("Exit after the last client disconnects, once at least one client has connected.")
    );
    const QCommandLineOption experimentalWritesOption(
        QStringLiteral("enable-experimental-writes"),
        QStringLiteral("Deprecated compatibility flag; ASUS Aura HID writes are now enabled by config verification and confirmation.")
    );

    parser.addOption(socketOption);
    parser.addOption(allowUnprivilegedOption);
    parser.addOption(backendOption);
    parser.addOption(exitOnDisconnectOption);
    parser.addOption(experimentalWritesOption);
}

DaemonOptions optionsFromParser(const QCommandLineParser& parser)
{
    return {
        .socketPath = parser.value(QStringLiteral("socket")),
        .backendId = parser.value(QStringLiteral("backend")).trimmed(),
        .allowUnprivileged = parser.isSet(QStringLiteral("allow-unprivileged")),
        .exitOnDisconnect = parser.isSet(QStringLiteral("exit-on-disconnect")),
    };
}

} // namespace

DaemonOptions parseDaemonOptions(QCoreApplication& application)
{
    QCommandLineParser parser;
    configureParser(parser);
    parser.process(application);

    if (parser.isSet(QStringLiteral("enable-experimental-writes"))) {
        qWarning().noquote()
            << QStringLiteral("--enable-experimental-writes is deprecated and ignored; ASUS writes remain config-verified and confirmation-gated.");
    }

    return optionsFromParser(parser);
}

DaemonOptions parseDaemonOptionsArguments(const QStringList& arguments, QString* errorMessage)
{
    QCommandLineParser parser;
    configureParser(parser);
    if (!parser.parse(arguments) && errorMessage != nullptr) {
        *errorMessage = parser.errorText();
    }
    return optionsFromParser(parser);
}

} // namespace lumacore
