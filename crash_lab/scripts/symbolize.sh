#!/usr/bin/env bash
# Turn backtrace_symbols() lines from a log into function + file:line.
#
#   ./build/crash_lab/lab16 2>&1 | crash_lab/scripts/symbolize.sh
#   crash_lab/scripts/symbolize.sh < service.log
#
# Handles both forms glibc prints:
#   /path/bin(_ZN3Foo3barEv+0x5c)[0x55d...]   symbol + offset  (needs -rdynamic or unstripped binary)
#   /path/bin(+0x1bfe)[0x55d...]               module-relative offset
# Uses the binary named in the line; pass DEBUG_FILE=path.debug to resolve
# against separate debug info (for stripped deployments).
set -euo pipefail

re_sym='^([^()]+)\(([^+()]*)\+(0x[0-9a-fA-F]+)\)\[(0x[0-9a-fA-F]+)\]'

while IFS= read -r line; do
  if [[ ! $line =~ $re_sym ]]; then
    echo "$line"
    continue
  fi
  bin="${BASH_REMATCH[1]}"
  sym="${BASH_REMATCH[2]}"
  off="${BASH_REMATCH[3]}"
  target_file="${DEBUG_FILE:-$bin}"

  if [[ -n $sym ]]; then
    base=$( (nm "$target_file" 2>/dev/null; nm -D "$target_file" 2>/dev/null) |
           awk -v s="$sym" '$3 == s || $3 ~ "^"s"@" { print $1; exit }')
    if [[ -z $base ]]; then
      echo "$line    # (symbol not found in $target_file)"
      continue
    fi
    addr=$(( 0x$base + off ))
  else
    addr=$(( off ))
  fi
  # Frames other than the crashing one hold RETURN addresses (the instruction
  # after the call). Subtracting 1 makes addr2line report the call's line.
  addr=$(( addr - 1 ))
  resolved=$(addr2line -f -C -i -p -e "$target_file" "$(printf '0x%x' "$addr")" 2>/dev/null || true)
  printf '%-60s -> %s\n' "${sym:-+$off} ($(basename "$bin"))" "${resolved:-??}"
done
