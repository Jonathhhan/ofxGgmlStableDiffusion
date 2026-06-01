param(
	[string]$Model = $(if ($env:OFXGGML_STABLE_DIFFUSION_MODEL) { $env:OFXGGML_STABLE_DIFFUSION_MODEL } else { "" }),
	[string]$Backend = $(if ($env:OFXGGML_STABLE_DIFFUSION_BACKEND) { $env:OFXGGML_STABLE_DIFFUSION_BACKEND } else { "" }),
	[switch]$Json,
	[switch]$Strict
)

$ErrorActionPreference = "Stop"

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$addonRoot = Resolve-Path (Join-Path $scriptRoot "..")
$addonsRoot = Split-Path -Parent $addonRoot
$script:Warnings = 0

function New-Check {
	param(
		[string]$State,
		[string]$Name,
		[string]$Detail = ""
	)
	if ($State -eq "WARN") {
		$script:Warnings++
	}
	return [pscustomobject]@{
		State = $State
		Name = $Name
		Detail = $Detail
	}
}

function Test-CommandAvailable {
	param([string]$Name)
	return $null -ne (Get-Command $Name -ErrorAction SilentlyContinue)
}

function Test-PathCheck {
	param(
		[string]$Path,
		[string]$Name,
		[string]$MissingDetail,
		[switch]$Directory
	)
	$exists = if ($Directory) {
		Test-Path -LiteralPath $Path -PathType Container
	} else {
		Test-Path -LiteralPath $Path -PathType Leaf
	}
	if ($exists) {
		return New-Check "OK" $Name $Path
	}
	return New-Check "WARN" $Name $MissingDetail
}

function Test-ConfiguredFile {
	param(
		[string]$Path,
		[string]$Name,
		[string]$Hint
	)
	if ([string]::IsNullOrWhiteSpace($Path)) {
		return New-Check "WARN" $Name $Hint
	}
	$expanded = [Environment]::ExpandEnvironmentVariables($Path)
	if (Test-Path -LiteralPath $expanded -PathType Leaf) {
		return New-Check "OK" $Name $expanded
	}
	return New-Check "WARN" $Name "configured path was not found: $expanded"
}

function Test-ForbiddenPath {
	param([string]$RelativePath)
	$path = Join-Path $addonRoot $RelativePath
	if (Test-Path -LiteralPath $path) {
		return New-Check "WARN" "artifact hygiene" "generated/local path exists: $RelativePath"
	}
	return $null
}

function Get-NativeRuntimeCandidates {
	return @(
		"libs\stable-diffusion\bin\stable-diffusion.dll",
		"libs\stable-diffusion\lib\vs\stable-diffusion.lib",
		"libs\stable-diffusion\lib\linux64\libstable-diffusion.a",
		"libs\stable-diffusion\lib\Linux64\libstable-diffusion.a",
		"libs\stable-diffusion\lib\osx\libstable-diffusion.a"
	)
}

function Get-CoreProviderManifest {
	$coreRoot = Join-Path $addonsRoot "ofxGgmlCore"
	$manifestScript = Join-Path $coreRoot "scripts\runtime-provider-manifest.ps1"
	if (!(Test-Path -LiteralPath $manifestScript -PathType Leaf)) {
		return $null
	}
	$json = & $manifestScript -Json -SummaryOnly 2>$null
	if (!$? -or !$json) {
		return $null
	}
	return (($json | ForEach-Object { $_.ToString() }) -join "`n") | ConvertFrom-Json
}

