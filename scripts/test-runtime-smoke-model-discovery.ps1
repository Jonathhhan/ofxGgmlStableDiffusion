$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$smokeScript = Join-Path $scriptRoot "run-stable-diffusion-runtime-smoke.ps1"

if (!(Test-Path -LiteralPath $smokeScript -PathType Leaf)) {
    throw "Stable Diffusion runtime smoke script was not found: $smokeScript"
}

$plan = (& $smokeScript -DryRun -Json -SummaryOnly) | ConvertFrom-Json
if (![bool]$plan.CliReady -or !(Test-Path -LiteralPath ([string]$plan.CliPath) -PathType Leaf)) {
    throw "Stable Diffusion runtime smoke did not resolve the bundled sd-cli executable."
}
if ([bool]$plan.ModelReady) {
    $extension = [System.IO.Path]::GetExtension([string]$plan.ModelPath).ToLowerInvariant()
    if ($extension -notin @(".safetensors", ".ckpt")) {
        throw "Automatic Stable Diffusion model discovery selected an unrelated model type: $($plan.ModelPath)"
    }
}

$tempModel = Join-Path ([System.IO.Path]::GetTempPath()) ("explicit-stable-diffusion-model-" + [Guid]::NewGuid().ToString("N") + ".gguf")
try {
    [System.IO.File]::WriteAllBytes($tempModel, [byte[]]@(0))
    $explicitPlan = (& $smokeScript -DryRun -Json -SummaryOnly -Model $tempModel) | ConvertFrom-Json
    if (![bool]$explicitPlan.ModelReady -or [string]$explicitPlan.ModelPath -ne $tempModel) {
        throw "An explicitly configured model path should remain available for compatibility diagnostics."
    }
} finally {
    if (Test-Path -LiteralPath $tempModel -PathType Leaf) {
        Remove-Item -LiteralPath $tempModel -Force
    }
}

Write-Host "Stable Diffusion runtime smoke model discovery checks passed."
