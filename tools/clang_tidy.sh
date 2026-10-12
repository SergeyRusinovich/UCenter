#!/bin/sh
# clang-tidy по исходникам проекта (src/, tests/, fuzz/) с флагами из compile_commands.json.
# Запуск из корня: tools/clang_tidy.sh [каталог сборки, по умолчанию build-debug] [файлы]
# Файлы — регулярное выражение пути от корня без .cpp, по умолчанию все: '(src|tests|fuzz)/.*'
# Нужен LLVM (macOS: brew install llvm; Linux: пакет clang-tidy).
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${1:-$ROOT/build-debug}"
FILES="${2:-(src|tests|fuzz)/.*}"
# Homebrew ставит LLVM вне PATH
LLVM_BIN="$(brew --prefix llvm 2>/dev/null || true)/bin"
[ -d "$LLVM_BIN" ] && PATH="$LLVM_BIN:$PATH"
# Версию можно задать явно: CLANG_TIDY=clang-tidy-19 RUN_CLANG_TIDY=run-clang-tidy-19
RUN="${RUN_CLANG_TIDY:-$(command -v run-clang-tidy || true)}"
TIDY="${CLANG_TIDY:-$(command -v clang-tidy || true)}"
[ -n "$RUN" ] && [ -n "$TIDY" ] || { echo "run-clang-tidy / clang-tidy not found (install LLVM)" >&2; exit 1; }
[ -f "$BUILD/compile_commands.json" ] || { echo "No $BUILD/compile_commands.json: configure the build first" >&2; exit 1; }
# clang-tidy из LLVM не знает, где SDK macOS: подсказываем, как это делает Apple clang
EXTRA=""
command -v xcrun >/dev/null && EXTRA="-extra-arg=-isysroot$(xcrun --show-sdk-path)"
# Сгенерированные moc/rcc-файлы не проверяем
# shellcheck disable=SC2086
exec "$RUN" -quiet -clang-tidy-binary "$TIDY" $EXTRA -p "$BUILD" "$ROOT/$FILES\.cpp$"
