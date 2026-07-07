// SPDX-License-Identifier: GPL-2.0-or-later

#include <QCoreApplication>
#include <QStringList>
#include <QThread>

#ifdef LUMACORE_FIXTURE_REQUIRE_MANY_ZONES
namespace {

QString optionValue(const QStringList& arguments, const QString& option)
{
    const qsizetype optionIndex = arguments.indexOf(option);
    if (optionIndex < 0 || optionIndex + 1 >= arguments.size()) {
        return {};
    }
    return arguments.at(optionIndex + 1);
}

} // namespace
#endif

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
#ifdef LUMACORE_FIXTURE_REQUIRE_MANY_ZONES
    const QStringList arguments = application.arguments();
    return optionValue(arguments, QStringLiteral("--backend")) == QStringLiteral("auto")
            && !optionValue(arguments, QStringLiteral("--socket")).isEmpty()
            && arguments.contains(QStringLiteral("--exit-on-disconnect"))
            && optionValue(arguments, QStringLiteral("--mock-scenario")) == QStringLiteral("many-zones")
        ? 42
        : 24;
#endif
#ifdef LUMACORE_FIXTURE_SLEEP
    QThread::msleep(5000);
    return 0;
#else
    return 23;
#endif
}
