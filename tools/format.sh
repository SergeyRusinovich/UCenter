#!/bin/sh
# clang-format для исходников проекта. Без аргументов — исправляет файлы,
# с --check — только проверяет (код возврата 1, если есть расхождения; так работает CI).
set -eu
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FORMAT="$(command -v clang-format || true)"
[ -n "$FORMAT" ] || FORMAT="$(brew --prefix llvm 2>/dev/null)/bin/clang-format"
[ -x "$FORMAT" ] || { echo "clang-format not found (install LLVM)" >&2; exit 1; }
FILES="$(find "$ROOT/src" "$ROOT/tests" "$ROOT/fuzz" "$ROOT/tools" -name '*.cpp' -o -name '*.h')"
if [ "${1:-}" = "--check" ]; then
    # shellcheck disable=SC2086
    exec "$FORMAT" --dry-run -Werror $FILES
fi
# shellcheck disable=SC2086
exec "$FORMAT" -i $FILES
