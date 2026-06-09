#pragma once

#include <QColor>
#include <QString>
#include <QtGlobal>

namespace lumacore {

class RgbColor
{
public:
    constexpr RgbColor() = default;
    constexpr RgbColor(quint8 red, quint8 green, quint8 blue)
        : m_red(red)
        , m_green(green)
        , m_blue(blue)
    {
    }

    [[nodiscard]] constexpr quint8 red() const { return m_red; }
    [[nodiscard]] constexpr quint8 green() const { return m_green; }
    [[nodiscard]] constexpr quint8 blue() const { return m_blue; }

    [[nodiscard]] QColor toQColor() const;
    [[nodiscard]] QString toHexString() const;

    [[nodiscard]] static RgbColor fromQColor(const QColor& color);
    [[nodiscard]] static RgbColor fromRgb(int red, int green, int blue);

    [[nodiscard]] friend constexpr bool operator==(const RgbColor&, const RgbColor&) = default;

private:
    quint8 m_red {0};
    quint8 m_green {0};
    quint8 m_blue {0};
};

} // namespace lumacore
