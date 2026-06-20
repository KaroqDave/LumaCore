#include "hardware/linux/ProbeResult.h"

#include <QRegularExpression>
#include <QStringList>

namespace lumacore::hardware::linux {

namespace {

bool containsAny(const QString& haystack, const QStringList& needles)
{
    for (const QString& needle : needles) {
        if (haystack.contains(needle, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

QString searchableText(const ProbeDevice& device)
{
    return QStringLiteral("%1 %2 %3 %4:%5")
        .arg(device.vendor, device.name, device.details, device.vendorId, device.productId);
}

} // namespace

QString stableProbeId(const QString& source, const QString& key)
{
    QString id = QStringLiteral("%1-%2").arg(source, key).toLower();
    id.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    id.replace(QRegularExpression(QStringLiteral("^-+|-+$")), QString());
    return id.isEmpty() ? QStringLiteral("linux-discovery-device") : id;
}

QString hexWord(quint16 value)
{
    return QStringLiteral("%1").arg(static_cast<unsigned int>(value), 4, 16, QLatin1Char('0')).toUpper();
}

QString usbVidPidKey(const ProbeDevice& device)
{
    if (device.vendorId.isEmpty() || device.productId.isEmpty()) {
        return {};
    }

    return QStringLiteral("%1:%2").arg(device.vendorId.toUpper(), device.productId.toUpper());
}

bool isKnownRgbController(const ProbeDevice& device)
{
    return device.vendorId.compare(QStringLiteral("0B05"), Qt::CaseInsensitive) == 0
        && device.productId.compare(QStringLiteral("19AF"), Qt::CaseInsensitive) == 0;
}

bool isLikelyRgbController(const ProbeDevice& device)
{
    if (isKnownRgbController(device)) {
        return true;
    }

    const QString text = searchableText(device);
    static const QStringList kControllerKeywords {
        QStringLiteral("aura"),
        QStringLiteral("rgb controller"),
        QStringLiteral("led controller"),
        QStringLiteral("lighting controller"),
        QStringLiteral("lighting node"),
        QStringLiteral("commander core"),
        QStringLiteral("commander pro"),
        QStringLiteral("rgb hub"),
        QStringLiteral("argb"),
        QStringLiteral("addressable"),
    };
    return containsAny(text, kControllerKeywords);
}

} // namespace lumacore::hardware::linux
