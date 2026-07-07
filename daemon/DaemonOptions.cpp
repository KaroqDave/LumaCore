// SPDX-License-Identifier: GPL-2.0-or-later

#include "daemon/DaemonOptions.h"

#include "ipc/DaemonProtocol.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>

#include <cstdio>
#include <cstdlib>
#include <optional>

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
        QStringLiteral("Backend id to activate. Use 'auto' to prefer ASUS hardware, then platform discovery, then mock."),
        QStringLiteral("id"),
        QStringLiteral("auto")
    );
    const QCommandLineOption exitOnDisconnectOption(
        QStringLiteral("exit-on-disconnect"),
        QStringLiteral("Exit after the last client disconnects, once at least one client has connected.")
    );
    const QCommandLineOption mockScenarioOption(
        QStringLiteral("mock-scenario"),
        QStringLiteral("Mock device scenario to expose when the mock backend or auto fallback is active."),
        QStringLiteral("id"),
        QStringLiteral("asus-board")
    );
    const QCommandLineOption dryRunOption(
        QStringLiteral("dry-run"),
        QStringLiteral("Override the startup dry-run state ('true' logs write intent only, 'false' arms writes). Defaults to the platform default."),
        QStringLiteral("enabled")
    );

    parser.addOption(socketOption);
    parser.addOption(allowUnprivilegedOption);
    parser.addOption(backendOption);
    parser.addOption(exitOnDisconnectOption);
    parser.addOption(mockScenarioOption);
    parser.addOption(dryRunOption);
}

std::optional<bool> parseDryRunValue(const QString& value)
{
    const QString normalized = value.trimmed().toLower();
    if (normalized == QStringLiteral("true") || normalized == QStringLiteral("1")
        || normalized == QStringLiteral("on") || normalized == QStringLiteral("yes")) {
        return true;
    }
    if (normalized == QStringLiteral("false") || normalized == QStringLiteral("0")
        || normalized == QStringLiteral("off") || normalized == QStringLiteral("no")) {
        return false;
    }
    return std::nullopt;
}

DaemonOptions optionsFromParser(const QCommandLineParser& parser, QString* errorMessage)
{
    std::optional<bool> dryRunEnabled;
    if (parser.isSet(QStringLiteral("dry-run"))) {
        dryRunEnabled = parseDryRunValue(parser.value(QStringLiteral("dry-run")));
        if (!dryRunEnabled.has_value() && errorMessage != nullptr) {
            *errorMessage = QStringLiteral(
                "Invalid --dry-run value '%1'. Use true or false."
            ).arg(parser.value(QStringLiteral("dry-run")));
        }
    }

    return {
        .socketPath = parser.value(QStringLiteral("socket")),
        .backendId = parser.value(QStringLiteral("backend")).trimmed(),
        .mockScenarioId = parser.value(QStringLiteral("mock-scenario")).trimmed(),
        .allowUnprivileged = parser.isSet(QStringLiteral("allow-unprivileged")),
        .exitOnDisconnect = parser.isSet(QStringLiteral("exit-on-disconnect")),
        .dryRunEnabled = dryRunEnabled,
    };
}

[[noreturn]] void showParserErrorAndExit(const QCommandLineParser& parser, const QString& errorMessage)
{
    std::fputs(qPrintable(errorMessage), stderr);
    std::fputs("\n\n", stderr);
    std::fputs(qPrintable(parser.helpText()), stderr);
    std::exit(EXIT_FAILURE);
}

} // namespace

DaemonOptions parseDaemonOptions(QCoreApplication& application)
{
    QCommandLineParser parser;
    configureParser(parser);
    parser.process(application);

    QString errorMessage;
    DaemonOptions options = optionsFromParser(parser, &errorMessage);
    if (!errorMessage.isEmpty()) {
        showParserErrorAndExit(parser, errorMessage);
    }
    return options;
}

DaemonOptions parseDaemonOptionsArguments(const QStringList& arguments, QString* errorMessage)
{
    if (errorMessage != nullptr) {
        errorMessage->clear();
    }
    QCommandLineParser parser;
    configureParser(parser);
    if (!parser.parse(arguments) && errorMessage != nullptr) {
        *errorMessage = parser.errorText();
    }
    return optionsFromParser(parser, errorMessage);
}

} // namespace lumacore
