#include "hardware/linux/ProbeResult.h"

#include <QRegularExpression>
#include <QStringList>

namespace lumacore::hardware::linux {

namespace {

struct DiscoveryCatalogEntry {
    const char* vendorId;
    const char* productId;
    const char* family;
    const char* stage;
    const char* status;
    const char* summary;
    const char* notes;
    bool writeCapableBackend;
};

constexpr DiscoveryCatalogEntry kDiscoveryCatalog[] {
    {
        "0B05",
        "19AF",
        "ASUS Aura USB HID",
        "guarded-write-backend",
        "validated guarded write backend",
        "Exact ASUS Aura HID identity with guarded writes available through the asus-aura-hid backend.",
        "linux-discovery reports this identity read-only. The auto backend deduplicates it when the verified ASUS backend represents the same VID:PID.",
        true,
    },
    {
        "0B05",
        "18F3",
        "ASUS Aura USB HID",
        "research-only",
        "read-only research candidate",
        "ASUS Aura USB identity recorded for inventory and future research only.",
        "Do not enable writes without owned-hardware captures, config-table tests, and explicit packet verification.",
        false,
    },
    {
        "0B05",
        "1939",
        "ASUS Aura USB HID",
        "research-only",
        "read-only research candidate",
        "ASUS Aura USB identity recorded for inventory and future research only.",
        "Do not enable writes without owned-hardware captures, config-table tests, and explicit packet verification.",
        false,
    },
};

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

const DiscoveryCatalogEntry* catalogEntryFor(const ProbeDevice& device)
{
    for (const DiscoveryCatalogEntry& entry : kDiscoveryCatalog) {
        if (device.vendorId.compare(QLatin1String(entry.vendorId), Qt::CaseInsensitive) == 0
            && device.productId.compare(QLatin1String(entry.productId), Qt::CaseInsensitive) == 0) {
            return &entry;
        }
    }

    return nullptr;
}

bool matchesControllerKeywords(const ProbeDevice& device)
{
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
    return containsAny(searchableText(device), kControllerKeywords);
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

DiscoverySupportInfo discoverySupportInfo(const ProbeDevice& device)
{
    if (const DiscoveryCatalogEntry* entry = catalogEntryFor(device)) {
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
        QStringLiteral("Visible as read-only inventory only."),
        false,
        false,
        false,
    };
}

bool isCatalogedRgbController(const ProbeDevice& device)
{
    // Cheap catalog lookup instead of building a full DiscoverySupportInfo
    // (which allocates five QStrings) only to read a single bool.
    return catalogEntryFor(device) != nullptr;
}

bool isKnownRgbController(const ProbeDevice& device)
{
    return isCatalogedRgbController(device);
}

bool isLikelyRgbController(const ProbeDevice& device)
{
    return catalogEntryFor(device) != nullptr || matchesControllerKeywords(device);
}

} // namespace lumacore::hardware::linux
