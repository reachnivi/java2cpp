#!/usr/bin/env bash
# CI check: every crash lab still shows the symptom its README promises, and
# the debugging workflows the answers rely on actually work.
#
#   scripts/ci/verify_labs.sh [build-dir]
set -uo pipefail

root="$(cd "$(dirname "$0")/../.." && pwd)"
build="${1:-build}"
[[ $build = /* ]] || build="$root/$build"
labs="$build/crash_lab"
work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
failures=0

fail() { echo "::error::$*"; failures=$((failures + 1)); }

# lab -> expected exit status ("any" = undefined behaviour, outcome varies)
declare -A expect=(
  [01]=139 [02]=134 [03]=134 [04]=134 [05]=134 [06]=any [07]=134 [08]=124
  [09]=124 [10]=any [11]=139 [12]=0 [13]=any [14]=2 [15]=139 [16]=139
)

for n in $(printf '%s\n' "${!expect[@]}" | sort); do
  bin="$labs/lab$n"
  [[ -x $bin ]] || { fail "lab$n not built"; continue; }
  # (the braces keep bash's own "Segmentation fault" job message out of the log)
  { timeout -s KILL 30 timeout 5 "$bin" >"$work/lab$n.out" 2>&1; } 2>/dev/null
  status=$?
  want="${expect[$n]}"
  if [[ $want == any ]]; then
    printf 'lab%s: status %-3s (undefined behaviour: any outcome accepted)\n' "$n" "$status"
  elif [[ $status == "$want" ]]; then
    printf 'lab%s: status %-3s OK\n' "$n" "$status"
  else
    fail "lab$n: expected status $want, got $status"
    tail -5 "$work/lab$n.out"
  fi
done

# Lab 12 must visibly leak: the alive-Commands counter keeps growing.
alive="$(grep -oE 'alive Commands=[0-9]+' "$work/lab12.out" | tail -1 | grep -oE '[0-9]+$')"
if [[ ${alive:-0} -lt 40000 ]]; then
  fail "lab12: expected >=40000 live Commands at the end, got '${alive:-none}'"
else
  echo "lab12: $alive Commands still alive after inflight map emptied OK"
fi

# Lab 16: the crash handler prints a symbolised backtrace, and symbolize.sh
# turns it into file:line.
if grep -q 'onAppendEntriesReply' "$work/lab16.out"; then
  resolved="$("$root/crash_lab/scripts/symbolize.sh" <"$work/lab16.out" | grep onAppendEntriesReply | head -1)"
  if [[ $resolved == *"16_crash_handler/main.cpp:30"* ]]; then
    echo "lab16: symbolize.sh resolves the crash to main.cpp:30 OK"
  else
    fail "lab16: symbolize.sh did not resolve to main.cpp:30: $resolved"
  fi
else
  fail "lab16: crash handler output has no onAppendEntriesReply frame"
fi

# Lab 15: production workflow — core from the stripped binary, symbols from
# the separate .debug file.
if command -v gdb >/dev/null; then
  (
    cd "$work" && ulimit -c unlimited && rm -f core* &&
      { "$labs/lab15_stripped" >/dev/null 2>&1; } 2>/dev/null
  )
  core="$(ls "$work"/core* 2>/dev/null | head -1)"
  if [[ -z $core ]]; then
    echo "lab15: no core file written here (core_pattern: $(cat /proc/sys/kernel/core_pattern)); skipping gdb check"
  else
    bt="$(gdb -batch -ex "symbol-file $labs/lab15.debug" -ex bt "$labs/lab15_stripped" "$core" 2>&1)"
    if grep -q 'Ledger::transfer' <<<"$bt"; then
      echo "lab15: gdb + lab15.debug symbolises the stripped core OK"
    else
      fail "lab15: gdb could not symbolise the stripped core"
      echo "$bt" | tail -5
    fi
  fi
else
  echo "lab15: gdb not installed; skipping"
fi

if ((failures > 0)); then
  echo "$failures crash-lab check(s) failed"
  exit 1
fi
echo "OK: all crash labs behave as documented."
