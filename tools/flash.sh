#!/usr/bin/env bash
# Flash a WatcheRobot ESP32-S3 release ZIP.
#
# Usage:
#   ./tools/flash.sh --zip ./WatcheRobot-ESP32S3-v0.3.2.zip --port /dev/ttyUSB0 --monitor

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

cd "${REPO_ROOT}"
python -m tools.win_flasher "$@"
