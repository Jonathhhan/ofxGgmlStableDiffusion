param(
	[string]$Model = $(if ($env:OFXGGML_STABLE_DIFFUSION_VIDEO_MODEL) { $env:OFXGGML_STABLE_DIFFUSION_VIDEO_MODEL } else { "" }),
	[string]$TextEncoder = $(if ($env:OFXGGML_STABLE_DIFFUSION_TEXT_ENCODER) { $env:OFXGGML_STABLE_DIFFUSION_TEXT_ENCODER } elseif ($env:OFXGGML_STABLE_DIFFUSION_T5XXL) { $env:OFXGGML_STABLE_DIFFUSION_T5XXL } else { "" }),
	[string]$Vae = $(if ($env:OFXGGML_STABLE_DIFFUSION_VAE) { $env:OFXGGML_STABLE_DIFFUSION_VAE } else { "" }),
	[string]$Exe = "",
	[int]$TimeoutSeconds = 900,
	[switch]$DryRun
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
	param(
		[string]$Path,
		[string]$Name
	)
	if ([string]::IsNullOrWhiteSpace($Path) -or !(Test-Path -LiteralPath $Path -PathType Leaf)) {
		throw "$Name was not found: $Path"
	}
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$addonRoot = (Resolve-Path (Join-Path $scriptRoot "..")).Path

if ([string]::IsNullOrWhiteSpace($Exe)) {
	$Exe = Join-Path $addonRoot "ofxGgmlStableDiffusionVideoGenerationExample\bin\ofxGgmlStableDiffusionVideoGenerationExample.exe"
}

$resolvedExe = Resolve-SmokePath $Exe
$resolvedModel = Resolve-SmokePath $Model
$resolvedTextEncoder = Resolve-SmokePath $TextEncoder
$resolvedVae = Resolve-SmokePath $Vae

$plan = [ordered]@{
	Name = "WAN context smoke"
	Exe = $resolvedExe
	Model = $resolvedModel
	TextEncoder = $resolvedTextEncoder
	Vae = $resolvedVae
	TimeoutSeconds = $TimeoutSeconds
	ContextSmokeEnv = "OFXGGML_STABLE_DIFFUSION_CONTEXT_SMOKE=1"
}

if ($DryRun) {
	$plan | ConvertTo-Json -Depth 4
	return
}

Require-File $resolvedExe "VideoGenerationExample executable"
Require-File $resolvedModel "WAN diffusion model"
Require-File $resolvedTextEncoder "UMT5 / T5XXL text encoder"
Require-File $resolvedVae "WAN VAE"

$oldVideoModel = $env:OFXGGML_STABLE_DIFFUSION_VIDEO_MODEL
$oldTextEncoder = $env:OFXGGML_STABLE_DIFFUSION_TEXT_ENCODER
$oldT5xxl = $env:OFXGGML_STABLE_DIFFUSION_T5XXL
$oldVae = $env:OFXGGML_STABLE_DIFFUSION_VAE
$oldSmoke = $env:OFXGGML_STABLE_DIFFUSION_CONTEXT_SMOKE
$oldSmokeTimeout = $env:OFXGGML_STABLE_DIFFUSION_CONTEXT_SMOKE_TIMEOUT_MS
$oldSmokeStatus = $env:OFXGGML_STABLE_DIFFUSION_CONTEXT_SMOKE_STATUS

try {
	$env:OFXGGML_STABLE_DIFFUSION_VIDEO_MODEL = $resolvedModel
	$env:OFXGGML_STABLE_DIFFUSION_TEXT_ENCODER = $resolvedTextEncoder
	$env:OFXGGML_STABLE_DIFFUSION_T5XXL = $resolvedTextEncoder
	$env:OFXGGML_STABLE_DIFFUSION_VAE = $resolvedVae
	$env:OFXGGML_STABLE_DIFFUSION_CONTEXT_SMOKE = "1"
	$env:OFXGGML_STABLE_DIFFUSION_CONTEXT_SMOKE_TIMEOUT_MS = [string]($TimeoutSeconds * 1000)
	$statusPath = Join-Path ([System.IO.Path]::GetTempPath()) "ofxGgmlStableDiffusion-wan-context-smoke.status.log"
	Remove-Item -LiteralPath $statusPath -Force -ErrorAction SilentlyContinue
	$env:OFXGGML_STABLE_DIFFUSION_CONTEXT_SMOKE_STATUS = $statusPath

	Write-Host "==> Running WAN context smoke"
	Write-Host "  Exe: $resolvedExe"
	Write-Host "  Model: $resolvedModel"
	Write-Host "  TextEncoder: $resolvedTextEncoder"
	Write-Host "  VAE: $resolvedVae"

	$stdoutPath = Join-Path ([System.IO.Path]::GetTempPath()) "ofxGgmlStableDiffusion-wan-context-smoke.out.log"
	$stderrPath = Join-Path ([System.IO.Path]::GetTempPath()) "ofxGgmlStableDiffusion-wan-context-smoke.err.log"
	Remove-Item -LiteralPath $stdoutPath, $stderrPath -Force -ErrorAction SilentlyContinue

	$startInfo = New-Object System.Diagnostics.ProcessStartInfo
	$startInfo.FileName = $resolvedExe
	$startInfo.WorkingDirectory = Split-Path -Parent $resolvedExe
	$startInfo.UseShellExecute = $false
	$startInfo.RedirectStandardOutput = $true
	$startInfo.RedirectStandardError = $true
	$startInfo.CreateNoWindow = $true

	$process = New-Object System.Diagnostics.Process
	$process.StartInfo = $startInfo
	$null = $process.Start()
	$stdoutTask = $process.StandardOutput.ReadToEndAsync()
	$stderrTask = $process.StandardError.ReadToEndAsync()

	if (!$process.WaitForExit($TimeoutSeconds * 1000)) {
		$process.Kill()
		$process.WaitForExit()
		throw "WAN context smoke timed out after $TimeoutSeconds seconds."
	}
	$stdoutTask.Wait()
	$stderrTask.Wait()
	$stdout = $stdoutTask.Result
	$stderr = $stderrTask.Result
	Set-Content -LiteralPath $stdoutPath -Value $stdout
	Set-Content -LiteralPath $stderrPath -Value $stderr

	if ($process.ExitCode -ne 0) {
		if (![string]::IsNullOrWhiteSpace($stdout)) {
			Write-Host "==> stdout"
			Write-Host $stdout
		}
		if (![string]::IsNullOrWhiteSpace($stderr)) {
			Write-Host "==> stderr"
			Write-Host $stderr
		}
		if (Test-Path -LiteralPath $statusPath) {
			Write-Host "==> status"
			Get-Content -LiteralPath $statusPath | ForEach-Object { Write-Host $_ }
		}
		throw "WAN context smoke failed with exit code $($process.ExitCode). Logs: $stdoutPath ; $stderrPath ; $statusPath"
	}
	if (![string]::IsNullOrWhiteSpace($stdout)) {
		Write-Host $stdout
	}
	if (![string]::IsNullOrWhiteSpace($stderr)) {
		Write-Host $stderr
	}
	Write-Host "==> WAN context smoke passed"
} finally {
	$env:OFXGGML_STABLE_DIFFUSION_VIDEO_MODEL = $oldVideoModel
	$env:OFXGGML_STABLE_DIFFUSION_TEXT_ENCODER = $oldTextEncoder
	$env:OFXGGML_STABLE_DIFFUSION_T5XXL = $oldT5xxl
	$env:OFXGGML_STABLE_DIFFUSION_VAE = $oldVae
	$env:OFXGGML_STABLE_DIFFUSION_CONTEXT_SMOKE = $oldSmoke
	$env:OFXGGML_STABLE_DIFFUSION_CONTEXT_SMOKE_TIMEOUT_MS = $oldSmokeTimeout
	$env:OFXGGML_STABLE_DIFFUSION_CONTEXT_SMOKE_STATUS = $oldSmokeStatus
}
