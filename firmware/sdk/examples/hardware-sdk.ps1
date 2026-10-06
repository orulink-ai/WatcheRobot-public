param(
    [string]$Example='robot',
    [string]$Action='build',
    [string]$Port=''
)
$ErrorActionPreference='Stop'
if ($Example -cnotin @('body','head','robot') -or $Action -cnotin @('build','flash','monitor')) {
    [Console]::Error.WriteLine('Example must be body/head/robot; Action must be build/flash/monitor'); exit 2
}
if ($Action -ne 'build' -and [string]::IsNullOrWhiteSpace($Port)) {
    [Console]::Error.WriteLine('flash/monitor requires an explicit Port'); exit 2
}
if (-not (Get-Command idf.py -ErrorAction SilentlyContinue)) {
    [Console]::Error.WriteLine('Activate ESP-IDF 6.0.2 first'); exit 2
}
$argsForIdf=@('-C', (Join-Path $PSScriptRoot $Example))
if ($Port) { $argsForIdf+=@('-p',$Port) }
& idf.py @argsForIdf $Action
exit $LASTEXITCODE

