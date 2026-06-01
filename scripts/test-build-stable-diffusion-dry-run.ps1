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

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$addonRoot = (Resolve-Path (Join-Path $scriptRoot "..")).Path
$addonsRoot = Split-Path -Parent $addonRoot
$buildScript = Join-Path $scriptRoot "build-stable-diffusion.ps1"
$coreRoot = Join-Path $addonsRoot "ofxGgmlCore"

$cpuOutput = & $buildScript -DryRun -SkipSourceRefresh -CpuOnly 2>&1 6>&1 | Out-String
Assert-Contains $cpuOutput "Backend mode: cpu-only" "CPU dry-run"
Assert-Contains $cpuOutput "System GGML: OFF (using bundled)" "CPU dry-run"
Assert-Contains $cpuOutput "-DSD_CUDA=OFF" "CPU dry-run"

$cudaOutput = & $buildScript -DryRun -SkipSourceRefresh -Cuda 2>&1 6>&1 | Out-String
Assert-Contains $cudaOutput "Backend mode: cuda" "CUDA dry-run"
Assert-Contains $cudaOutput "-DSD_CUDA=ON" "CUDA dry-run"
Assert-Contains $cudaOutput "System GGML: OFF (using bundled)" "CUDA dry-run"

$vulkanOutput = & $buildScript -DryRun -SkipSourceRefresh -Vulkan 2>&1 6>&1 | Out-String
Assert-Contains $vulkanOutput "Backend mode: vulkan" "Vulkan dry-run"
Assert-Contains $vulkanOutput "-DSD_VULKAN=ON" "Vulkan dry-run"
Assert-Contains $vulkanOutput "System GGML: OFF (using bundled)" "Vulkan dry-run"

if ((Test-Path -LiteralPath $coreRoot -PathType Container) -and
	(Test-Path -LiteralPath (Join-Path $coreRoot "libs\ggml\lib\vs\ggml.lib") -PathType Leaf)) {
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
} elseif (Test-Path -LiteralPath $coreRoot -PathType Container) {
	Write-Host "==> Skipping system GGML dry-run; ofxGgmlCore ggml libs are not staged"
}

Write-Host "==> Stable Diffusion build dry-run coverage passed"
