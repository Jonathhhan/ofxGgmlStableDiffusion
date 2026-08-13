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
    "ofxGgmlStableDiffusionBasicGenerationExample/addons.make",
    "ofxGgmlStableDiffusionImageWorkflowExample/addons.make",
    "ofxGgmlStableDiffusionVideoGenerationExample/addons.make",
    "ofxGgmlStableDiffusionVideoControlFramesExample/addons.make",
    "ofxGgmlStableDiffusionCreativeLoopExample/addons.make",
    "ofxGgmlStableDiffusionLoraEmbeddingExample/addons.make",
    "scripts/run-tests.ps1",
    "scripts/doctor-stable-diffusion.ps1",
    "scripts/doctor-stable-diffusion.bat",
    "scripts/doctor-stable-diffusion.sh",
    "scripts/test-doctor-stable-diffusion.ps1",
    "scripts/test-project-generator-examples.ps1",
    "scripts/run-stable-diffusion-runtime-smoke.ps1",
    "scripts/run-stable-diffusion-runtime-smoke.bat",
    "scripts/run-image-generation-smoke.ps1",
    "scripts/run-image-generation-smoke.bat",
    "scripts/test-runtime-smoke-model-discovery.ps1",
    "scripts/run-wan-context-smoke.ps1",
    "scripts/run-wan-context-smoke.bat",
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
$nativeGuidePath = Join-Path $addonRoot "docs/NATIVE_BUILD.md"
$metadataPath = Join-Path $addonRoot "ofxggml-addon.json"
$addonConfigPath = Join-Path $addonRoot "addon_config.mk"
$addonsMakePath = Join-Path $addonRoot "ofxGgmlStableDiffusionExample/addons.make"
$basicAddonsMakePath = Join-Path $addonRoot "ofxGgmlStableDiffusionBasicGenerationExample/addons.make"
$imageWorkflowAddonsMakePath = Join-Path $addonRoot "ofxGgmlStableDiffusionImageWorkflowExample/addons.make"
$videoAddonsMakePath = Join-Path $addonRoot "ofxGgmlStableDiffusionVideoGenerationExample/addons.make"
$videoControlFramesAddonsMakePath = Join-Path $addonRoot "ofxGgmlStableDiffusionVideoControlFramesExample/addons.make"
$creativeLoopAddonsMakePath = Join-Path $addonRoot "ofxGgmlStableDiffusionCreativeLoopExample/addons.make"
$loraEmbeddingAddonsMakePath = Join-Path $addonRoot "ofxGgmlStableDiffusionLoraEmbeddingExample/addons.make"
$starterCppPath = Join-Path $addonRoot "ofxGgmlStableDiffusionExample/src/ofApp.cpp"
$starterHeaderPath = Join-Path $addonRoot "ofxGgmlStableDiffusionExample/src/ofApp.h"
$starterReadmePath = Join-Path $addonRoot "ofxGgmlStableDiffusionExample/README.md"
$basicCppPath = Join-Path $addonRoot "ofxGgmlStableDiffusionBasicGenerationExample/src/ofApp.cpp"
$imageWorkflowCppPath = Join-Path $addonRoot "ofxGgmlStableDiffusionImageWorkflowExample/src/ofApp.cpp"
$videoCppPath = Join-Path $addonRoot "ofxGgmlStableDiffusionVideoGenerationExample/src/ofApp.cpp"
$videoControlFramesCppPath = Join-Path $addonRoot "ofxGgmlStableDiffusionVideoControlFramesExample/src/ofApp.cpp"
$nativeBuildPath = Join-Path $addonRoot "scripts/build-stable-diffusion.ps1"
$nativeBuildBatchPath = Join-Path $addonRoot "scripts/build-stable-diffusion.bat"
$nativeDownloadPath = Join-Path $addonRoot "scripts/download-stable-diffusion-release.ps1"
$setupAddonPath = Join-Path $addonRoot "scripts/setup_addon.ps1"
$setupWindowsPath = Join-Path $addonRoot "scripts/setup_windows.bat"

