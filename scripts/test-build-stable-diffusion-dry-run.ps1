param()

$ErrorActionPreference = "Stop"

function Assert-Contains {
	param(
		[string]$Text,
		[string]$Needle,
		[string]$Label
	)
	if (!$Text.Contains($Needle)) {
		throw "$Label did not contain expected text: $Needle`n$Text"
	}
}

function Assert-NotContains {
	param(
		[string]$Text,
		[string]$Needle,
		[string]$Label
	)
	if ($Text.Contains($Needle)) {
		throw "$Label unexpectedly contained text: $Needle`n$Text"
	}
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$addonRoot = (Resolve-Path (Join-Path $scriptRoot "..")).Path
$addonsRoot = Split-Path -Parent $addonRoot
$buildScript = Join-Path $scriptRoot "build-stable-diffusion.ps1"
$coreRoot = Join-Path $addonsRoot "ofxGgmlCore"
$coreManifestScript = Join-Path $coreRoot "scripts\runtime-provider-manifest.ps1"

if (-not (Test-Path -LiteralPath $coreManifestScript -PathType Leaf)) {
	throw "ofxGgmlCore ggml provider is not staged. Run ofxGgmlCore\scripts\setup-ggml.ps1 first, or use -UseBundledGgml for manual fallback builds."
}
$coreManifest = & $coreManifestScript -Json -SummaryOnly | ConvertFrom-Json
if (-not [bool]$coreManifest.ReadyForCompanions) {
	throw "ofxGgmlCore ggml provider is not staged. Run ofxGgmlCore\scripts\setup-ggml.ps1 first, or use -UseBundledGgml for manual fallback builds."
}

$cpuOutput = & $buildScript -DryRun -SkipSourceRefresh -CpuOnly 2>&1 6>&1 | Out-String
Assert-Contains $cpuOutput "Backend mode: cpu-only" "CPU dry-run"
Assert-Contains $cpuOutput "System GGML: ON" "CPU dry-run"
Assert-Contains $cpuOutput "-DSD_USE_SYSTEM_GGML=ON" "CPU dry-run"
Assert-Contains $cpuOutput "-DSD_CUDA=OFF" "CPU dry-run"

$cudaOutput = & $buildScript -DryRun -SkipSourceRefresh -Cuda 2>&1 6>&1 | Out-String
Assert-Contains $cudaOutput "Backend mode: cuda" "CUDA dry-run"
Assert-Contains $cudaOutput "-DSD_CUDA=ON" "CUDA dry-run"
Assert-Contains $cudaOutput "System GGML: ON" "CUDA dry-run"

$vulkanOutput = & $buildScript -DryRun -SkipSourceRefresh -Vulkan 2>&1 6>&1 | Out-String
Assert-Contains $vulkanOutput "Backend mode: vulkan" "Vulkan dry-run"
Assert-Contains $vulkanOutput "-DSD_VULKAN=ON" "Vulkan dry-run"
Assert-Contains $vulkanOutput "System GGML: ON" "Vulkan dry-run"

$systemOutput = & $buildScript `
	-DryRun `
	-SkipSourceRefresh `
	-Cuda `
	-UseSystemGgml `
	-OfxGgmlPath $coreRoot 2>&1 6>&1 | Out-String
Assert-Contains $systemOutput "Backend mode: cuda" "system GGML dry-run"
Assert-Contains $systemOutput "System GGML: ON" "system GGML dry-run"
Assert-Contains $systemOutput "-DSD_USE_SYSTEM_GGML=ON" "system GGML dry-run"
Assert-Contains $systemOutput $coreRoot "system GGML dry-run"

$bundledOutput = & $buildScript -DryRun -SkipSourceRefresh -CpuOnly -UseBundledGgml 2>&1 6>&1 | Out-String
Assert-Contains $bundledOutput "Backend mode: cpu-only" "bundled GGML fallback dry-run"
Assert-Contains $bundledOutput "System GGML: OFF (using bundled)" "bundled GGML fallback dry-run"

$coreRefreshOutput = & $buildScript `
	-DryRun `
	-Cuda `
	-SourceReleaseTag master-813-bfbef5b 2>&1 6>&1 | Out-String
Assert-Contains $coreRefreshOutput "Release tag: master-813-bfbef5b" "Core source refresh dry-run"
Assert-NotContains $coreRefreshOutput "Refreshing ggml source" "Core source refresh dry-run"
Assert-NotContains $coreRefreshOutput "GGUF dimension compatibility patch" "Core source refresh dry-run"

$bundledRefreshOutput = & $buildScript `
	-DryRun `
	-Cuda `
	-UseBundledGgml `
	-SourceReleaseTag master-813-bfbef5b `
	-GgmlReleaseTag v0.19.0 2>&1 6>&1 | Out-String
Assert-Contains $bundledRefreshOutput "Refreshing ggml source" "bundled ggml pin dry-run"
Assert-Contains $bundledRefreshOutput "Release tag: v0.19.0" "bundled ggml pin dry-run"
Assert-Contains $bundledRefreshOutput "GGUF dimension compatibility patch" "bundled ggml pin dry-run"

$systemPinRejected = $false
try {
	& $buildScript -DryRun -Cuda -UseSystemGgml -GgmlReleaseTag v0.19.0 2>&1 6>&1 | Out-Null
} catch {
	$systemPinRejected = $_.Exception.Message.Contains("only applies to -UseBundledGgml")
}
if (-not $systemPinRejected) {
	throw "System ggml lane accepted a redundant -GgmlReleaseTag override"
}

Write-Host "==> Stable Diffusion build dry-run coverage passed"
