#!/usr/bin/env bash
# Positional arguments match PowerShell: Example Action Port.
set -u
example="${1:-robot}"
action="${2:-build}"
port="${3:-}"
case "$example" in body|head|robot) ;; *) echo 'Example must be body/head/robot' >&2; exit 2;; esac
case "$action" in build|flash|monitor) ;; *) echo 'Action must be build/flash/monitor' >&2; exit 2;; esac
if [[ "$action" != build && -z "$port" ]]; then
  echo 'flash/monitor requires an explicit Port' >&2; exit 2
fi
if ! command -v idf.py >/dev/null; then
  echo 'Activate ESP-IDF 6.0.2 first' >&2; exit 2
fi
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
args=(-C "$script_dir/$example")
if [[ -n "$port" ]]; then args+=(-p "$port"); fi
idf.py "${args[@]}" "$action"
exit $?

