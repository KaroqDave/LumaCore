// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QString>
#include <QTime>

namespace lumacore {

[[nodiscard]] QString normalizeScheduleTime(const QString& value);
[[nodiscard]] QTime parseScheduleTime(const QString& value);

} // namespace lumacore
