param(
	[string]$Model = $(if ($env:OFXGGML_STABLE_DIFFUSION_MODEL) { $env:OFXGGML_STABLE_DIFFUSION_MODEL } else { "" }),
	[ValidateSet("cpu", "cuda", "vulkan", "metal")]
	[string]$Backend = "cuda",
	[string]$Prompt = "a small red robot in a Berlin studio, cinematic photograph",
	[string]$OutputPath = "",
	[string]$Exe = "",
	[int]$TimeoutSeconds = 120,
	[switch]$DryRun,
	[switch]$Json
)

$ErrorActionPreference = "Stop"

function Resolve-SmokePath {
	param([string]$Path)
	if ([string]::IsNullOrWhiteSpace($Path)) {
		return ""
	}
	$expanded = [Environment]::ExpandEnvironmentVariables($Path)
	if ([System.IO.Path]::IsPathRooted($expanded)) {
		return [System.IO.Path]::GetFullPath($expanded)
	}
	return [System.IO.Path]::GetFullPath((Join-Path $addonRoot $expanded))
}

function Require-File {
	param([string]$Path, [string]$Name)
	if ([string]::IsNullOrWhiteSpace($Path) -or
		!(Test-Path -LiteralPath $Path -PathType Leaf)) {
		throw "$Name was not found: $Path"
	}
}

