// SPDX-License-Identifier: GPL-2.0-or-later

#include "hardware/windows/ProbeResult.h"

#include "hardware/common/RgbControllerCatalog.h"

#include <QRegularExpression>
#include <QStringList>

namespace lumacore::hardware::windows {

namespace {

QString searchableText(const ProbeDevice& device)
{
    return QStringLiteral("%1 %2 %3 %4:%5")
        .arg(device.vendor, device.name, device.details, device.vendorId, device.productId);
}

const common::RgbControllerCatalogEntry* catalogEntryFor(const ProbeDevice& device)
{
    return common::rgbControllerCatalogEntry(device.vendorId, device.productId);
}

bool matchesControllerKeywords(const ProbeDevice& device)
{
    return common::matchesRgbControllerKeywords(searchableText(device));
}

} // namespace

QString stableProbeId(const QString& source, const QString& key)
{
    QString id = QStringLiteral("%1-%2").arg(source, key).toLower();
    id.replace(QRegularExpression(QStringLiteral("[^a-z0-9]+")), QStringLiteral("-"));
    id.replace(QRegularExpression(QStringLiteral("^-+|-+$")), QString());
    return id.isEmpty() ? QStringLiteral("windows-discovery-device") : id;
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

DiscoverySupportInfo discoverySupportInfo(const ProbeDevice& device)
{
    if (const common::RgbControllerCatalogEntry* entry = catalogEntryFor(device)) {
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
        QStringLiteral("No RGB controller catalog entry or heuristic match."),
        QStringLiteral("Visible as read-only Windows HID inventory only."),
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

} // namespace lumacore::hardware::windows
