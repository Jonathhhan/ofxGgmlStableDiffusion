param(
	[string]$Configuration = "Release",
	[string]$BuildDir = "",
	[string]$Model = $(if ($env:OFXGGML_STABLE_DIFFUSION_MODEL) { $env:OFXGGML_STABLE_DIFFUSION_MODEL } else { "" }),
	[ValidateSet("cpu", "cuda", "vulkan", "metal")]
	[string]$Backend = $(if ($env:OFXGGML_STABLE_DIFFUSION_BACKEND) { $env:OFXGGML_STABLE_DIFFUSION_BACKEND } else { "cpu" }),
	[string]$OutputPath = "",
	[string]$Prompt = "a simple red circle on a white background",
	[int]$Width = 256,
	[int]$Height = 256,
	[int]$Steps = 1,
	[int]$Seed = 1,
	[switch]$Clean,
	[switch]$DryRun,
	[switch]$Json,
	[switch]$SummaryOnly,
	[switch]$RequireModel,
	[switch]$InferenceOnly
)

$ErrorActionPreference = "Stop"

function Write-Step {
	param([string]$Message)
	if (!$Json) {
		Write-Host "==> $Message"
	}
}

function Get-PowerShellExecutable {
	$pwsh = Get-Command pwsh -ErrorAction SilentlyContinue
	if ($pwsh) {
		return $pwsh.Source
	}
	$windowsPowerShell = Get-Command powershell -ErrorAction SilentlyContinue
	if ($windowsPowerShell) {
		return $windowsPowerShell.Source
	}
	throw "Could not find pwsh or powershell."
}

function Write-SmokeOutputPath {
	param(
		[string]$Path,
		[string]$Content
	)
	if ([string]::IsNullOrWhiteSpace($Path)) {
		return
	}
	$target = if ([System.IO.Path]::IsPathRooted($Path)) {
		$Path
	} else {
		Join-Path $addonRoot $Path
	}
	$directory = Split-Path -Parent $target
	if (!(Test-Path -LiteralPath $directory -PathType Container)) {
		New-Item -ItemType Directory -Path $directory -Force | Out-Null
	}
	Set-Content -LiteralPath $target -Value $Content
}

function Resolve-SmokeModel {
	param([string]$ConfiguredModel)
	if (![string]::IsNullOrWhiteSpace($ConfiguredModel)) {
		$expanded = [Environment]::ExpandEnvironmentVariables($ConfiguredModel)
		if ([System.IO.Path]::IsPathRooted($expanded)) {
			return [System.IO.Path]::GetFullPath($expanded)
		}
		return [System.IO.Path]::GetFullPath((Join-Path $addonRoot $expanded))
	}
	$roots = @(
		(Join-Path $addonRoot "ofxGgmlStableDiffusionExample\bin\data\models"),
		(Join-Path $addonRoot "models"),
		(Join-Path (Split-Path -Parent $addonRoot) "models")
	)
	foreach ($root in $roots) {
		if (!(Test-Path -LiteralPath $root -PathType Container)) {
			continue
		}
		$candidate = Get-ChildItem -LiteralPath $root -Recurse -File -ErrorAction SilentlyContinue |
			Where-Object { $_.Extension -in @(".safetensors", ".ckpt") } |
			Sort-Object Length, Name |
			Select-Object -First 1
		if ($candidate) {
			return $candidate.FullName
		}
	}
	return ""
}

function Resolve-SmokeCli {
	$candidates = @(
		(Join-Path $addonRoot "libs\stable-diffusion\build\bin\Release\sd-cli.exe"),
		(Join-Path $addonRoot "libs\stable-diffusion\bin\vs\sd-cli.exe"),
		(Join-Path $addonRoot "libs\variants\$Backend\stable-diffusion\bin\vs\sd-cli.exe")
	)
	foreach ($candidate in $candidates) {
		if (Test-Path -LiteralPath $candidate -PathType Leaf) {
			return $candidate
		}
	}
	return ""
}

