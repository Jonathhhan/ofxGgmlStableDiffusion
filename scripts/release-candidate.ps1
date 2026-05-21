param(
    [switch]$SkipTests
)

$ErrorActionPreference = "Stop"
$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path

& (Join-Path $scriptRoot "validate-local.ps1") -SkipTests:$SkipTests
if ($LASTEXITCODE -ne 0) {
    throw "validate-local.ps1 failed with exit code $LASTEXITCODE"
}
