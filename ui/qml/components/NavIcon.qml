import QtQuick

Item {
    id: navIcon

    property string name: ""
    property color color: "#F2F5F8"
    property real strokeWidth: 2
    property bool animationsEnabled: true
    readonly property int animationDuration: animationsEnabled ? 160 : 0

    implicitWidth: 22
    implicitHeight: 22

    Behavior on color {
        ColorAnimation {
            duration: navIcon.animationDuration
            easing.type: Easing.OutCubic
        }
    }

    onColorChanged: canvas.requestPaint()
    onNameChanged: canvas.requestPaint()
    onStrokeWidthChanged: canvas.requestPaint()

    Canvas {
        id: canvas

        anchors.fill: parent
        antialiasing: true

        onPaint: {
            const ctx = getContext("2d")
            ctx.reset()
            ctx.lineWidth = navIcon.strokeWidth
            ctx.strokeStyle = navIcon.color
            ctx.fillStyle = navIcon.color
            ctx.lineCap = "round"
            ctx.lineJoin = "round"

            const w = width
            const h = height

            switch (navIcon.name) {
            case "devices":
                drawDevices(ctx, w, h)
                break
            case "motherboard":
                drawMotherboard(ctx, w, h)
                break
            case "profiles":
                drawProfiles(ctx, w, h)
                break
            case "settings":
                drawSettings(ctx, w, h)
                break
            case "chevron":
                drawChevron(ctx, w, h)
                break
            case "info":
                drawInfo(ctx, w, h)
                break
            case "activity":
                drawActivity(ctx, w, h)
                break
            case "filter":
                drawFilter(ctx, w, h)
                break
            case "check":
                drawCheck(ctx, w, h)
                break
            default:
                break
            }
        }

        function drawMotherboard(ctx, w, h) {
            const m = w * 0.12
            const bw = w - m * 2
            const bh = h - m * 2
            const r = w * 0.12

            roundedRect(ctx, m, m, bw, bh, r)
            ctx.stroke()

            const cx = m + bw / 2
            const cy = m + bh / 2
            const cw = bw * 0.44
            const ch = bh * 0.34
            const chipX = cx - cw / 2
            const chipY = cy - ch / 2

            roundedRect(ctx, chipX, chipY, cw, ch, w * 0.05)
            ctx.stroke()

            const padR = Math.max(1.2, w * 0.055)
            const pads = [
                [m + bw * 0.18, m + bh * 0.22],
                [m + bw * 0.82, m + bh * 0.22],
                [m + bw * 0.18, m + bh * 0.78],
                [m + bw * 0.82, m + bh * 0.78]
            ]

            const traceWidth = Math.max(1, navIcon.strokeWidth * 0.75)
            ctx.lineWidth = traceWidth
            for (let i = 0; i < pads.length; ++i) {
                const px = pads[i][0]
                const py = pads[i][1]
                const targetX = i < 2 ? chipX + cw * 0.25 : chipX + cw * 0.75
                const targetY = i % 2 === 0 ? chipY + ch * 0.2 : chipY + ch * 0.8

                ctx.beginPath()
                ctx.moveTo(px, py)
                ctx.lineTo(targetX, py)
                ctx.lineTo(targetX, targetY)
                ctx.stroke()
            }
            ctx.lineWidth = navIcon.strokeWidth

            for (let i = 0; i < pads.length; ++i) {
                ctx.beginPath()
                ctx.arc(pads[i][0], pads[i][1], padR, 0, Math.PI * 2)
                ctx.fill()
            }
        }

        function drawDevices(ctx, w, h) {
            const m = w * 0.16
            const cell = (w - m * 2) / 2
            const gap = cell * 0.18
            const size = cell - gap / 2
            const r = size * 0.28
            const positions = [
                [m, m],
                [m + cell + gap / 2, m],
                [m, m + cell + gap / 2],
                [m + cell + gap / 2, m + cell + gap / 2]
            ]
            for (let i = 0; i < positions.length; ++i) {
                roundedRect(ctx, positions[i][0], positions[i][1], size, size, r)
                ctx.stroke()
            }
        }

        function drawProfiles(ctx, w, h) {
            const cx = w / 2
            ctx.beginPath()
            ctx.moveTo(cx, h * 0.14)
            ctx.lineTo(w * 0.86, h * 0.36)
            ctx.lineTo(cx, h * 0.58)
            ctx.lineTo(w * 0.14, h * 0.36)
            ctx.closePath()
            ctx.stroke()

            ctx.beginPath()
            ctx.moveTo(w * 0.14, h * 0.54)
            ctx.lineTo(cx, h * 0.76)
            ctx.lineTo(w * 0.86, h * 0.54)
            ctx.stroke()

            ctx.beginPath()
            ctx.moveTo(w * 0.14, h * 0.7)
            ctx.lineTo(cx, h * 0.92)
            ctx.lineTo(w * 0.86, h * 0.7)
            ctx.stroke()
        }

        function drawSettings(ctx, w, h) {
            const cx = w / 2
            const cy = h / 2
            const outer = w * 0.42
            const inner = w * 0.3
            const teeth = 8
            ctx.beginPath()
            for (let i = 0; i <= teeth * 2; ++i) {
                const radius = (i % 2 === 0) ? outer : inner
                const angle = (Math.PI * i) / teeth
                const px = cx + radius * Math.cos(angle)
                const py = cy + radius * Math.sin(angle)
                if (i === 0) {
                    ctx.moveTo(px, py)
                } else {
                    ctx.lineTo(px, py)
                }
            }
            ctx.closePath()
            ctx.stroke()

            ctx.beginPath()
            ctx.arc(cx, cy, w * 0.13, 0, Math.PI * 2)
            ctx.stroke()
        }

        function drawChevron(ctx, w, h) {
            const cx = w / 2
            ctx.beginPath()
            ctx.moveTo(cx + w * 0.12, h * 0.25)
            ctx.lineTo(cx - w * 0.16, h * 0.5)
            ctx.lineTo(cx + w * 0.12, h * 0.75)
            ctx.stroke()
        }

        function drawActivity(ctx, w, h) {
            const left = w * 0.16
            const right = w * 0.84
            const lineCount = 4
            const gap = h * 0.18
            const top = h * 0.2

            for (let i = 0; i < lineCount; ++i) {
                const y = top + i * gap
                const lineWidth = right - left - (i % 2 === 0 ? 0 : w * 0.18)
                ctx.beginPath()
                ctx.moveTo(left, y)
                ctx.lineTo(lineWidth, y)
                ctx.stroke()
            }
        }

        function drawInfo(ctx, w, h) {
            const cx = w / 2
            const cy = h / 2
            ctx.beginPath()
            ctx.arc(cx, cy, w * 0.4, 0, Math.PI * 2)
            ctx.stroke()

            ctx.beginPath()
            ctx.arc(cx, cy - h * 0.18, navIcon.strokeWidth * 0.7, 0, Math.PI * 2)
            ctx.fill()

            ctx.beginPath()
            ctx.moveTo(cx, cy - h * 0.04)
            ctx.lineTo(cx, cy + h * 0.22)
            ctx.stroke()
        }

        function drawFilter(ctx, w, h) {
            const left = w * 0.18
            const right = w * 0.82
            const ys = [h * 0.28, h * 0.5, h * 0.72]
            const widths = [0.64, 0.44, 0.24]

            for (let i = 0; i < ys.length; ++i) {
                const lineWidth = w * widths[i]
                ctx.beginPath()
                ctx.moveTo(left, ys[i])
                ctx.lineTo(left + lineWidth, ys[i])
                ctx.stroke()
            }

            ctx.beginPath()
            ctx.moveTo(right - w * 0.12, h * 0.28)
            ctx.lineTo(right, h * 0.28)
            ctx.stroke()
        }

        function drawCheck(ctx, w, h) {
            ctx.beginPath()
            ctx.moveTo(w * 0.18, h * 0.52)
            ctx.lineTo(w * 0.42, h * 0.74)
            ctx.lineTo(w * 0.82, h * 0.28)
            ctx.stroke()
        }

        function roundedRect(ctx, x, y, rw, rh, r) {
            ctx.beginPath()
            ctx.moveTo(x + r, y)
            ctx.lineTo(x + rw - r, y)
            ctx.arcTo(x + rw, y, x + rw, y + r, r)
            ctx.lineTo(x + rw, y + rh - r)
            ctx.arcTo(x + rw, y + rh, x + rw - r, y + rh, r)
            ctx.lineTo(x + r, y + rh)
            ctx.arcTo(x, y + rh, x, y + rh - r, r)
            ctx.lineTo(x, y + r)
            ctx.arcTo(x, y, x + r, y, r)
            ctx.closePath()
        }
    }
}
