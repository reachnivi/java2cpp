#!/usr/bin/env bash
# CI check for starter mode (J2C_USE_SOLUTIONS=OFF).
# Starter code is SUPPOSED to fail its tests. What must never happen is a
# starter that crashes or hangs: that would make the learner's first run
# useless. Also guards against tests silently disappearing.
#
#   scripts/ci/check_starters.sh [build-dir] [expected-test-count]
set -uo pipefail

build="${1:-build}"
expected="${2:-}"

out="$(ctest --test-dir "$build" -j"$(nproc)" 2>&1)"

bad="$(grep -E '\((SEGFAULT|Subprocess aborted|Timeout|Child aborted|OTHER_FAULT|ILLEGAL|BUS|NUMERICAL|Exception: Other)\)' <<<"$out" || true)"
if [[ -n $bad ]]; then
  echo "::error::starter tests must fail cleanly, but some crashed or hung:"
  echo "$bad"
  exit 1
fi

total="$(grep -oE 'tests failed out of [0-9]+' <<<"$out" | grep -oE '[0-9]+$' || true)"
if [[ -z $total ]]; then
  total="$(grep -oE 'tests passed, 0 tests failed out of [0-9]+' <<<"$out" | grep -oE '[0-9]+$' || true)"
fi
echo "starter tests discovered: ${total:-?}"
grep -E 'tests passed' <<<"$out"

if [[ -n $expected && $total != "$expected" ]]; then
  echo "::error::expected $expected tests, found ${total:-none}: a test file stopped compiling into tests?"
  exit 1
fi
echo "OK: all starters compile, and every failing test fails cleanly (no crash, no hang)."