Assert-ContentContains $readmePath "ofxStableDiffusion" "ofxStableDiffusion lineage"
Assert-ContentContains $readmePath "base is .?ofxStableDiffusion.? as an addon" "ofxStableDiffusion addon base"
Assert-ContentContains $readmePath "stable-diffusion\.cpp" "stable-diffusion.cpp backend"
Assert-ContentContains $readmePath "not based on\s+.?ofxGgmlDiffusion" "ofxGgmlDiffusion exclusion"
Assert-ContentContains $agentsPath "ofxStableDiffusion" "agent lineage guidance"
Assert-ContentContains $stagingPath "Exclude .?ofxGgmlDiffusion" "staging exclusion guidance"
Assert-ContentContains $workflowGuidePath "ofxGgmlDiffusion.? is intentionally paused" "workflow guide diffusion exclusion"
Assert-ContentContains $metadataPath '"requires"\s*:\s*\[\s*"ofxGgmlCore"\s*\]' "Core metadata dependency contract"
$metadata = Get-Content -LiteralPath $metadataPath -Raw | ConvertFrom-Json
if ([string]$metadata.inferenceSmokeReport -ne [string]$metadata.runtimeSmokeReport -or
    [string]$metadata.inferenceSmokeReport -ne ".stable-diffusion-runtime-smoke.json") {
    throw "Stable Diffusion runtime and inference evidence must share .stable-diffusion-runtime-smoke.json."
}
Assert-ContentContains $basicCppPath 'ofLogWarning\("ofxGgmlStableDiffusionSmoke"\)' "visible wrapper smoke result"
Assert-ContentContains $starterCppPath 'Supported modes:' "capability-aware starter summary"
Assert-ContentContains $starterCppPath 'capabilities\.textToImage' "capability-aware image generation gate"
Assert-ContentContains $starterCppPath 'ofxGgmlStableDiffusionVideoGenerationExample' "video-model workflow routing"
Assert-ContentContains $addonConfigPath "ADDON_NAME\s*=\s*ofxGgmlStableDiffusion" "addon name"
Assert-ContentContains $addonConfigPath "ADDON_DEPENDENCIES\s*\+=\s*ofxGgmlCore" "Core addon dependency"
Assert-ContentContains $addonsMakePath "(?m)^ofxGgmlStableDiffusion\r?$" "example addon dependency"
Assert-ContentContains $addonsMakePath "(?m)^ofxGgmlCore\r?$" "example Core dependency"
Assert-ContentContains $addonsMakePath "(?m)^ofxImGui\r?$" "example ImGui dependency"
Assert-ContentNotContains $addonsMakePath "(?m)^ofxGgmlDiffusion\r?$" "ofxGgmlDiffusion example dependency"
foreach ($exampleAddonsMakePath in @($basicAddonsMakePath, $imageWorkflowAddonsMakePath, $videoAddonsMakePath, $videoControlFramesAddonsMakePath, $creativeLoopAddonsMakePath, $loraEmbeddingAddonsMakePath)) {
    Assert-ContentContains $exampleAddonsMakePath "(?m)^ofxGgmlStableDiffusion\r?$" "example addon dependency"
    Assert-ContentContains $exampleAddonsMakePath "(?m)^ofxGgmlCore\r?$" "example Core dependency"
    Assert-ContentContains $exampleAddonsMakePath "(?m)^ofxImGui\r?$" "example ImGui dependency"
    Assert-ContentNotContains $exampleAddonsMakePath "(?m)^ofxGgmlDiffusion\r?$" "ofxGgmlDiffusion example dependency"
}
if (Test-Path -LiteralPath (Join-Path $addonRoot "examples") -PathType Container) {
    throw "Examples should live at the addon root, not under a nested examples directory."
}

