#!/usr/bin/env bash
set -u
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
python "$script_dir/verify.py" "$@"
exit $?
