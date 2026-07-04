// SPDX-License-Identifier: GPL-2.0-or-later

#include "app/GuiApplication.h"
#include "app/GuiOptions.h"
#include "app/Version.h"

#include <QApplication>
#include <QQuickStyle>

#include <cstdio>
#include <cstring>

int main(int argc, char* argv[])
{
    for (int index = 1; index < argc; ++index) {
        if (std::strcmp(argv[index], "--version") == 0 || std::strcmp(argv[index], "-v") == 0) {
            std::fprintf(stdout, "LumaCore %s\n", qPrintable(lumacore::applicationVersion()));
            return 0;
        }
    }

    if (!lumacore::GuiApplication::validateEnvironment()) {
        return 1;
    }

    QApplication qtApplication(argc, argv);
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    lumacore::GuiApplication::configureQtApplication(qtApplication);
    const lumacore::GuiOptions options = lumacore::parseGuiOptions(qtApplication);
    lumacore::GuiApplication application(qtApplication, options);
    if (options.selfTest) {
        return application.runSelfTest();
    }
    return application.run();
}