function Test-GeneratedBuildDir {
	param([string]$Path)
	if ([string]::IsNullOrWhiteSpace($Path)) {
		return $false
	}
	$fullPath = [System.IO.Path]::GetFullPath($Path)
	$tempRoot = [System.IO.Path]::GetFullPath([System.IO.Path]::GetTempPath()).TrimEnd("\", "/") + [System.IO.Path]::DirectorySeparatorChar
	$addonBuildRoot = [System.IO.Path]::GetFullPath((Join-Path $addonRoot "build")).TrimEnd("\", "/") + [System.IO.Path]::DirectorySeparatorChar
	return $fullPath.StartsWith($tempRoot, [System.StringComparison]::OrdinalIgnoreCase) -or
		$fullPath.StartsWith($addonBuildRoot, [System.StringComparison]::OrdinalIgnoreCase)
}

function Invoke-SmokeStep {
	param(
		[string]$Name,
		[string[]]$Arguments
	)
	$output = @()
	$exitCode = 0
	try {
		$output = & $powerShell @Arguments 2>&1 | ForEach-Object { "$_" }
		$exitCode = $LASTEXITCODE
	} catch {
		$output += "$_"
		$exitCode = 1
	}
	return [ordered]@{
		Name = $Name
		Passed = ($exitCode -eq 0)
		ExitCode = $exitCode
		Output = $output
	}
}

function Invoke-ModelInference {
	param(
		[string]$Executable,
		[string]$ModelPath,
		[string]$ImagePath
	)
	$arguments = @(
		"--backend", $Backend,
		"-m", $ModelPath,
		"-p", $Prompt,
		"-W", [string]$Width,
		"-H", [string]$Height,
		"--steps", [string]$Steps,
		"--cfg-scale", "1",
		"--sampling-method", "euler",
		"-s", [string]$Seed,
		"-o", $ImagePath
	)
	$output = @()
	$exitCode = 1
	$started = Get-Date
	$previousErrorActionPreference = $ErrorActionPreference
	try {
		$ErrorActionPreference = "Continue"
		$output = & $Executable @arguments 2>&1 | ForEach-Object { "$_" }
		$exitCode = $LASTEXITCODE
	} catch {
		$output += "$_"
	} finally {
		$ErrorActionPreference = $previousErrorActionPreference
	}
	$imageReady = Test-Path -LiteralPath $ImagePath -PathType Leaf
	$outputBytes = if ($imageReady) { (Get-Item -LiteralPath $ImagePath).Length } else { 0 }
	return [ordered]@{
		Name = "Stable Diffusion model inference"
		Passed = ($exitCode -eq 0 -and $outputBytes -ge 1000)
		ExitCode = $exitCode
		Output = $output
		OutputPath = $ImagePath
		OutputBytes = $outputBytes
		ElapsedMs = [int]((Get-Date) - $started).TotalMilliseconds
	}
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$addonRoot = (Resolve-Path (Join-Path $scriptRoot "..")).Path
$doctorScript = Join-Path $scriptRoot "doctor-stable-diffusion.ps1"
$testScript = Join-Path $scriptRoot "run-tests.ps1"
$buildDryRunScript = Join-Path $scriptRoot "test-build-stable-diffusion-dry-run.ps1"
$resolvedModel = Resolve-SmokeModel -ConfiguredModel $Model
$resolvedCli = Resolve-SmokeCli
$cliReady = ![string]::IsNullOrWhiteSpace($resolvedCli)
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
	$BuildDir = Join-Path ([System.IO.Path]::GetTempPath()) "ofxGgmlStableDiffusion-runtime-smoke"
}

$runtimeCandidates = @(
	"libs\stable-diffusion\bin\stable-diffusion.dll",
	"libs\stable-diffusion\lib\vs\stable-diffusion.lib",
	"libs\stable-diffusion\lib\Linux64\libstable-diffusion.so",
	"libs\stable-diffusion\lib\linux64\libstable-diffusion.a",
	"libs\stable-diffusion\lib\osx\libstable-diffusion.dylib"
)
$runtimeMatches = @($runtimeCandidates | Where-Object {
	Test-Path -LiteralPath (Join-Path $addonRoot $_) -PathType Leaf
})
$ready = (Test-Path -LiteralPath (Join-Path $addonRoot "libs\stable-diffusion\include\stable-diffusion.h") -PathType Leaf) -and
	(Test-Path -LiteralPath (Join-Path $addonRoot "src\ofxGgmlStableDiffusion.h") -PathType Leaf) -and
	(Test-Path -LiteralPath $doctorScript -PathType Leaf) -and
	(Test-Path -LiteralPath $testScript -PathType Leaf)
$modelReady = ![string]::IsNullOrWhiteSpace($resolvedModel) -and
	(Test-Path -LiteralPath $resolvedModel -PathType Leaf)

$plan = [ordered]@{
	Name = "ofxGgmlStableDiffusion runtime smoke"
	Root = $addonRoot
	Backend = $Backend
	BuildDir = $BuildDir
	ModelPath = $(if ($modelReady) { $resolvedModel } else { "<not-configured>" })
	Ready = [bool]$ready
	ModelReady = [bool]$modelReady
	CliPath = $(if ($cliReady) { $resolvedCli } else { "<not-found>" })
	CliReady = [bool]$cliReady
	ModelBacked = $false
	RuntimeMatches = @($runtimeMatches)
	SmokeKind = "stable-diffusion-wrapper-boundary"
	InferenceChecked = $false
	NextCommands = @(
		"scripts\run-stable-diffusion-runtime-smoke.bat -DryRun",
		"scripts\run-stable-diffusion-runtime-smoke.bat -Json -SummaryOnly",
		"scripts\run-stable-diffusion-runtime-smoke.bat -Json -SummaryOnly -OutputPath .stable-diffusion-runtime-smoke.json",
		"scripts\run-stable-diffusion-runtime-smoke.bat -RequireModel -Json -SummaryOnly -OutputPath .stable-diffusion-runtime-smoke.json",
		"scripts\run-stable-diffusion-runtime-smoke.bat -InferenceOnly -Model C:\path\to\model.safetensors -Backend cpu -Json -SummaryOnly"
	)
}

if ($DryRun) {
	if ($Json) {
		$content = $plan | ConvertTo-Json -Depth 6
		Write-SmokeOutputPath -Path $OutputPath -Content $content
		$content
		return
	}
	Write-Step "ofxGgmlStableDiffusion runtime smoke plan"
	Write-Host "  Backend: $($plan.Backend)"
	Write-Host "  BuildDir: $($plan.BuildDir)"
	Write-Host "  Ready: $($plan.Ready)"
	Write-Host "  ModelReady: $($plan.ModelReady)"
	Write-Host "  ModelPath: $($plan.ModelPath)"
	Write-Host "  Next: $($plan.NextCommands[1])"
	return
}

if (($RequireModel -or $InferenceOnly) -and -not $modelReady) {
	throw "No Stable Diffusion model was found. Set OFXGGML_STABLE_DIFFUSION_MODEL or pass -Model."
}
if ($Clean -and (Test-GeneratedBuildDir -Path $BuildDir) -and (Test-Path -LiteralPath $BuildDir)) {
	Remove-Item -LiteralPath $BuildDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

$started = Get-Date
$powerShell = Get-PowerShellExecutable
$results = @()
if (!$InferenceOnly) {
	$results += Invoke-SmokeStep -Name "Stable Diffusion doctor" -Arguments @(
		"-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $doctorScript, "-Json"
	)
	$results += Invoke-SmokeStep -Name "Stable Diffusion build dry-runs" -Arguments @(
		"-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $buildDryRunScript
	)
	$results += Invoke-SmokeStep -Name "Stable Diffusion wrapper tests" -Arguments @(
		"-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $testScript, "-Configuration", $Configuration, "-BuildDir", (Join-Path $BuildDir "tests")
	)
}
$modelInference = $null
if ($modelReady) {
	$modelOutputPath = Join-Path $BuildDir "stable-diffusion-smoke.png"
	if ($cliReady) {
		$modelInference = Invoke-ModelInference -Executable $resolvedCli -ModelPath $resolvedModel -ImagePath $modelOutputPath
	} else {
		$modelInference = [ordered]@{
			Name = "Stable Diffusion model inference"
			Passed = $false
			ExitCode = 1
			Output = @("Stable Diffusion CLI was not found.")
			OutputPath = $modelOutputPath
			OutputBytes = 0
			ElapsedMs = 0
		}
	}
	$results += $modelInference
}

$failed = @($results | Where-Object { -not $_.Passed })
$inferenceChecked = ($null -ne $modelInference -and [bool]$modelInference.Passed)
$elapsedMs = [int]((Get-Date) - $started).TotalMilliseconds
$summary = [ordered]@{
	Name = "ofxGgmlStableDiffusion runtime smoke"
	Passed = ($failed.Count -eq 0)
	InferenceChecked = [bool]$inferenceChecked
	SmokeKind = $(if ($inferenceChecked) { "model-backed-cli-image" } else { "stable-diffusion-wrapper-boundary" })
	Backend = $Backend
	RuntimeProvider = "stable-diffusion.cpp"
	ModelPath = $(if ($modelReady) { $resolvedModel } else { "<not-configured>" })
	Configuration = $Configuration
	BuildDir = $BuildDir
	ModelBacked = [bool]$inferenceChecked
	InferenceOutputPath = $(if ($null -ne $modelInference) { [string]$modelInference.OutputPath } else { "" })
	InferenceOutputBytes = $(if ($null -ne $modelInference) { [long]$modelInference.OutputBytes } else { 0 })
	RuntimeMatched = ($runtimeMatches.Count -gt 0)
	RuntimeMatches = @($runtimeMatches)
	ResultCount = $results.Count
	FailedCount = $failed.Count
	ElapsedMs = $elapsedMs
	Error = $(if ($failed.Count -eq 0) { "" } else { (($failed | ForEach-Object { $_.Output }) -join "`n") })
}

if ($Json) {
	$payload = if ($SummaryOnly) {
		[ordered]@{
			Name = [string]$summary.Name
			Summary = $summary
			NextCommands = @($plan.NextCommands)
		}
	} else {
		[ordered]@{
			Summary = $summary
			Results = $results
			NextCommands = @($plan.NextCommands)
		}
	}
	$content = $payload | ConvertTo-Json -Depth 7
	Write-SmokeOutputPath -Path $OutputPath -Content $content
	$content
} else {
	foreach ($result in $results) {
		Write-Step $result.Name
		foreach ($line in $result.Output) {
			Write-Host $line
		}
	}
	Write-Step "ofxGgmlStableDiffusion runtime smoke summary"
	Write-Host "  Backend: $($summary.Backend)"
	Write-Host "  ModelBacked: $($summary.ModelBacked)"
	Write-Host "  InferenceChecked: $($summary.InferenceChecked)"
	Write-Host "  Passed: $($summary.Passed)"
	Write-Host "  ElapsedMs: $($summary.ElapsedMs)"
}

if ($failed.Count -gt 0) {
	exit 1
}
