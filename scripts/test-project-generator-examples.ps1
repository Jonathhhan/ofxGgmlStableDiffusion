param(
    [string]$Platform = "vs",
    [string]$WorkDir = "",
    [switch]$KeepWorkDir
)

$ErrorActionPreference = "Stop"

function Write-Step {
    param([string]$Message)
    Write-Host "==> $Message"
}

function Convert-ToPgOption {
    param(
        [string]$Name,
        [string]$Value
    )
    return "-$Name$Value"
}

function Invoke-ProjectGenerator {
    param(
        [string]$ProjectGenerator,
        [string]$OfRoot,
        [string]$PlatformName,
        [string]$ProjectPath
    )

    $pgOfRoot = Convert-ToShortPath $OfRoot
    $pgProjectPath = Convert-ToShortPath $ProjectPath

    & $ProjectGenerator `
        (Convert-ToPgOption "o" $pgOfRoot) `
        (Convert-ToPgOption "p" $PlatformName) `
        $pgProjectPath
    if ($LASTEXITCODE -ne 0) {
        throw "projectGenerator failed for $ProjectPath with exit code $LASTEXITCODE"
    }
}

function Convert-ToShortPath {
    param([string]$Path)

    $resolvedPath = (Resolve-Path -LiteralPath $Path).Path
    $shortPath = & cmd.exe /d /c "for %I in (`"$resolvedPath`") do @echo %~sI"
    $shortPath = ($shortPath | Select-Object -First 1).Trim()
    if ([string]::IsNullOrWhiteSpace($shortPath)) {
        return $resolvedPath
    }
    return $shortPath
}

function Assert-GeneratedProjectContains {
    param(
        [string]$ProjectFile,
        [string]$Needle,
        [string]$Description
    )
    $content = Get-Content -LiteralPath $ProjectFile -Raw
    if ($content -notlike "*$Needle*") {
        throw "$Description was not found in $ProjectFile"
    }
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$addonRoot = (Resolve-Path (Join-Path $scriptRoot "..")).Path
$ofRoot = (Resolve-Path (Join-Path $addonRoot "..\..")).Path
$projectGenerator = Join-Path $ofRoot "projectGenerator\resources\app\app\projectGenerator.exe"

if (-not (Test-Path -LiteralPath $projectGenerator -PathType Leaf)) {
    throw "projectGenerator was not found at $projectGenerator"
}

if ([string]::IsNullOrWhiteSpace($WorkDir)) {
    $WorkDir = Join-Path $addonRoot "p"
}

if (Test-Path -LiteralPath $WorkDir) {
    Remove-Item -LiteralPath $WorkDir -Recurse -Force
}
New-Item -ItemType Directory -Path $WorkDir -Force | Out-Null

$examples = @(
    "ofxGgmlStableDiffusionExample",
    "ofxGgmlStableDiffusionBasicGenerationExample",
    "ofxGgmlStableDiffusionImageWorkflowExample",
    "ofxGgmlStableDiffusionVideoGenerationExample",
    "ofxGgmlStableDiffusionVideoControlFramesExample",
    "ofxGgmlStableDiffusionCreativeLoopExample",
    "ofxGgmlStableDiffusionLoraEmbeddingExample"
)

try {
    foreach ($example in $examples) {
        $source = Join-Path $addonRoot $example
        $target = Join-Path $WorkDir $example
        if (-not (Test-Path -LiteralPath $source -PathType Container)) {
            throw "Missing example: $source"
        }

        Write-Step "Generating $example with projectGenerator ($Platform)"
        New-Item -ItemType Directory -Path $target -Force | Out-Null
        Get-ChildItem -LiteralPath $source -Force |
            Where-Object { $_.Name -notin @(".vs", "bin", "obj") } |
            Copy-Item -Destination $target -Recurse -Force
        New-Item -ItemType Directory -Path (Join-Path $target "bin") -Force | Out-Null
        Invoke-ProjectGenerator `
            -ProjectGenerator $projectGenerator `
            -OfRoot $ofRoot `
            -PlatformName $Platform `
            -ProjectPath $target

        $projectFile = Join-Path $target "$example.vcxproj"
        if (-not (Test-Path -LiteralPath $projectFile -PathType Leaf)) {
            throw "projectGenerator did not create $projectFile"
        }

        Assert-GeneratedProjectContains $projectFile "ofxGgmlStableDiffusion" "ofxGgmlStableDiffusion project wiring"
        Assert-GeneratedProjectContains $projectFile "ofxImGui" "ofxImGui project wiring"
        Assert-GeneratedProjectContains $projectFile "src\ofApp.cpp" "example source wiring"
        Assert-GeneratedProjectContains $projectFile "src\main.cpp" "example main wiring"
        Assert-GeneratedProjectContains $projectFile "stable-diffusion.lib" "stable-diffusion import library wiring"
    }
} finally {
    if (-not $KeepWorkDir -and (Test-Path -LiteralPath $WorkDir)) {
        Remove-Item -LiteralPath $WorkDir -Recurse -Force
    }
}

Write-Step "projectGenerator example smoke completed"
