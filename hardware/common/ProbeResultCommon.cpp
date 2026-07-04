// SPDX-License-Identifier: GPL-2.0-or-later

#include "hardware/common/ProbeResultCommon.h"

#include "hardware/common/RgbControllerCatalog.h"

#include <QRegularExpression>

namespace lumacore::hardware::common {

namespace {

QString searchableText(const ProbeDevice& device)
{
    return QStringLiteral("%1 %2 %3 %4:%5")
        .arg(device.vendor, device.name, device.details, device.vendorId, device.productId);
}

const RgbControllerCatalogEntry* catalogEntryFor(const ProbeDevice& device)
{
    return rgbControllerCatalogEntry(device.vendorId, device.productId);
}

bool matchesControllerKeywords(const ProbeDevice& device)
{
    return matchesRgbControllerKeywords(searchableText(device));
}

} // namespace

QString stableProbeId(const QString& source, const QString& key, const QString& fallbackId)
{
    QString id = QStringLiteral("%1-%2").arg(source, key).toLower();
    id.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    id.replace(QRegularExpression(QStringLiteral("^-+|-+$")), QString());
    return id.isEmpty() ? fallbackId : id;
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

DiscoverySupportInfo discoverySupportInfo(const ProbeDevice& device, const QString& unclassifiedSummary)
{
    if (const RgbControllerCatalogEntry* entry = catalogEntryFor(device)) {
        return {
            QString::fromLatin1(entry->stage),
            QString::fromLatin1(entry->status),
            QString::fromLatin1(entry->family),
            QString::fromLatin1(entry->summary),
            QString::fromLatin1(entry->notes),
            true,
            true,
            entry->writeCapableBackend,
        };
    }

    if (matchesControllerKeywords(device)) {
        return {
            QStringLiteral("heuristic"),
            QStringLiteral("heuristic RGB candidate"),
            QStringLiteral("Uncataloged RGB-like device"),
            QStringLiteral("Device text matches RGB controller keywords; inventory remains read-only until researched."),
            QStringLiteral("Add a catalog entry with stable identifiers before implementing dry-run previews or writes."),
            false,
            true,
            false,
        };
    }

    return {
        QStringLiteral("unclassified"),
        QStringLiteral("unclassified device"),
        QString(),
        unclassifiedSummary,
        QStringLiteral("Visible as read-only inventory only."),
        false,
        false,
        false,
    };
}

bool isCatalogedRgbController(const ProbeDevice& device)
{
    return catalogEntryFor(device) != nullptr;
}

bool isLikelyRgbController(const ProbeDevice& device)
{
    return catalogEntryFor(device) != nullptr || matchesControllerKeywords(device);
}

} // namespace lumacore::hardware::common
