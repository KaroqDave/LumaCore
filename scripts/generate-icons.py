#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later
"""Regenerate app icon raster assets from assets/icons/lumacore.svg."""

from __future__ import annotations

import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SVG_PATH = ROOT / "assets" / "icons" / "lumacore.svg"
ICON_DIR = SVG_PATH.parent
SIZES = (16, 32, 48, 64, 128, 256)


def _require_pyside6():
    try:
        from PySide6.QtCore import QSize, Qt
        from PySide6.QtGui import QImage, QPainter
        from PySide6.QtSvg import QSvgRenderer
        from PySide6.QtWidgets import QApplication
    except ImportError as error:
        raise SystemExit(
            "PySide6 is required to render SVG icons. Install it with: pip install PySide6"
        ) from error

    return QApplication, QImage, QPainter, QSvgRenderer, QSize, Qt


def render_pngs() -> list[Path]:
    QApplication, QImage, QPainter, QSvgRenderer, QSize, Qt = _require_pyside6()

    app = QApplication.instance() or QApplication(sys.argv)
    _ = app

    renderer = QSvgRenderer(str(SVG_PATH))
    if not renderer.isValid():
        raise SystemExit(f"Invalid SVG: {SVG_PATH}")

    outputs: list[Path] = []
    for size in SIZES:
        image = QImage(QSize(size, size), QImage.Format.Format_ARGB32_Premultiplied)
        image.fill(Qt.GlobalColor.transparent)
        painter = QPainter(image)
        renderer.render(painter)
        painter.end()

        output = ICON_DIR / f"lumacore-{size}.png"
        if not image.save(str(output), "PNG"):
            raise SystemExit(f"Failed to write {output}")
        outputs.append(output)
        print(f"wrote {output.relative_to(ROOT)}")

    return outputs


def write_ico(png_paths: list[Path]) -> Path:
    try:
        from PIL import Image
    except ImportError as error:
        raise SystemExit(
            "Pillow is required to write lumacore.ico. Install it with: pip install Pillow"
        ) from error

    ico_path = ICON_DIR / "lumacore.ico"
    images = [Image.open(path).convert("RGBA") for path in png_paths]
    images[-1].save(
        ico_path,
        format="ICO",
        sizes=[(size, size) for size in SIZES],
        append_images=images[:-1],
    )

    icon_count = struct.unpack_from("<H", ico_path.read_bytes(), 4)[0]
    print(f"wrote {ico_path.relative_to(ROOT)} ({icon_count} embedded sizes)")
    return ico_path


def main() -> int:
    if not SVG_PATH.is_file():
        raise SystemExit(f"Missing source icon: {SVG_PATH}")

    png_paths = render_pngs()
    write_ico(png_paths)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
