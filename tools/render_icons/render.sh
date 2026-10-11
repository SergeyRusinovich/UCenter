#!/bin/sh
# Пересобирает растровые иконки из resources/icons/ucenter.svg:
# ucenter.png (окно), ucenter.ico (Windows), ucenter.icns (macOS, нужен iconutil).
# Запуск из корня проекта: tools/render_icons/render.sh [путь к Qt]
set -eu
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="$(mktemp -d)"
trap 'rm -rf "$BUILD"' EXIT
cmake -S "$ROOT/tools/render_icons" -B "$BUILD" ${1:+-DCMAKE_PREFIX_PATH="$1"} >/dev/null
cmake --build "$BUILD" >/dev/null
QT_QPA_PLATFORM=offscreen "$BUILD/render_icons" "$ROOT/resources/icons/ucenter.svg" "$BUILD/out"
cp "$BUILD/out/ucenter.png" "$BUILD/out/ucenter.ico" "$ROOT/resources/icons/"
if command -v iconutil >/dev/null; then
    iconutil -c icns "$BUILD/out/ucenter.iconset" -o "$ROOT/resources/icons/ucenter.icns"
else
    echo "iconutil not found (macOS only): ucenter.icns is not updated" >&2
fi
echo "Icons updated in $ROOT/resources/icons"