function Get-StableDiffusionRuntimeProvider {
	param([string[]]$RuntimeMatches)
	if (@($RuntimeMatches).Count -eq 0) {
		return [pscustomobject]@{
			State = "WARN"
			Name = "runtime provider"
			Provider = "unknown"
			Detail = "native runtime is not staged yet"
		}
	}

	$cachePath = Join-Path $addonRoot "libs\stable-diffusion\build\CMakeCache.txt"
	if (Test-Path -LiteralPath $cachePath -PathType Leaf) {
		$cache = Get-Content -LiteralPath $cachePath -Raw
		if ($cache -match "(?m)^SD_USE_SYSTEM_GGML:BOOL=ON") {
			$coreManifest = Get-CoreProviderManifest
			$detail = "system ggml from ofxGgmlCore"
			if ($coreManifest) {
				$backends = @("CPU", "CUDA", "Vulkan", "Metal", "OpenCL") | Where-Object {
					$coreManifest.EnabledBackends.$_
				}
				if ($backends.Count -gt 0) {
					$detail += "; Core backends: " + ($backends -join ", ")
				}
			}
			return [pscustomobject]@{
				State = "OK"
				Name = "runtime provider"
				Provider = "system-ggml-core"
				Detail = $detail
			}
		}
		return [pscustomobject]@{
			State = "OK"
			Name = "runtime provider"
			Provider = "standalone"
			Detail = "standalone stable-diffusion.cpp runtime"
		}
	}

	return [pscustomobject]@{
		State = "OK"
		Name = "runtime provider"
		Provider = "standalone"
		Detail = "standalone runtime staged; build cache unavailable"
	}
}

$checks = @()
$checks += New-Check "OK" "addon root" $addonRoot.Path

foreach ($tool in @("git", "cmake")) {
	if (Test-CommandAvailable $tool) {
		$checks += New-Check "OK" $tool ((Get-Command $tool).Source)
	} else {
		$checks += New-Check "WARN" $tool "not found in PATH"
	}
}

$checks += Test-PathCheck `
	-Path (Join-Path $addonsRoot "ofxGgmlCore") `
	-Name "ofxGgmlCore sibling" `
	-MissingDetail "clone beside ofxGgmlStableDiffusion for ecosystem validation" `
	-Directory

$checks += Test-PathCheck `
	-Path (Join-Path $addonRoot "libs\stable-diffusion\include\stable-diffusion.h") `
	-Name "stable-diffusion header" `
	-MissingDetail "run scripts\setup_addon.ps1 or stage the native header"

$runtimeCandidates = @(Get-NativeRuntimeCandidates)
$runtimeMatches = @($runtimeCandidates | Where-Object {
	Test-Path -LiteralPath (Join-Path $addonRoot $_) -PathType Leaf
})
if ($runtimeMatches.Count -gt 0) {
	$checks += New-Check "OK" "native runtime" ($runtimeMatches -join ", ")
} else {
	$checks += New-Check "WARN" "native runtime" "run scripts\setup_addon.ps1 or scripts\build-stable-diffusion.ps1"
}

$runtimeProvider = Get-StableDiffusionRuntimeProvider -RuntimeMatches $runtimeMatches
$checks += New-Check $runtimeProvider.State $runtimeProvider.Name $runtimeProvider.Detail

$checks += Test-PathCheck `
	-Path (Join-Path $addonRoot "src\ofxGgmlStableDiffusion.h") `
	-Name "wrapper header" `
	-MissingDetail "wrapper header is missing"

$checks += Test-PathCheck `
	-Path (Join-Path $addonRoot "src\ofxGgmlStableDiffusion.cpp") `
	-Name "wrapper source" `
	-MissingDetail "wrapper source is missing"

$checks += Test-PathCheck `
	-Path (Join-Path $addonRoot "ofxGgmlStableDiffusionExample\addons.make") `
	-Name "example addon file" `
	-MissingDetail "example addon file is missing"

$checks += Test-PathCheck `
	-Path (Join-Path $addonRoot "ofxGgmlStableDiffusionBasicGenerationExample\addons.make") `
	-Name "basic generation example" `
	-MissingDetail "basic generation example skeleton is missing"

$checks += Test-PathCheck `
	-Path (Join-Path $addonRoot "ofxGgmlStableDiffusionImageWorkflowExample\addons.make") `
	-Name "image workflow example" `
	-MissingDetail "image workflow example skeleton is missing"

