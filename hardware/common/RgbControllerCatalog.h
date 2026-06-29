// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QLatin1String>
#include <QString>
#include <QStringList>
#include <Qt>

// Single source of truth for read-only RGB-controller discovery classification,
// shared by the Linux and Windows discovery providers. The two providers keep
// their own thin platform structs (HID paths differ), but the cataloged research
// identities, the heuristic keyword list, and the per-stage descriptions live
// here so the platforms cannot drift out of sync (e.g. the gated-write stage
// string must match docs/hardware/discovery-catalog.md on every platform).
namespace lumacore::hardware::common {

struct RgbControllerCatalogEntry {
    const char* vendorId;
    const char* productId;
    const char* family;
    const char* stage;
    const char* status;
    const char* summary;
    const char* notes;
    bool writeCapableBackend;
};

inline constexpr RgbControllerCatalogEntry kRgbControllerCatalog[] {
    {
        "0B05",
        "19AF",
        "ASUS Aura USB HID",
        "guarded-write-backend",
        "validated guarded write backend",
        "Exact ASUS Aura HID identity with guarded writes available through the asus-aura-hid backend.",
        "Discovery reports this identity read-only. The auto backend deduplicates it when the verified ASUS backend represents the same VID:PID.",
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

[[nodiscard]] inline const RgbControllerCatalogEntry* rgbControllerCatalogEntry(
    const QString& vendorId,
    const QString& productId
)
{
    for (const RgbControllerCatalogEntry& entry : kRgbControllerCatalog) {
        if (vendorId.compare(QLatin1String(entry.vendorId), Qt::CaseInsensitive) == 0
            && productId.compare(QLatin1String(entry.productId), Qt::CaseInsensitive) == 0) {
            return &entry;
        }
    }

    return nullptr;
}

[[nodiscard]] inline bool matchesRgbControllerKeywords(const QString& searchableText)
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

    for (const QString& needle : kControllerKeywords) {
        if (searchableText.contains(needle, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

} // namespace lumacore::hardware::common
