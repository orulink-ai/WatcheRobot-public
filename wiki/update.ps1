$ErrorActionPreference = 'Stop'
python (Join-Path $PSScriptRoot 'update.py') @args
exit $LASTEXITCODE
