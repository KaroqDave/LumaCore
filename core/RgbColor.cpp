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

RgbColor RgbColor::fromQColor(const QColor& color)
{
    return fromRgb(color.red(), color.green(), color.blue());
}

RgbColor RgbColor::fromRgb(int red, int green, int blue)
{
    return RgbColor(
        static_cast<quint8>(qBound(0, red, 255)),
        static_cast<quint8>(qBound(0, green, 255)),
        static_cast<quint8>(qBound(0, blue, 255))
    );
}

} // namespace lumacore
