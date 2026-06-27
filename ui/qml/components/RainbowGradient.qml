// SPDX-License-Identifier: GPL-2.0-or-later

import QtQuick

// The canonical full-spectrum rainbow used by zone swatches that represent an
// animated rainbow or color-cycle effect. Shared so the device tree and the
// workspace swatch render the same gradient and never drift apart.
Gradient {
    orientation: Gradient.Horizontal
    GradientStop { position: 0.0; color: "#FF0000" }
    GradientStop { position: 0.2; color: "#FFFF00" }
    GradientStop { position: 0.4; color: "#00FF00" }
    GradientStop { position: 0.6; color: "#00FFFF" }
    GradientStop { position: 0.8; color: "#0000FF" }
    GradientStop { position: 1.0; color: "#FF00FF" }
}
