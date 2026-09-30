#!/usr/bin/env bash
# Run a crash lab with core dumps enabled, and tell you how to open the core.
#
#   crash_lab/scripts/run_lab.sh 01            # build dir defaults to ./build
#   BUILD=build-asan crash_lab/scripts/run_lab.sh 06
#   crash_lab/scripts/run_lab.sh 03 abc        # extra args go to the program
#
# Cores land in ./cores/core.labNN. Hanging labs (08, 09) are snapshotted with
# gcore after a few seconds, then killed.
set -uo pipefail

lab="${1:?usage: run_lab.sh NN [program args...]}"
shift
root="$(cd "$(dirname "$0")/../.." && pwd)"
build="${BUILD:-build}"
[[ $build = /* ]] || build="$root/$build"
bin="$build/crash_lab/lab$lab"
[[ $lab == 15 ]] && bin="${bin}_stripped"  # run what production would run
[[ -x $bin ]] || { echo "no $bin — run: cmake -S . -B build && cmake --build build -j"; exit 1; }

pattern="$(cat /proc/sys/kernel/core_pattern 2>/dev/null || echo '?')"
if [[ $pattern == \|* ]]; then
  echo "note: core_pattern pipes cores to a helper: $pattern"
  echo "      (systemd: 'coredumpctl list' / 'coredumpctl gdb'; Ubuntu apport: /var/crash)"
fi

mkdir -p "$root/cores"
cd "$root/cores" || exit 1
rm -f core "core.lab$lab"
ulimit -c unlimited

case "$lab" in
  08|09)
    "$bin" "$@" &
    pid=$!
    sleep 3
    if kill -0 "$pid" 2>/dev/null; then
      echo ">>> lab$lab (pid $pid) is still running after 3s: looks hung. Snapshotting with gcore..."
      gcore -o "core.lab$lab" "$pid" >/dev/null 2>&1 && mv "core.lab$lab.$pid" "core.lab$lab"
      kill -9 "$pid" 2>/dev/null
      wait "$pid" 2>/dev/null
      echo ">>> live alternative next time:  gdb -p <pid>   then: thread apply all bt"
    else
      wait "$pid"
      echo ">>> exited with status $?"
    fi
    ;;
  *)
    "$bin" "$@"
    status=$?
    if (( status > 128 )); then
      echo ">>> killed by signal $((status - 128)) ($(kill -l $((status - 128)) 2>/dev/null)), exit status $status"
    else
      echo ">>> exited with status $status"
    fi
    [[ -f core ]] && mv core "core.lab$lab"
    ;;
esac

if [[ -f core.lab$lab ]]; then
  echo ">>> core file: $root/cores/core.lab$lab"
  echo ">>> gdb $bin $root/cores/core.lab$lab"
elif [[ $lab != 06 && $lab != 10 && $lab != 12 && $lab != 14 ]]; then
  echo ">>> no core produced. Check: ulimit -c, core_pattern ($pattern), disk space, cwd writable."
fi