function Test-PngSignature {
	param([string]$Path)
	if (!(Test-Path -LiteralPath $Path -PathType Leaf)) {
		return $false
	}
	$expected = @(0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a)
	$actual = @(Get-Content -LiteralPath $Path -Encoding Byte -TotalCount 8)
	if ($actual.Count -ne $expected.Count) {
		return $false
	}
	for ($i = 0; $i -lt $expected.Count; ++$i) {
		if ($actual[$i] -ne $expected[$i]) {
			return $false
		}
	}
	return $true
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$addonRoot = (Resolve-Path (Join-Path $scriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($Exe)) {
	$Exe = Join-Path $addonRoot "ofxGgmlStableDiffusionExample\bin\ofxGgmlStableDiffusionExample.exe"
}
if ([string]::IsNullOrWhiteSpace($OutputPath)) {
	$OutputPath = Join-Path ([System.IO.Path]::GetTempPath()) "ofxGgmlStableDiffusion-image-smoke.png"
}

$resolvedExe = Resolve-SmokePath $Exe
$resolvedModel = Resolve-SmokePath $Model
$resolvedOutput = Resolve-SmokePath $OutputPath
$statusPath = Join-Path ([System.IO.Path]::GetTempPath()) "ofxGgmlStableDiffusion-image-smoke.status.log"
$stdoutPath = Join-Path ([System.IO.Path]::GetTempPath()) "ofxGgmlStableDiffusion-image-smoke.out.log"
$stderrPath = Join-Path ([System.IO.Path]::GetTempPath()) "ofxGgmlStableDiffusion-image-smoke.err.log"

$plan = [ordered]@{
	Name = "ofxGgmlStableDiffusion openFrameworks image generation smoke"
	Exe = $resolvedExe
	Model = $resolvedModel
	Backend = $Backend
	Prompt = $Prompt
	OutputPath = $resolvedOutput
	TimeoutSeconds = $TimeoutSeconds
	SmokeEnv = "OFXGGML_STABLE_DIFFUSION_IMAGE_SMOKE=1"
}
if ($DryRun) {
	$plan | ConvertTo-Json -Depth 4
	return
}

Require-File $resolvedExe "Starter example executable"
Require-File $resolvedModel "Stable Diffusion image model"
Remove-Item -LiteralPath $resolvedOutput, $statusPath, $stdoutPath, $stderrPath -Force -ErrorAction SilentlyContinue

$oldModel = $env:OFXGGML_STABLE_DIFFUSION_MODEL
$oldBackend = $env:OFXGGML_STABLE_DIFFUSION_BACKEND
$oldSmoke = $env:OFXGGML_STABLE_DIFFUSION_IMAGE_SMOKE
$oldTimeout = $env:OFXGGML_STABLE_DIFFUSION_IMAGE_SMOKE_TIMEOUT_MS
$oldStatus = $env:OFXGGML_STABLE_DIFFUSION_IMAGE_SMOKE_STATUS
$oldOutput = $env:OFXGGML_STABLE_DIFFUSION_IMAGE_SMOKE_OUTPUT
$oldPrompt = $env:OFXGGML_STABLE_DIFFUSION_IMAGE_SMOKE_PROMPT

try {
	$env:OFXGGML_STABLE_DIFFUSION_MODEL = $resolvedModel
	$env:OFXGGML_STABLE_DIFFUSION_BACKEND = $Backend
	$env:OFXGGML_STABLE_DIFFUSION_IMAGE_SMOKE = "1"
	$env:OFXGGML_STABLE_DIFFUSION_IMAGE_SMOKE_TIMEOUT_MS = [string]($TimeoutSeconds * 1000)
	$env:OFXGGML_STABLE_DIFFUSION_IMAGE_SMOKE_STATUS = $statusPath
	$env:OFXGGML_STABLE_DIFFUSION_IMAGE_SMOKE_OUTPUT = $resolvedOutput
	$env:OFXGGML_STABLE_DIFFUSION_IMAGE_SMOKE_PROMPT = $Prompt

	$startInfo = New-Object System.Diagnostics.ProcessStartInfo
	$startInfo.FileName = $resolvedExe
	$startInfo.WorkingDirectory = Split-Path -Parent $resolvedExe
	$startInfo.UseShellExecute = $false
	$startInfo.RedirectStandardOutput = $true
	$startInfo.RedirectStandardError = $true
	$startInfo.CreateNoWindow = $true

	$started = Get-Date
	$process = New-Object System.Diagnostics.Process
	$process.StartInfo = $startInfo
	$null = $process.Start()
	$stdoutTask = $process.StandardOutput.ReadToEndAsync()
	$stderrTask = $process.StandardError.ReadToEndAsync()
	if (!$process.WaitForExit($TimeoutSeconds * 1000)) {
		$process.Kill()
		$process.WaitForExit()
		throw "Image generation smoke timed out after $TimeoutSeconds seconds."
	}
	$stdoutTask.Wait()
	$stderrTask.Wait()
	$stdout = $stdoutTask.Result
	$stderr = $stderrTask.Result
	Set-Content -LiteralPath $stdoutPath -Value $stdout
	Set-Content -LiteralPath $stderrPath -Value $stderr

	$outputBytes = if (Test-Path -LiteralPath $resolvedOutput) {
		(Get-Item -LiteralPath $resolvedOutput).Length
	} else {
		0
	}
	$pngReady = $outputBytes -ge 1000 -and (Test-PngSignature $resolvedOutput)
	$status = if (Test-Path -LiteralPath $statusPath) {
		@(Get-Content -LiteralPath $statusPath | ForEach-Object { "$_" })
	} else {
		@()
	}
	$combinedOutput = $stdout + "`n" + $stderr
	$backendObserved = switch ($Backend) {
		"cuda" { $combinedOutput -match 'Initializing backend:\s*CUDA|ggml_cuda_init:\s*found' }
		"vulkan" { $combinedOutput -match 'Initializing backend:\s*Vulkan|ggml_vulkan' }
		"metal" { $combinedOutput -match 'Initializing backend:\s*Metal|ggml_metal' }
		default { $combinedOutput -match 'Initializing backend:\s*CPU|using CPU backend' }
	}
	$passed = $process.ExitCode -eq 0 -and $pngReady -and $backendObserved -and
		($status -match '^finish:0:')
	$result = [ordered]@{
		Name = $plan.Name
		Passed = [bool]$passed
		ExitCode = $process.ExitCode
		Backend = $Backend
		BackendObserved = [bool]$backendObserved
		ModelPath = $resolvedModel
		OutputPath = $resolvedOutput
		OutputBytes = [long]$outputBytes
		PngValidated = [bool]$pngReady
		ElapsedMs = [int]((Get-Date) - $started).TotalMilliseconds
		Status = $status
		StdoutPath = $stdoutPath
		StderrPath = $stderrPath
	}
	if ($Json) {
		$result | ConvertTo-Json -Depth 5
	} else {
		Write-Host "==> openFrameworks image generation smoke"
		Write-Host "  Passed: $($result.Passed)"
		Write-Host "  Backend: $($result.Backend)"
		Write-Host "  Output: $($result.OutputPath) ($($result.OutputBytes) bytes)"
		Write-Host "  ElapsedMs: $($result.ElapsedMs)"
		$status | ForEach-Object { Write-Host "  Status: $_" }
	}
	if (!$passed) {
		if (![string]::IsNullOrWhiteSpace($stdout)) { Write-Host $stdout }
		if (![string]::IsNullOrWhiteSpace($stderr)) { Write-Host $stderr }
		throw "Image generation smoke failed. Logs: $stdoutPath ; $stderrPath ; $statusPath"
	}
} finally {
	$env:OFXGGML_STABLE_DIFFUSION_MODEL = $oldModel
	$env:OFXGGML_STABLE_DIFFUSION_BACKEND = $oldBackend
	$env:OFXGGML_STABLE_DIFFUSION_IMAGE_SMOKE = $oldSmoke
	$env:OFXGGML_STABLE_DIFFUSION_IMAGE_SMOKE_TIMEOUT_MS = $oldTimeout
	$env:OFXGGML_STABLE_DIFFUSION_IMAGE_SMOKE_STATUS = $oldStatus
	$env:OFXGGML_STABLE_DIFFUSION_IMAGE_SMOKE_OUTPUT = $oldOutput
	$env:OFXGGML_STABLE_DIFFUSION_IMAGE_SMOKE_PROMPT = $oldPrompt
}
