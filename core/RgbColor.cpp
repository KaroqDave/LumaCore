// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/RgbColor.h"

#include <QChar>

namespace lumacore {

QColor RgbColor::toQColor() const
{
    return QColor(red(), green(), blue());
}

QString RgbColor::toHexString() const
{
    return QStringLiteral("#%1%2%3")
        .arg(static_cast<int>(red()), 2, 16, QChar('0'))
        .arg(static_cast<int>(green()), 2, 16, QChar('0'))
        .arg(static_cast<int>(blue()), 2, 16, QChar('0'))
        .toUpper();
}

QJsonObject RgbColor::toJson() const
{
    return {
        {QStringLiteral("red"), red()},
        {QStringLiteral("green"), green()},
        {QStringLiteral("blue"), blue()},
        {QStringLiteral("hex"), toHexString()},
    };
}

RgbColor RgbColor::fromQColor(const QColor& color)
{
    return fromRgb(color.red(), color.green(), color.blue());
}

RgbColor RgbColor::fromHexString(const QString& value, bool* ok)
{
    const QColor color(value);
    const bool isValid = color.isValid();
    if (ok != nullptr) {
        *ok = isValid;
    }

    return isValid ? fromQColor(color) : RgbColor();
}

RgbColor RgbColor::fromRgb(int red, int green, int blue)
{
    return RgbColor(
        static_cast<quint8>(qBound(0, red, 255)),
        static_cast<quint8>(qBound(0, green, 255)),
        static_cast<quint8>(qBound(0, blue, 255))
    );
}

RgbColor RgbColor::fromJson(const QJsonObject& object)
{
    // Mirror toJson()'s "red"/"green"/"blue" keys so serialize/deserialize
    // round-trip; fall back to a hex string when the numeric keys are absent.
    if (object.contains(QStringLiteral("red"))
        || object.contains(QStringLiteral("green"))
        || object.contains(QStringLiteral("blue"))) {
        return fromRgb(
            object.value(QStringLiteral("red")).toInt(),
            object.value(QStringLiteral("green")).toInt(),
            object.value(QStringLiteral("blue")).toInt()
        );
    }
    return fromHexString(object.value(QStringLiteral("hex")).toString());
}

} // namespace lumacore
