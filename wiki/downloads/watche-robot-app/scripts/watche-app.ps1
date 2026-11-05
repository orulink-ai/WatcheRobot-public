# 与 .sh 保持相同参数和退出码；WATCHE_PYTHON 可指定宿主解释器。
$ErrorActionPreference = 'Stop'
$interpreter = if ($env:WATCHE_PYTHON) { $env:WATCHE_PYTHON } else { 'python' }
try {
    & $interpreter (Join-Path $PSScriptRoot 'workflow.py') @args
    exit $LASTEXITCODE
} catch {
    [Console]::Error.WriteLine($_.Exception.Message)
    exit 1
}
