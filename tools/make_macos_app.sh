#!/bin/sh
# Собирает UCenter.app в корне проекта: обёртку, через которую macOS разрешает программе
# доступ к геопозиции компьютера (нужен NSLocationUsageDescription в Info.plist бандла).
# Приложение запускает main.py из этой папки с зависимостями из .venv:
# пересобирать нужно только после пересоздания .venv или переноса папки.
set -eu

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PY="${PYTHON:-$ROOT/.venv/bin/python}"
APP="$ROOT/UCenter.app"

[ -x "$PY" ] || { echo "Python not found: $PY (create .venv or set PYTHON)" >&2; exit 1; }
# Исполняемый файл бандла должен лежать внутри него, иначе macOS не увидит Info.plist,
# поэтому копируем интерпретатор framework-сборки Python (python.org, Homebrew)
INTERP="$("$PY" -c 'import os, sys; print(os.path.join(sys.base_prefix, "Resources/Python.app/Contents/MacOS/Python"))')"
SITE="$("$PY" -c 'import site; print(site.getsitepackages()[0])')"
[ -x "$INTERP" ] || { echo "Framework build of Python is required (python.org or Homebrew)" >&2; exit 1; }

rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS"
cp "$INTERP" "$APP/Contents/MacOS/python"

cat > "$APP/Contents/MacOS/UCenter" <<LAUNCHER
#!/bin/sh
export PYTHONPATH="$SITE:$ROOT"
export UCENTER_APP=1
cd "$ROOT"
exec "\$(dirname "\$0")/python" "$ROOT/main.py" "\$@"
LAUNCHER
chmod +x "$APP/Contents/MacOS/UCenter"

cat > "$APP/Contents/Info.plist" <<'PLIST'
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key><string>UCenter</string>
    <key>CFBundleIdentifier</key><string>local.ucenter</string>
    <key>CFBundleName</key><string>UCenter</string>
    <key>CFBundleDisplayName</key><string>UCenter</string>
    <key>CFBundlePackageType</key><string>APPL</string>
    <key>CFBundleShortVersionString</key><string>1.0</string>
    <key>NSHighResolutionCapable</key><true/>
    <key>NSLocationUsageDescription</key>
    <string>UCenter uses this computer's location as point A for azimuth measurements.</string>
    <key>NSLocationWhenInUseUsageDescription</key>
    <string>UCenter uses this computer's location as point A for azimuth measurements.</string>
</dict>
</plist>
PLIST

# Ad-hoc подпись: без неё на Apple Silicon бандл не запустится, а macOS не запомнит разрешение
codesign --force --deep --sign - "$APP"
echo "Built $APP — start it with: open \"$APP\""
