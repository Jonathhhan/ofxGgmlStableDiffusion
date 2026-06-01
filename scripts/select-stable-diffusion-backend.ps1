param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('cpu-only', 'cuda', 'vulkan', 'metal')]
    [string]$Backend,
    [string]$VariantRootDir = "",
    [string]$InstallIncludeDir = "",
    [string]$InstallLibDir = "",
    [string]$InstallBinDir = "",
    [string]$InstallGgmlIncludeDir = "",
    [string]$InstallGgmlLibDir = "",
    [string]$ExampleBinDir = ""
)

$ErrorActionPreference = "Stop"

function Write-Step {
    param([string]$Message)
    Write-Host "==> $Message"
}

function Remove-DirectoryContents {
    param([string]$LiteralPath)

    if (-not (Test-Path -LiteralPath $LiteralPath)) {
        return
    }

    Get-ChildItem -LiteralPath $LiteralPath -Force -ErrorAction SilentlyContinue |
        ForEach-Object {
            Remove-Item -LiteralPath $_.FullName -Recurse -Force
        }
}

function Copy-DirectoryContents {
    param(
        [string]$Source,
        [string]$Destination
    )

    if (-not (Test-Path -LiteralPath $Source)) {
        throw "Variant path was not found: $Source"
    }

    New-Item -ItemType Directory -Force -Path $Destination | Out-Null
    Get-ChildItem -LiteralPath $Source -Force -ErrorAction SilentlyContinue |
        ForEach-Object {
            Copy-Item -LiteralPath $_.FullName -Destination $Destination -Recurse -Force
        }
}

function Get-ExampleBinDirs {
    param(
        [string]$AddonRoot,
        [string]$ExplicitExampleBinDir
    )

    if (-not [string]::IsNullOrWhiteSpace($ExplicitExampleBinDir)) {
        return @($ExplicitExampleBinDir)
    }

    return @(
        Get-ChildItem -LiteralPath $AddonRoot -Directory -Filter "ofxGgmlStableDiffusion*Example" |
            Sort-Object Name |
            ForEach-Object { Join-Path $_.FullName "bin" }
    )
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$addonRoot = (Resolve-Path (Join-Path $scriptRoot '..')).Path

if ([string]::IsNullOrWhiteSpace($VariantRootDir)) {
    $VariantRootDir = Join-Path $addonRoot 'libs\variants'
}
if ([string]::IsNullOrWhiteSpace($InstallIncludeDir)) {
    $InstallIncludeDir = Join-Path $addonRoot 'libs\stable-diffusion\include'
}
if ([string]::IsNullOrWhiteSpace($InstallLibDir)) {
    $InstallLibDir = Join-Path $addonRoot 'libs\stable-diffusion\lib\vs'
}
if ([string]::IsNullOrWhiteSpace($InstallBinDir)) {
    $InstallBinDir = Join-Path $addonRoot 'libs\stable-diffusion\bin\vs'
}
if ([string]::IsNullOrWhiteSpace($InstallGgmlIncludeDir)) {
    $InstallGgmlIncludeDir = Join-Path $addonRoot 'libs\ggml\include'
}
if ([string]::IsNullOrWhiteSpace($InstallGgmlLibDir)) {
    $InstallGgmlLibDir = Join-Path $addonRoot 'libs\ggml\lib\vs'
}
$exampleBinDirs = Get-ExampleBinDirs -AddonRoot $addonRoot -ExplicitExampleBinDir $ExampleBinDir

$variantGgmlStableDiffusionIncludeDir = Join-Path $VariantRootDir "$Backend\stable-diffusion\include"
$variantGgmlStableDiffusionLibDir = Join-Path $VariantRootDir "$Backend\stable-diffusion\lib\vs"
$variantGgmlStableDiffusionBinDir = Join-Path $VariantRootDir "$Backend\stable-diffusion\bin\vs"
$variantGgmlIncludeDir = Join-Path $VariantRootDir "$Backend\ggml\include"
$variantGgmlLibDir = Join-Path $VariantRootDir "$Backend\ggml\lib\vs"

if (-not (Test-Path -LiteralPath $variantGgmlStableDiffusionIncludeDir)) {
    throw "The '$Backend' stable-diffusion variant is not staged yet. Build it first with scripts/build-stable-diffusion.ps1."
}
if (-not (Test-Path -LiteralPath $variantGgmlStableDiffusionLibDir)) {
    throw "The '$Backend' stable-diffusion libraries are not staged yet. Build it first with scripts/build-stable-diffusion.ps1."
}

Write-Step "Selecting backend variant"
Write-Host "    Backend: $Backend"
Write-Host "    Variant root: $VariantRootDir"

New-Item -ItemType Directory -Force -Path $InstallIncludeDir | Out-Null
New-Item -ItemType Directory -Force -Path $InstallLibDir | Out-Null
New-Item -ItemType Directory -Force -Path $InstallBinDir | Out-Null
New-Item -ItemType Directory -Force -Path $InstallGgmlIncludeDir | Out-Null
New-Item -ItemType Directory -Force -Path $InstallGgmlLibDir | Out-Null

Remove-DirectoryContents -LiteralPath $InstallIncludeDir
Remove-DirectoryContents -LiteralPath $InstallLibDir
Remove-DirectoryContents -LiteralPath $InstallBinDir
Remove-DirectoryContents -LiteralPath $InstallGgmlIncludeDir
Remove-DirectoryContents -LiteralPath $InstallGgmlLibDir

Copy-DirectoryContents -Source $variantGgmlStableDiffusionIncludeDir -Destination $InstallIncludeDir
Copy-DirectoryContents -Source $variantGgmlStableDiffusionLibDir -Destination $InstallLibDir
if (Test-Path -LiteralPath $variantGgmlStableDiffusionBinDir) {
    Copy-DirectoryContents -Source $variantGgmlStableDiffusionBinDir -Destination $InstallBinDir
}

if (Test-Path -LiteralPath $variantGgmlIncludeDir) {
    Copy-DirectoryContents -Source $variantGgmlIncludeDir -Destination $InstallGgmlIncludeDir
}
if (Test-Path -LiteralPath $variantGgmlLibDir) {
    Copy-DirectoryContents -Source $variantGgmlLibDir -Destination $InstallGgmlLibDir
}

$selectedDll = Join-Path $InstallLibDir 'stable-diffusion.dll'
if (Test-Path -LiteralPath $selectedDll) {
    foreach ($binDir in $exampleBinDirs) {
        if (-not (Test-Path -LiteralPath $binDir)) {
            continue
        }
        try {
            Copy-Item -LiteralPath $selectedDll -Destination $binDir -Force
        } catch [System.IO.IOException] {
            Write-Warning "Skipping example runtime copy into $binDir because the destination DLL is in use. Close the running example app and rerun this selector if you want the chosen backend staged there too."
        }
    }
}

Write-Host ""
Write-Host "Selected backend variant: $Backend"
Write-Host "  stable-diffusion include: $InstallIncludeDir"
Write-Host "  stable-diffusion libs:    $InstallLibDir"
Write-Host "  stable-diffusion bins:    $InstallBinDir"
Write-Host "  ggml include:             $InstallGgmlIncludeDir"
Write-Host "  ggml libs:                $InstallGgmlLibDir"
