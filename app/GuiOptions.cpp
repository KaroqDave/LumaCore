// SPDX-License-Identifier: GPL-2.0-or-later

#include "app/GuiOptions.h"

#include "ipc/DaemonProtocol.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>

namespace lumacore {

namespace {

void configureParser(QCommandLineParser& parser)
{
    parser.setApplicationDescription(QStringLiteral("LumaCore RGB controller GUI."));
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption socketOption(
        QStringList {QStringLiteral("s"), QStringLiteral("socket")},
        QStringLiteral("LumaCore daemon local endpoint name or path."),
        QStringLiteral("endpoint"),
        defaultDaemonSocketPath()
    );
    const QCommandLineOption noAutoStartDaemonOption(
        QStringLiteral("no-auto-start-daemon"),
        QStringLiteral("Do not automatically start the bundled daemon when it is unavailable.")
    );
    const QCommandLineOption mockScenarioOption(
        QStringLiteral("mock-scenario"),
        QStringLiteral("Mock device scenario to pass to an auto-started daemon."),
        QStringLiteral("id")
    );
    const QCommandLineOption selfTestOption(
        QStringLiteral("self-test"),
        QStringLiteral("Load the QML interface headlessly, report any load or binding errors, and exit.")
    );
    parser.addOption(socketOption);
    parser.addOption(noAutoStartDaemonOption);
    parser.addOption(mockScenarioOption);
    parser.addOption(selfTestOption);
}

GuiOptions optionsFromParser(const QCommandLineParser& parser)
{
    return {
        .daemonSocketPath = parser.value(QStringLiteral("socket")),
        .mockScenarioId = parser.value(QStringLiteral("mock-scenario")).trimmed(),
#ifdef Q_OS_WIN
        .autoStartDaemon = !parser.isSet(QStringLiteral("no-auto-start-daemon")),
#else
        .autoStartDaemon = false,
#endif
        .selfTest = parser.isSet(QStringLiteral("self-test")),
    };
}

} // namespace

GuiOptions parseGuiOptions(QCoreApplication& application)
{
    QCommandLineParser parser;
    configureParser(parser);
    parser.process(application);
    return optionsFromParser(parser);
}

GuiOptions parseGuiOptionsArguments(const QStringList& arguments, QString* errorMessage)
{
    QCommandLineParser parser;
    configureParser(parser);
    if (!parser.parse(arguments) && errorMessage != nullptr) {
        *errorMessage = parser.errorText();
    }
    return optionsFromParser(parser);
}

} // namespace lumacore
