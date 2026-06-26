// SPDX-License-Identifier: GPL-2.0-or-later

#include <QCoreApplication>
#include <QThread>

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
#ifdef LUMACORE_FIXTURE_SLEEP
    QThread::msleep(5000);
    return 0;
#else
    return 23;
#endif
}