$checks += Test-PathCheck `
	-Path (Join-Path $addonRoot "ofxGgmlStableDiffusionVideoGenerationExample\addons.make") `
	-Name "video generation example" `
	-MissingDetail "video generation example skeleton is missing"

$checks += Test-PathCheck `
	-Path (Join-Path $addonRoot "ofxGgmlStableDiffusionVideoControlFramesExample\addons.make") `
	-Name "video control frames example" `
	-MissingDetail "video control frames example skeleton is missing"

$checks += Test-PathCheck `
	-Path (Join-Path $addonRoot "ofxGgmlStableDiffusionCreativeLoopExample\addons.make") `
	-Name "creative loop example" `
	-MissingDetail "creative loop example skeleton is missing"

$checks += Test-PathCheck `
	-Path (Join-Path $addonRoot "ofxGgmlStableDiffusionLoraEmbeddingExample\addons.make") `
	-Name "LoRA embedding example" `
	-MissingDetail "LoRA embedding example skeleton is missing"

$checks += Test-PathCheck `
	-Path (Join-Path $addonsRoot "ofxImGui") `
	-Name "example ImGui addon" `
	-MissingDetail "ofxImGui sibling addon is missing for examples" `
	-Directory

if ([string]::IsNullOrWhiteSpace($Backend)) {
	$checks += New-Check "WARN" "backend selection" "set OFXGGML_STABLE_DIFFUSION_BACKEND or pass -Backend"
} else {
	$checks += New-Check "OK" "backend selection" $Backend
}

$checks += Test-ConfiguredFile `
	-Path $Model `
	-Name "diffusion model" `
	-Hint "set OFXGGML_STABLE_DIFFUSION_MODEL or pass -Model for a real render smoke"

$exampleNames = @(
	"ofxGgmlStableDiffusionExample",
	"ofxGgmlStableDiffusionBasicGenerationExample",
	"ofxGgmlStableDiffusionImageWorkflowExample",
	"ofxGgmlStableDiffusionVideoGenerationExample",
	"ofxGgmlStableDiffusionVideoControlFramesExample",
	"ofxGgmlStableDiffusionCreativeLoopExample",
	"ofxGgmlStableDiffusionLoraEmbeddingExample"
)
$exampleArtifactSuffixes = @(
	"bin\data\generated",
	"bin\data\output",
	"bin\data\outputs",
	"bin\data\renders",
	"bin\data\videos"
)

$artifactPaths = @(
	"build",
	".vs",
	"tests\build",
	"libs\stable-diffusion\build",
	"libs\stable-diffusion\downloads"
)
foreach ($exampleName in $exampleNames) {
	foreach ($suffix in $exampleArtifactSuffixes) {
		$artifactPaths += Join-Path $exampleName $suffix
	}
}

$artifactWarnings = @()
foreach ($relative in $artifactPaths) {
	$warning = Test-ForbiddenPath -RelativePath $relative
	if ($null -ne $warning) {
		$artifactWarnings += $warning
	}
}
if ($artifactWarnings.Count -eq 0) {
	$checks += New-Check "OK" "artifact hygiene" "no generated/local paths detected"
} else {
	$checks += $artifactWarnings
}

if ($Json) {
	[pscustomobject]@{
		Root = $addonRoot.Path
		Warnings = $script:Warnings
		Checks = $checks
		NativeRuntimeCandidates = $runtimeCandidates
		RuntimeProvider = $runtimeProvider.Provider
	} | ConvertTo-Json -Depth 6
} else {
	Write-Host "ofxGgmlStableDiffusion doctor"
	Write-Host "Root  $addonRoot"
	Write-Host ""
	foreach ($check in $checks) {
		$line = "{0,-5} {1}" -f $check.State, $check.Name
		if (![string]::IsNullOrWhiteSpace($check.Detail)) {
			$line += " - $($check.Detail)"
		}
		Write-Host $line
	}
	Write-Host ""
	if ($script:Warnings -eq 0) {
		Write-Host "Doctor passed."
	} else {
		Write-Host "Doctor found $script:Warnings warning(s)."
	}
}

if ($Strict -and $script:Warnings -gt 0) {
	exit 1
}
