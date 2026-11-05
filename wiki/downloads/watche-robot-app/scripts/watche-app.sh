#!/bin/sh
# 与 .ps1 使用相同参数；WATCHE_PYTHON 可指定宿主解释器。
case "$0" in
    */*) SCRIPT_PARENT=${0%/*} ;;
    *) SCRIPT_PARENT=. ;;
esac
SCRIPT_DIR=$(CDPATH= cd -- "$SCRIPT_PARENT" && pwd) || exit 1
INTERPRETER=${WATCHE_PYTHON:-python3}
if ! command -v "$INTERPRETER" >/dev/null 2>&1; then
    printf '%s\n' '缺少 Python；请设置 WATCHE_PYTHON 为解释器路径' >&2
    exit 1
fi
"$INTERPRETER" "$SCRIPT_DIR/workflow.py" "$@"
exit $?
