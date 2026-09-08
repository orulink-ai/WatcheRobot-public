# Use only the explicitly activated project Conda environment.
$ErrorActionPreference = 'Stop'
if (-not $env:CONDA_PREFIX -or -not (Test-Path -LiteralPath (Join-Path $env:CONDA_PREFIX 'python.exe'))) {
    Write-Error 'Run conda activate watcherobot first.'
    exit 1
}
& (Join-Path $env:CONDA_PREFIX 'python.exe') -I (Join-Path $PSScriptRoot 'flash_setup.py') @args
exit $LASTEXITCODE
