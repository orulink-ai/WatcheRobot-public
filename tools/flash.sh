#!/usr/bin/env bash
set -euo pipefail
tool_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
if [[ -z "${CONDA_PREFIX:-}" || ! -x "$CONDA_PREFIX/bin/python" ]]; then
  echo 'Run conda activate watcherobot first.' >&2
  exit 1
fi
exec "$CONDA_PREFIX/bin/python" -I "$tool_dir/flash_setup.py" "$@"