Write-Step "Checking canonical starter and native source pin"
Assert-ContentContains $starterCppPath 'imgui_stdlib\.h' "dynamic ImGui string support"
Assert-ContentContains $starterCppPath 'InputText\("Model path",\s*&modelPath\)' "pasteable model path"
Assert-ContentContains $starterCppPath 'OFXGGML_STABLE_DIFFUSION_MODEL' "configured model discovery"
Assert-ContentContains $starterCppPath 'last_model\.txt' "successful model path persistence"
Assert-ContentContains $starterCppPath 'settings\.backend\s*=\s*"cuda"' "automatic CUDA context selection"
Assert-ContentContains $starterCppPath 'OFXGGML_STABLE_DIFFUSION_IMAGE_SMOKE' "model-backed starter image smoke mode"
Assert-ContentContains $starterCppPath 'ofSaveImage' "starter image smoke output"
Assert-ContentContains $starterCppPath 'Local models' "recursive starter model chooser"
Assert-ContentContains $starterCppPath 'VRAM policy' "explicit starter VRAM policy display"
Assert-ContentContains $starterReadmePath 'run-image-generation-smoke\.ps1' "starter image smoke documentation"
Assert-ContentNotContains $starterHeaderPath 'array<char' "fixed-size starter text buffers"
Assert-ContentContains $basicCppPath 'Select PhotoMaker ID images folder' "PhotoMaker ID images folder browser"
Assert-ContentContains $imageWorkflowCppPath 'Browse Input\.\.\.' "image workflow input image browser"
Assert-ContentContains $imageWorkflowCppPath 'imgui_stdlib\.h' "pasteable image workflow strings"
Assert-ContentContains $imageWorkflowCppPath 'Match input size' "image workflow input dimension matching"
Assert-ContentContains $imageWorkflowCppPath 'workflowReady' "mode-aware image workflow readiness"
Assert-ContentNotContains $imageWorkflowCppPath 'array<char' "fixed-size image workflow buffers"
Assert-ContentContains $imageWorkflowCppPath 'Browse Mask\.\.\.' "image workflow mask browser"
Assert-ContentContains $imageWorkflowCppPath 'Browse Control\.\.\.' "image workflow control image browser"
Assert-ContentContains $videoCppPath 'Select video start image' "video start image browser"
Assert-ContentContains $videoCppPath 'Select video end frame' "video end frame browser"
Assert-ContentContains $videoControlFramesCppPath 'Select control frame folder' "video control frame folder browser"
foreach ($pinPath in @($nativeBuildPath, $nativeDownloadPath, $setupWindowsPath, $readmePath, $nativeGuidePath)) {
    Assert-ContentContains $pinPath 'master-813-bfbef5b' "current stable-diffusion.cpp release pin"
}
Assert-ContentContains $setupAddonPath '\[string\]\$SourceReleaseTag' "PowerShell source release override"
Assert-ContentContains $setupWindowsPath '--source-release-tag' "Windows source release override"
Assert-ContentContains $nativeBuildPath '\[string\]\$CudaArchitectures' "PowerShell CUDA architecture override"
Assert-ContentContains $nativeBuildBatchPath '--cuda-architectures' "Windows build CUDA architecture override"
Assert-ContentContains $nativeBuildBatchPath '--build-cli' "Windows build CLI option"
Assert-ContentContains $nativeBuildBatchPath '--skip-source-refresh' "Windows source reuse option"
Assert-ContentContains $setupWindowsPath '--cuda-architectures' "Windows CUDA architecture override"
Assert-ContentContains $nativeGuidePath 'master-820-de298c2' "latest verified bundled upstream runtime"
Assert-ContentContains $nativeGuidePath 'UseBundledGgml' "latest upstream bundled ggml compatibility lane"
Assert-ContentNotContains $setupAddonPath 'GgmlReleaseTag' "duplicate ggml setup option"
Assert-ContentNotContains $setupWindowsPath 'ggml-release-tag' "duplicate ggml Windows setup option"
Assert-ContentContains $nativeBuildPath 'GgmlReleaseTag only applies to -UseBundledGgml' "bundled ggml pin guard"

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

Write-Step "Checking projectGenerator example generation"
& (Join-Path $scriptRoot "test-project-generator-examples.ps1")
if (!$?) {
    throw "projectGenerator example smoke test failed"
}

Write-Step "Checking Stable Diffusion runtime smoke dry-run"
& (Join-Path $scriptRoot "run-stable-diffusion-runtime-smoke.ps1") -DryRun -Json -SummaryOnly
Write-Step "Checking openFrameworks image generation smoke dry-run"
& (Join-Path $scriptRoot "run-image-generation-smoke.ps1") -DryRun -Json
if (!$?) {
    throw "Stable Diffusion runtime smoke dry-run failed"
}

Write-Step "Checking Stable Diffusion runtime smoke model discovery"
& (Join-Path $scriptRoot "test-runtime-smoke-model-discovery.ps1")
if (!$?) {
    throw "Stable Diffusion runtime smoke model discovery test failed"
}

Write-Step "Checking WAN context smoke dry-run"
& (Join-Path $scriptRoot "run-wan-context-smoke.ps1") -DryRun
if (!$?) {
    throw "WAN context smoke dry-run failed"
}

if (-not $SkipTests) {
    Write-Step "Running CMake wrapper tests"
    & (Join-Path $scriptRoot "run-tests.ps1")
    if ($LASTEXITCODE -ne 0) {
        throw "run-tests.ps1 failed with exit code $LASTEXITCODE"
    }
}

Write-Step "Local validation completed"
