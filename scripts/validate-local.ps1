param(
    [switch]$SkipTests
)

$ErrorActionPreference = "Stop"

function Write-Step {
    param([string]$Message)
    Write-Host "==> $Message"
}

function Assert-PathExists {
    param([string]$Path)
    if (-not (Test-Path -LiteralPath $Path)) {
        throw "Required path is missing: $Path"
    }
}

function Assert-ContentContains {
    param(
        [string]$Path,
        [string]$Pattern,
        [string]$Description
    )
    $text = Get-Content -LiteralPath $Path -Raw
    if ($text -notmatch $Pattern) {
        throw "$Description was not found in $Path"
    }
}

function Assert-ContentNotContains {
    param(
        [string]$Path,
        [string]$Pattern,
        [string]$Description
    )
    $text = Get-Content -LiteralPath $Path -Raw
    if ($text -match $Pattern) {
        throw "$Description was unexpectedly found in $Path"
    }
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$addonRoot = (Resolve-Path (Join-Path $scriptRoot "..")).Path

Write-Step "Checking addon layout"
$requiredPaths = @(
    "addon_config.mk",
    "README.md",
    "ofxggml-addon.json",
    "AGENTS.md",
    "HERMES.md",
    ".github/instructions/ofxggml-ecosystem.instructions.md",
    ".github/workflows/coding-agent-instructions.yml",
    "docs/STAGING_PLAN.md",
    "docs/STABLE_DIFFUSION_WORKFLOWS.md",
    "src/ofxGgmlStableDiffusion.h",
    "src/ofxGgmlStableDiffusion.cpp",
    "ofxGgmlStableDiffusionExample/addons.make",
    "scripts/run-tests.ps1",
    "scripts/doctor-stable-diffusion.ps1",
    "scripts/doctor-stable-diffusion.bat",
    "scripts/doctor-stable-diffusion.sh",
    "scripts/test-doctor-stable-diffusion.ps1",
    "scripts/run-stable-diffusion-runtime-smoke.ps1",
    "scripts/run-stable-diffusion-runtime-smoke.bat",
    "scripts/test-build-stable-diffusion-dry-run.ps1",
    "tests/CMakeLists.txt"
)

foreach ($relativePath in $requiredPaths) {
    Assert-PathExists (Join-Path $addonRoot $relativePath)
}

Write-Step "Checking staging contract"
$readmePath = Join-Path $addonRoot "README.md"
$agentsPath = Join-Path $addonRoot "AGENTS.md"
$stagingPath = Join-Path $addonRoot "docs/STAGING_PLAN.md"
$workflowGuidePath = Join-Path $addonRoot "docs/STABLE_DIFFUSION_WORKFLOWS.md"
$metadataPath = Join-Path $addonRoot "ofxggml-addon.json"
$addonConfigPath = Join-Path $addonRoot "addon_config.mk"
$addonsMakePath = Join-Path $addonRoot "ofxGgmlStableDiffusionExample/addons.make"

Assert-ContentContains $readmePath "ofxStableDiffusion" "ofxStableDiffusion lineage"
Assert-ContentContains $readmePath "base is .?ofxStableDiffusion.? as an addon" "ofxStableDiffusion addon base"
Assert-ContentContains $readmePath "stable-diffusion\.cpp" "stable-diffusion.cpp backend"
Assert-ContentContains $readmePath "not based on\s+.?ofxGgmlDiffusion" "ofxGgmlDiffusion exclusion"
Assert-ContentContains $agentsPath "ofxStableDiffusion" "agent lineage guidance"
Assert-ContentContains $stagingPath "Exclude .?ofxGgmlDiffusion" "staging exclusion guidance"
Assert-ContentContains $workflowGuidePath "ofxGgmlDiffusion.? is intentionally paused" "workflow guide diffusion exclusion"
Assert-ContentContains $metadataPath '"requires"\s*:\s*\[\s*\]' "standalone metadata dependency contract"
Assert-ContentContains $addonConfigPath "ADDON_NAME\s*=\s*ofxGgmlStableDiffusion" "addon name"
Assert-ContentContains $addonsMakePath "(?m)^ofxGgmlStableDiffusion\r?$" "example addon dependency"
Assert-ContentNotContains $addonsMakePath "(?m)^ofxGgmlDiffusion\r?$" "ofxGgmlDiffusion example dependency"

Write-Step "Checking Stable Diffusion doctor"
& (Join-Path $scriptRoot "test-doctor-stable-diffusion.ps1")
if (!$?) {
    throw "Stable Diffusion doctor smoke test failed"
}

Write-Step "Checking Stable Diffusion build dry-runs"
& (Join-Path $scriptRoot "test-build-stable-diffusion-dry-run.ps1")
if (!$?) {
    throw "Stable Diffusion build dry-run smoke test failed"
}

Write-Step "Checking Stable Diffusion runtime smoke dry-run"
& (Join-Path $scriptRoot "run-stable-diffusion-runtime-smoke.ps1") -DryRun -Json -SummaryOnly
if (!$?) {
    throw "Stable Diffusion runtime smoke dry-run failed"
}

if (-not $SkipTests) {
    Write-Step "Running CMake wrapper tests"
    & (Join-Path $scriptRoot "run-tests.ps1")
    if ($LASTEXITCODE -ne 0) {
        throw "run-tests.ps1 failed with exit code $LASTEXITCODE"
    }
}

Write-Step "Local validation completed"
