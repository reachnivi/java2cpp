#!/usr/bin/env bash
# Run clang-tidy (config: .clang-tidy) over C++ sources using the compile
# commands from a CMake build dir.
#
#   scripts/lint.sh crash_lab              # all .cpp under crash_lab/
#   scripts/lint.sh exercises/09_threads_queue/test.cpp
#   BUILD=build-sol scripts/lint.sh exercises
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
build="${BUILD:-build}"
[[ $build = /* ]] || build="$root/$build"
[[ -f $build/compile_commands.json ]] || {
  echo "no $build/compile_commands.json: run cmake -S . -B build first"; exit 1; }
target="${1:-crash_lab}"
mapfile -t files < <(find "$root/$target" -name '*.cpp' 2>/dev/null || true)
[[ ${#files[@]} -gt 0 ]] || files=("$root/$target")
for f in "${files[@]}"; do
  echo "=== ${f#$root/}"
  clang-tidy -p "$build" --quiet "$f" 2>/dev/null | grep -E 'warning:|error:' || echo "   (clean)"
done
