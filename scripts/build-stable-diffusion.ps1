param(
    [string]$SourceDir = "",
    [string]$BuildDir = "",
    [string]$InstallIncludeDir = "",
    [string]$InstallLibDir = "",
    [string]$InstallGgmlIncludeDir = "",
    [string]$InstallGgmlLibDir = "",
    [string]$VariantRootDir = "",
    [string]$ExampleBinDir = "",
    [string]$InstallBinDir = "",
    [string]$Configuration = "Release",
    [string]$SourceReleaseTag = "",
    [string]$Generator = "",
    [int]$Jobs = 0,
    [switch]$Clean,
    [string]$GgmlReleaseTag = "",
    [Alias('Cpu')][switch]$CpuOnly,
    [Alias('Gpu')][switch]$Cuda,
    [string]$CudaArchitectures = $(if ($env:OFXGGML_CUDA_ARCHITECTURES) { $env:OFXGGML_CUDA_ARCHITECTURES } else { "" }),
    [switch]$Vulkan,
    [switch]$Metal,
    [switch]$All,
    [switch]$BuildCli,
    [switch]$SkipSourceRefresh,
    [switch]$DryRun,
    [switch]$UseSystemGgml,
    [switch]$UseBundledGgml,
    [string]$OfxGgmlPath = ""
)

$ErrorActionPreference = "Stop"
$DefaultSourceReleaseTag = "master-813-bfbef5b"

if ($UseSystemGgml -and $UseBundledGgml) {
    throw "Choose either -UseSystemGgml or -UseBundledGgml, not both."
}
if (-not $UseBundledGgml) {
    $UseSystemGgml = $true
}
if ($UseSystemGgml -and -not [string]::IsNullOrWhiteSpace($GgmlReleaseTag)) {
    throw "-GgmlReleaseTag only applies to -UseBundledGgml. The default system lane gets ggml from ofxGgmlCore."
}

function Write-Step {
    param([string]$Message)
    Write-Host "==> $Message"
}

function Test-IsWindowsHost {
    return $env:OS -eq 'Windows_NT'
}

function Test-IsMacHost {
    return [System.Environment]::OSVersion.Platform -eq [System.PlatformID]::MacOSX
}

function Invoke-External {
    param(
        [string]$FilePath,
        [string[]]$Arguments
    )

    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $FilePath
    $startInfo.Arguments = Join-ProcessArguments -Arguments $Arguments
    $startInfo.WorkingDirectory = (Get-Location).Path
    $startInfo.UseShellExecute = $false

    Copy-SanitizedProcessEnvironment -StartInfo $startInfo

    $process = [System.Diagnostics.Process]::Start($startInfo)
    $process.WaitForExit()
    if ($process.ExitCode -ne 0) {
        throw "Command failed with exit code $($process.ExitCode): $FilePath $($Arguments -join ' ')"
    }
}

function Invoke-ExternalInCurrentShell {
    param(
        [string]$FilePath,
        [string[]]$Arguments
    )

    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "Command failed with exit code ${LASTEXITCODE}: $FilePath $($Arguments -join ' ')"
    }
}

function Invoke-MSBuildFreshPowerShell {
    param(
        [string]$MSBuildPath,
        [string[]]$Arguments
    )

    $powershell = Get-CommandPathOrNull 'powershell.exe'
    if (-not $powershell) {
        $powershell = Get-CommandPathOrNull 'powershell'
    }
    if (-not $powershell) {
        throw "powershell.exe was not found; cannot launch isolated MSBuild process."
    }

    $quoted = @($MSBuildPath) + $Arguments | ForEach-Object {
        "'" + ([string]$_ -replace "'", "''") + "'"
    }
    $command = '& ' + ($quoted -join ' ')
    Invoke-External -FilePath $powershell -Arguments @(
        '-NoProfile',
        '-ExecutionPolicy', 'Bypass',
        '-Command', $command
    )
}

function Join-ProcessArguments {
    param([string[]]$Arguments)

    return (($Arguments | ForEach-Object {
        if ($_ -notmatch '[\s"]') {
            $_
        } else {
            '"' + ($_ -replace '"', '\"') + '"'
        }
    }) -join ' ')
}

function Copy-SanitizedProcessEnvironment {
    param([System.Diagnostics.ProcessStartInfo]$StartInfo)

    $environment = $StartInfo.Environment
    if ($null -eq $environment) {
        $environment = $StartInfo.EnvironmentVariables
    }
    if ($null -eq $environment) {
        throw "ProcessStartInfo did not expose a mutable environment dictionary."
    }

    $environment.Clear()
    $seen = New-Object 'System.Collections.Generic.HashSet[string]' ([System.StringComparer]::OrdinalIgnoreCase)
    $pathValue = $env:Path

    foreach ($key in [Environment]::GetEnvironmentVariables('Process').Keys) {
        if ([string]::Equals([string]$key, 'Path', [System.StringComparison]::OrdinalIgnoreCase)) {
            continue
        }
        if ($seen.Add([string]$key)) {
            $environment[[string]$key] = [Environment]::GetEnvironmentVariable([string]$key, 'Process')
        }
    }

    if (-not [string]::IsNullOrEmpty($pathValue)) {
        $environment['Path'] = $pathValue
    }
}

function Get-CommandPathOrNull {
    param([string]$Name)
    try {
        return (Get-Command $Name -ErrorAction Stop).Source
    } catch {
        return $null
    }
}

function Convert-ToCMakePath {
    param([string]$Path)
    return ([System.IO.Path]::GetFullPath($Path) -replace '\\', '/')
}

function Add-RequiredLibraryPath {
    param(
        [System.Collections.Generic.List[string]]$Libraries,
        [string]$Path,
        [string]$Description
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Description was not found at: $Path"
    }
    $Libraries.Add($Path)
}

function Get-CoreProviderManifest {
    param([string]$ProviderPath)

    $manifestScript = Join-Path $ProviderPath 'scripts\runtime-provider-manifest.ps1'
    if (-not (Test-Path -LiteralPath $manifestScript -PathType Leaf)) {
        return $null
    }

    $json = & $manifestScript -Json -SummaryOnly 2>$null
    if (!$? -or !$json) {
        return $null
    }
    return (($json | ForEach-Object { $_.ToString() }) -join "`n") | ConvertFrom-Json
}

function New-OfxGgmlCmakePackage {
    param(
        [string]$OfxGgmlPath,
        [string]$PackageRoot,
        [bool]$EnableCuda,
        [bool]$EnableVulkan
    )

    $providerManifest = Get-CoreProviderManifest -ProviderPath $OfxGgmlPath
    if ($providerManifest) {
        $includeDir = [string]$providerManifest.GgmlIncludeDir
        $libDir = [string]$providerManifest.GgmlLibDir
    } else {
        $includeDir = [System.IO.Path]::Combine($OfxGgmlPath, 'libs', 'ggml', 'include')
        $libDir = [System.IO.Path]::Combine($OfxGgmlPath, 'libs', 'ggml', 'lib')
    }
    $packageDir = Join-Path $PackageRoot 'ggml'
    $libraries = [System.Collections.Generic.List[string]]::new()

    Add-RequiredLibraryPath -Libraries $libraries -Path (Join-Path $libDir 'ggml.lib') -Description 'system ggml.lib'
    Add-RequiredLibraryPath -Libraries $libraries -Path (Join-Path $libDir 'ggml-base.lib') -Description 'system ggml-base.lib'
    Add-RequiredLibraryPath -Libraries $libraries -Path (Join-Path $libDir 'ggml-cpu.lib') -Description 'system ggml-cpu.lib'

    if ($EnableCuda) {
        Add-RequiredLibraryPath -Libraries $libraries -Path (Join-Path $libDir 'ggml-cuda.lib') -Description 'system ggml-cuda.lib'
        $cudaLibDir = if ($env:CUDA_PATH) { [System.IO.Path]::Combine($env:CUDA_PATH, 'lib', 'x64') } else { '' }
        Add-RequiredLibraryPath -Libraries $libraries -Path (Join-Path $cudaLibDir 'cublas.lib') -Description 'CUDA cublas.lib'
        Add-RequiredLibraryPath -Libraries $libraries -Path (Join-Path $cudaLibDir 'cudart.lib') -Description 'CUDA cudart.lib'
        Add-RequiredLibraryPath -Libraries $libraries -Path (Join-Path $cudaLibDir 'cuda.lib') -Description 'CUDA cuda.lib'
    }

    if ($EnableVulkan) {
        Add-RequiredLibraryPath -Libraries $libraries -Path (Join-Path $libDir 'ggml-vulkan.lib') -Description 'system ggml-vulkan.lib'
        $vulkanLibDir = if ($env:VULKAN_SDK) { [System.IO.Path]::Combine($env:VULKAN_SDK, 'Lib') } else { '' }
        Add-RequiredLibraryPath -Libraries $libraries -Path (Join-Path $vulkanLibDir 'vulkan-1.lib') -Description 'Vulkan vulkan-1.lib'
    }

    New-Item -ItemType Directory -Force -Path $packageDir | Out-Null

    $cmakeIncludeDir = Convert-ToCMakePath $includeDir
    $cmakeLibraryEntries = ($libraries | ForEach-Object { '    "' + (Convert-ToCMakePath $_) + '"' }) -join "`n"
    $configPath = Join-Path $packageDir 'ggml-config.cmake'
    $versionPath = Join-Path $packageDir 'ggml-version.cmake'

    $configContent = @"
include_guard(GLOBAL)

set(ggml_FOUND TRUE)
set(_ofxggml_include_dir "$cmakeIncludeDir")
set(_ofxggml_libraries
$cmakeLibraryEntries
)

if (NOT TARGET ggml::ggml)
    add_library(ggml::ggml INTERFACE IMPORTED)
    set_target_properties(ggml::ggml PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "`${_ofxggml_include_dir}"
        INTERFACE_COMPILE_DEFINITIONS "GGML_MAX_NAME=128"
        INTERFACE_LINK_LIBRARIES "`${_ofxggml_libraries}"
    )
endif()
"@

    $versionContent = @"
set(PACKAGE_VERSION "ofxGgml")
set(PACKAGE_VERSION_COMPATIBLE TRUE)
"@

    Set-Content -LiteralPath $configPath -Value $configContent -Encoding ASCII
    Set-Content -LiteralPath $versionPath -Value $versionContent -Encoding ASCII
    return $packageDir
}

function Test-CudaAvailable {
    if ($env:CUDA_PATH -and (Test-Path -LiteralPath $env:CUDA_PATH)) {
        return $true
    }
    return [bool](Get-CommandPathOrNull 'nvcc.exe')
}

function Test-VulkanAvailable {
    if ($env:VULKAN_SDK -and (Test-Path -LiteralPath $env:VULKAN_SDK)) {
        return $true
    }
    return [bool](Get-CommandPathOrNull 'glslc.exe') -or
        [bool](Get-CommandPathOrNull 'vulkaninfo.exe')
}

function Test-MetalAvailable {
    if (-not (Test-IsMacHost)) {
        return $false
    }
    return [bool](Get-CommandPathOrNull 'xcrun')
}

function Invoke-SelfBuild {
    param(
        [string]$BackendName,
        [string]$ScriptPath,
        [string]$SourceDir,
        [string]$BuildDir,
        [string]$InstallIncludeDir,
        [string]$InstallLibDir,
        [string]$InstallGgmlIncludeDir,
        [string]$InstallGgmlLibDir,
        [string]$VariantRootDir,
        [string]$ExampleBinDir,
        [string]$InstallBinDir,
        [string]$Configuration,
        [string]$SourceReleaseTag,
        [string]$CudaArchitectures,
        [string]$Generator,
        [int]$Jobs,
        [string]$GgmlReleaseTag,
        [switch]$Clean,
        [switch]$BuildCli,
        [switch]$DryRun,
        [switch]$SkipSourceRefresh,
        [switch]$UseSystemGgml,
        [switch]$UseBundledGgml,
        [string]$OfxGgmlPath
    )

    $invokeArgs = @{
        SourceDir = $SourceDir
        BuildDir = $BuildDir
        InstallIncludeDir = $InstallIncludeDir
        InstallLibDir = $InstallLibDir
        InstallGgmlIncludeDir = $InstallGgmlIncludeDir
        InstallGgmlLibDir = $InstallGgmlLibDir
        VariantRootDir = $VariantRootDir
        ExampleBinDir = $ExampleBinDir
        InstallBinDir = $InstallBinDir
        Configuration = $Configuration
        Generator = $Generator
        Jobs = $Jobs
        GgmlReleaseTag = $GgmlReleaseTag
    }

    if (-not [string]::IsNullOrWhiteSpace($SourceReleaseTag)) {
        $invokeArgs.SourceReleaseTag = $SourceReleaseTag
    }
    if (-not [string]::IsNullOrWhiteSpace($CudaArchitectures)) {
        $invokeArgs.CudaArchitectures = $CudaArchitectures
    }
    if ($Clean) {
        $invokeArgs.Clean = $true
    }
    if ($DryRun) {
        $invokeArgs.DryRun = $true
    }
    if ($BuildCli) {
        $invokeArgs.BuildCli = $true
    }
    if ($SkipSourceRefresh) {
        $invokeArgs.SkipSourceRefresh = $true
    }
    if ($UseSystemGgml) {
        $invokeArgs.UseSystemGgml = $true
    }
    if ($UseBundledGgml) {
        $invokeArgs.UseBundledGgml = $true
    }
    if (-not [string]::IsNullOrWhiteSpace($OfxGgmlPath)) {
        $invokeArgs.OfxGgmlPath = $OfxGgmlPath
    }

    switch ($BackendName) {
        'cpu-only' { $invokeArgs.CpuOnly = $true }
        'cuda' { $invokeArgs.Cuda = $true }
        'vulkan' { $invokeArgs.Vulkan = $true }
        'metal' { $invokeArgs.Metal = $true }
        default { throw "Unknown backend '$BackendName'." }
    }

    & $ScriptPath @invokeArgs
}

function Get-ShortBuildDir {
    param(
        [string]$AddonRoot,
        [switch]$DryRun
    )

    $hashBytes = [System.Security.Cryptography.SHA1]::Create().ComputeHash(
        [System.Text.Encoding]::UTF8.GetBytes($AddonRoot))
    $hash = [System.BitConverter]::ToString($hashBytes).Replace('-', '').Substring(0, 10).ToLowerInvariant()
    $candidateRoots = @()

    if (-not [string]::IsNullOrWhiteSpace($env:OFXSD_SHORT_BUILD_ROOT)) {
        $candidateRoots += $env:OFXSD_SHORT_BUILD_ROOT
    }
    if (-not [string]::IsNullOrWhiteSpace($env:PUBLIC)) {
        $candidateRoots += (Join-Path $env:PUBLIC 'sd')
    }
    if (-not [string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
        $candidateRoots += (Join-Path $env:LOCALAPPDATA 'sd')
    }

    $tempRoot = [System.IO.Path]::GetTempPath()
    if (-not [string]::IsNullOrWhiteSpace($tempRoot)) {
        $candidateRoots += (Join-Path $tempRoot 'sd')
    }

    foreach ($root in ($candidateRoots | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Select-Object -Unique)) {
        $candidate = Join-Path $root $hash
        if ($DryRun) {
            return $candidate
        }

        try {
            New-Item -ItemType Directory -Force -Path $root | Out-Null
            return $candidate
        } catch {
            continue
        }
    }

    return Join-Path $tempRoot "ofxsd-build-$hash"
}

function Find-FirstFile {
    param(
        [string]$Root,
        [string[]]$Names
    )

    if (-not (Test-Path -LiteralPath $Root)) {
        return $null
    }

    foreach ($name in $Names) {
        $match = Get-ChildItem -LiteralPath $Root -Recurse -File -Filter $name -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if ($match) {
            return $match.FullName
        }
    }

    return $null
}

function Copy-IfPresent {
    param(
        [string]$Path,
        [string]$Destination,
        [switch]$AllowLockedDestination
    )

    if (-not $Path) {
        return
    }

    if ($DryRun) {
        Write-Host "Copy $Path -> $Destination"
        return
    }

    try {
        Copy-Item -LiteralPath $Path -Destination $Destination -Force
    } catch [System.IO.IOException] {
        if ($AllowLockedDestination) {
            Write-Warning "Skipping copy into $Destination because the destination file is in use. Close the running example app and re-run setup if you want the latest runtime staged there."
            return
        }
        throw
    }
}

function Get-VendoredValue {
    param(
        [string]$Path,
        [string]$Prefix
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        return $null
    }

    $line = Get-Content -LiteralPath $Path -ErrorAction SilentlyContinue |
        Where-Object { $_ -like "$Prefix*" } |
        Select-Object -First 1
    if (-not $line) {
        return $null
    }

    return $line.Substring($Prefix.Length).Trim()
}

function Invoke-GitHubJsonRequest {
    param([string]$Uri)

    $headers = @{
        'Accept' = 'application/vnd.github+json'
        'User-Agent' = 'ofxGgmlStableDiffusion-build'
        'X-GitHub-Api-Version' = '2022-11-28'
    }

    try {
        return Invoke-RestMethod -Uri $Uri -Headers $headers
    } catch {
        throw "GitHub API request failed for $Uri. $($_.Exception.Message)"
    }
}

function Require-GitPath {
    $git = Get-CommandPathOrNull 'git.exe'
    if (-not $git) {
        $git = Get-CommandPathOrNull 'git'
    }
    if (-not $git) {
        throw "git was not found in PATH. A recursive clone is required so the latest release-tag source snapshot includes ggml/libwebp/libwebm submodules."
    }
    return $git
}

function Require-MSBuildPath {
    $msbuild = Get-CommandPathOrNull 'MSBuild.exe'
    if ($msbuild) {
        return $msbuild
    }

    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere -PathType Leaf) {
        $msbuild = & $vswhere `
            -latest `
            -products '*' `
            -requires Microsoft.Component.MSBuild `
            -property installationPath |
            ForEach-Object { Join-Path $_ 'MSBuild\Current\Bin\amd64\MSBuild.exe' } |
            Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
            Select-Object -First 1
        if ($msbuild) {
            return $msbuild
        }
    }

    $candidates = @(
        "${env:ProgramFiles}\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\18\Professional\MSBuild\Current\Bin\amd64\MSBuild.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\18\Enterprise\MSBuild\Current\Bin\amd64\MSBuild.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\18\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\17\Community\MSBuild\Current\Bin\amd64\MSBuild.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\17\Professional\MSBuild\Current\Bin\amd64\MSBuild.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\17\Enterprise\MSBuild\Current\Bin\amd64\MSBuild.exe",
        "${env:ProgramFiles}\Microsoft Visual Studio\17\BuildTools\MSBuild\Current\Bin\amd64\MSBuild.exe"
    )
    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate -PathType Leaf)) {
            return $candidate
        }
    }

    throw "MSBuild.exe was not found. Install Visual Studio Build Tools or pass a non-Visual Studio CMake generator."
}

function Get-ReleaseMetadata {
    param([string]$Tag)

    $repoApiBase = 'https://api.github.com/repos/leejet/stable-diffusion.cpp/releases'
    if ([string]::IsNullOrWhiteSpace($Tag)) {
        return Invoke-GitHubJsonRequest -Uri ($repoApiBase + '/latest')
    }

    $escapedTag = [System.Uri]::EscapeDataString($Tag)
    try {
        return Invoke-GitHubJsonRequest -Uri ($repoApiBase + '/tags/' + $escapedTag)
    } catch {
        Write-Warning ("GitHub release metadata was unavailable for {0}; cloning the tag directly. {1}" -f $Tag, $_.Exception.Message)
        return New-DryRunReleaseMetadata -Tag $Tag -Repository 'https://github.com/leejet/stable-diffusion.cpp'
    }
}

function New-DryRunReleaseMetadata {
    param(
        [string]$Tag,
        [string]$Repository
    )
    $resolvedTag = if ([string]::IsNullOrWhiteSpace($Tag)) { "remote default" } else { $Tag }
    return [pscustomobject]@{
        tag_name = $resolvedTag
        target_commitish = ""
        html_url = $Repository
        zipball_url = ""
    }
}

function Get-GgmlReleaseMetadata {
    param([string]$Tag)

    $repoApiBase = 'https://api.github.com/repos/ggml-org/ggml/releases'
    if ([string]::IsNullOrWhiteSpace($Tag)) {
        return Invoke-GitHubJsonRequest -Uri ($repoApiBase + '/latest')
    }

    $escapedTag = [System.Uri]::EscapeDataString($Tag)
    try {
        return Invoke-GitHubJsonRequest -Uri ($repoApiBase + '/tags/' + $escapedTag)
    } catch {
        Write-Warning ("GitHub release metadata was unavailable for ggml {0}; cloning the tag directly. {1}" -f $Tag, $_.Exception.Message)
        return New-DryRunReleaseMetadata -Tag $Tag -Repository 'https://github.com/ggml-org/ggml'
    }
}

function Remove-DirectoryContents {
    param([string]$LiteralPath)

    if (-not (Test-Path -LiteralPath $LiteralPath)) {
        return
    }

    Get-ChildItem -LiteralPath $LiteralPath -Force -ErrorAction SilentlyContinue |
        ForEach-Object {
            $itemPath = $_.FullName
            try {
                Remove-Item -LiteralPath $itemPath -Recurse -Force -ErrorAction Stop
            } catch [System.IO.DirectoryNotFoundException] {
                if (Test-Path -LiteralPath $itemPath) {
                    $quarantineRoot = Join-Path ([System.IO.Path]::GetTempPath()) 'ofxsd-source-cleanup-quarantine'
                    New-Item -ItemType Directory -Force -Path $quarantineRoot | Out-Null
                    $quarantinePath = Join-Path $quarantineRoot ((Split-Path -Leaf $itemPath) + '-' + [Guid]::NewGuid().ToString('N'))
                    Move-Item -LiteralPath $itemPath -Destination $quarantinePath
                    Write-Warning "Moved an undeletable stale source subtree to: $quarantinePath"
                }
            }
        }
}

function Write-VendorPinFile {
    param(
        [string]$Path,
        [object]$ReleaseMetadata,
        [string]$Repository = "",
        [string]$ResolvedCommit = "",
        [string]$Notes = ""
    )

    $lines = New-Object System.Collections.Generic.List[string]
    $lines.Add('Vendored by ofxGgmlStableDiffusion')
    if (-not [string]::IsNullOrWhiteSpace($Repository)) {
        $lines.Add('Upstream repository: ' + $Repository)
    }
    $lines.Add('Upstream release tag: ' + $ReleaseMetadata.tag_name)
    if (-not [string]::IsNullOrWhiteSpace($ResolvedCommit)) {
        $lines.Add('Upstream commit: ' + $ResolvedCommit)
    }
    if (-not [string]::IsNullOrWhiteSpace($ReleaseMetadata.target_commitish)) {
        $lines.Add('Upstream target commitish: ' + $ReleaseMetadata.target_commitish)
    }
    $lines.Add('Release URL: ' + $ReleaseMetadata.html_url)
    $lines.Add('Zipball URL: ' + $ReleaseMetadata.zipball_url)
    $lines.Add('Fetched at: ' + [DateTimeOffset]::UtcNow.ToString('o'))
    if (-not [string]::IsNullOrWhiteSpace($Notes)) {
        $lines.Add('Notes: ' + $Notes)
    }

    [System.IO.File]::WriteAllLines($Path, $lines)
}

function Get-GitHeadCommit {
    param(
        [string]$GitPath,
        [string]$RepositoryRoot
    )

    $commit = (& $GitPath -C $RepositoryRoot rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($commit)) {
        throw "Failed to resolve HEAD commit for '$RepositoryRoot'."
    }

    return $commit
}

function Refresh-GgmlVendorTree {
    param(
        [string]$GitPath,
        [string]$Tag,
        [string]$TargetDir,
        [switch]$DryRun
    )

    if ([string]::IsNullOrWhiteSpace($Tag)) {
        Write-Step "Using ggml submodule pinned by stable-diffusion.cpp"
        Write-Host ("    Source dir: {0}" -f $TargetDir)
        Write-Host "    Pass -GgmlReleaseTag to override this with a separate ggml release."
        return
    }

    $releaseMetadata = if ($DryRun) {
        New-DryRunReleaseMetadata -Tag $Tag -Repository 'https://github.com/ggml-org/ggml'
    } else {
        Get-GgmlReleaseMetadata -Tag $Tag
    }
    $resolvedReleaseTag = $releaseMetadata.tag_name
    $downloadRoot = Join-Path $env:TEMP 'ofxsd-ggml-release'
    $cloneRoot = Join-Path $downloadRoot ('clone-' + $resolvedReleaseTag)

    Write-Step "Refreshing ggml source from upstream release snapshot"
    Write-Host ("    Release tag: {0}" -f $resolvedReleaseTag)
    Write-Host ("    Destination: {0}" -f $TargetDir)

    if ($DryRun) {
        Write-Host ("Clone https://github.com/ggml-org/ggml.git tag {0}" -f $resolvedReleaseTag)
        Write-Host ("Clone to {0}" -f $cloneRoot)
        Write-Host ("Replace contents of {0}" -f $TargetDir)
        return
    }

    New-Item -ItemType Directory -Force -Path $downloadRoot | Out-Null
    if (Test-Path -LiteralPath $cloneRoot) {
        Remove-Item -LiteralPath $cloneRoot -Recurse -Force
    }

    Invoke-External -FilePath $GitPath -Arguments @(
        'clone',
        '--depth', '1',
        '--branch', $resolvedReleaseTag,
        'https://github.com/ggml-org/ggml.git',
        $cloneRoot
    )

    $cmakeLists = Join-Path $cloneRoot 'CMakeLists.txt'
    if (-not (Test-Path -LiteralPath $cmakeLists)) {
        throw "The ggml clone for '$resolvedReleaseTag' did not contain a top-level CMakeLists.txt."
    }

    New-Item -ItemType Directory -Force -Path $TargetDir | Out-Null
    Remove-DirectoryContents -LiteralPath $TargetDir

    Get-ChildItem -LiteralPath $cloneRoot -Force |
        ForEach-Object {
            if ($_.Name -eq '.git') {
                return
            }
            Copy-Item -LiteralPath $_.FullName -Destination $TargetDir -Recurse -Force
        }

    $resolvedCommit = Get-GitHeadCommit -GitPath $GitPath -RepositoryRoot $cloneRoot
    Write-VendorPinFile `
        -Path (Join-Path $TargetDir 'OFX_VENDOR_PIN.txt') `
        -ReleaseMetadata $releaseMetadata `
        -Repository 'https://github.com/ggml-org/ggml' `
        -ResolvedCommit $resolvedCommit `
        -Notes 'Source-only vendor refresh staged under stable-diffusion.cpp.'
}

function Apply-GgufExtraDimensionFoldPatch {
    param(
        [string]$SourceDir,
        [switch]$DryRun
    )

    $ggufPath = Join-Path $SourceDir 'ggml\src\gguf.cpp'
    Write-Step "Applying stable-diffusion.cpp GGUF dimension compatibility patch"
    Write-Host ("    Target: {0}" -f $ggufPath)
    if ($DryRun) {
        Write-Host "Patch gguf.cpp so tensors with more than GGML_MAX_DIMS dimensions fold extra dimensions into the last ggml dimension."
        return
    }
    if (-not (Test-Path -LiteralPath $ggufPath)) {
        throw "Cannot patch GGUF reader because gguf.cpp was not found at: $ggufPath"
    }

    $content = [System.IO.File]::ReadAllText($ggufPath)
    if ($content.Contains('int64_t folded_dims = 1;')) {
        Write-Host "    Already patched."
        return
    }

    $old = @'
            if (n_dims > GGML_MAX_DIMS) {
                GGML_LOG_ERROR("%s: tensor '%s' has invalid number of dimensions: %" PRIu32 " > %" PRIu32 "\n",
                    __func__, info.t.name, n_dims, GGML_MAX_DIMS);
                ok = false;
                break;
            }
            for (uint32_t j = 0; ok && j < GGML_MAX_DIMS; ++j) {
                info.t.ne[j] = 1;
                if (j < n_dims) {
                    ok = ok && gr.read(info.t.ne[j]);
                }

                // check that all ne are non-negative
                if (info.t.ne[j] < 0) {
                    GGML_LOG_ERROR("%s: tensor '%s' dimension %" PRIu32 " has invalid number of elements: %" PRIi64 " < 0\n",
                        __func__, info.t.name, j, info.t.ne[j]);
                    ok = false;
                    break;
                }
            }
'@
    $new = @'
            int64_t folded_dims = 1;
            for (uint32_t j = 0; ok && j < GGML_MAX_DIMS; ++j) {
                info.t.ne[j] = 1;
                if (j < n_dims) {
                    ok = ok && gr.read(info.t.ne[j]);
                }

                // check that all ne are non-negative
                if (info.t.ne[j] < 0) {
                    GGML_LOG_ERROR("%s: tensor '%s' dimension %" PRIu32 " has invalid number of elements: %" PRIi64 " < 0\n",
                        __func__, info.t.name, j, info.t.ne[j]);
                    ok = false;
                    break;
                }
            }
            for (uint32_t j = GGML_MAX_DIMS; ok && j < n_dims; ++j) {
                int64_t folded_ne = 1;
                ok = ok && gr.read(folded_ne);
                if (folded_ne < 0) {
                    GGML_LOG_ERROR("%s: tensor '%s' dimension %" PRIu32 " has invalid number of elements: %" PRIi64 " < 0\n",
                        __func__, info.t.name, j, folded_ne);
                    ok = false;
                    break;
                }
                if (folded_ne != 0 && INT64_MAX/folded_ne <= folded_dims) {
                    GGML_LOG_ERROR("%s: folded tensor dimensions in tensor '%s' are >= %" PRIi64 "\n",
                        __func__, info.t.name, INT64_MAX);
                    ok = false;
                    break;
                }
                folded_dims *= folded_ne;
            }
            if (ok && n_dims > GGML_MAX_DIMS) {
                if (folded_dims != 0 && INT64_MAX/folded_dims <= info.t.ne[GGML_MAX_DIMS - 1]) {
                    GGML_LOG_ERROR("%s: folded shape for tensor '%s' is >= %" PRIi64 "\n",
                        __func__, info.t.name, INT64_MAX);
                    ok = false;
                    break;
                }
                info.t.ne[GGML_MAX_DIMS - 1] *= folded_dims;
            }
'@
    if ($content.Contains($old)) {
        [System.IO.File]::WriteAllText($ggufPath, $content.Replace($old, $new))
        return
    }

    $oldLf = $old -replace "`r`n", "`n"
    if ($content.Contains($oldLf)) {
        $newLf = $new -replace "`r`n", "`n"
        [System.IO.File]::WriteAllText($ggufPath, $content.Replace($oldLf, $newLf))
        return
    }

    $shapeBlockPattern = '(?s)            if \(n_dims > GGML_MAX_DIMS\) \{.*?            \}\r?\n\r?\n(?=            // check that the total number of elements is representable)'
    $updated = [System.Text.RegularExpressions.Regex]::Replace($content, $shapeBlockPattern, $new + [Environment]::NewLine, 1)
    if ($updated -eq $content) {
        throw "Could not apply GGUF dimension compatibility patch. The gguf.cpp tensor-shape block did not match the expected upstream layout."
    }
    [System.IO.File]::WriteAllText($ggufPath, $updated)
}

$scriptRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$addonRoot = (Resolve-Path (Join-Path $scriptRoot '..')).Path

if ([string]::IsNullOrWhiteSpace($SourceDir)) {
    $SourceDir = Join-Path $addonRoot 'libs\stable-diffusion\source'
}
$buildDirWasImplicit = [string]::IsNullOrWhiteSpace($BuildDir)
if ([string]::IsNullOrWhiteSpace($InstallIncludeDir)) {
    $InstallIncludeDir = Join-Path $addonRoot 'libs\stable-diffusion\include'
}
if ([string]::IsNullOrWhiteSpace($InstallLibDir)) {
    $InstallLibDir = Join-Path $addonRoot 'libs\stable-diffusion\lib\vs'
}

function Copy-DirectoryContents {
    param(
        [string]$Source,
        [string]$Destination
    )

    if (-not (Test-Path -LiteralPath $Source)) {
        return
    }

    if ($DryRun) {
        Write-Host "Copy contents $Source -> $Destination"
        return
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

$stageBundledGgml = -not $UseSystemGgml

if ($stageBundledGgml -and [string]::IsNullOrWhiteSpace($InstallGgmlIncludeDir)) {
    $InstallGgmlIncludeDir = Join-Path $addonRoot 'libs\ggml\include'
}
if ($stageBundledGgml -and [string]::IsNullOrWhiteSpace($InstallGgmlLibDir)) {
    $InstallGgmlLibDir = Join-Path $addonRoot 'libs\ggml\lib\vs'
}
if ([string]::IsNullOrWhiteSpace($VariantRootDir)) {
    $VariantRootDir = Join-Path $addonRoot 'libs\variants'
}
if ([string]::IsNullOrWhiteSpace($InstallBinDir)) {
    $InstallBinDir = Join-Path $addonRoot 'libs\stable-diffusion\bin\vs'
}
if ($Jobs -le 0) {
    $Jobs = [Math]::Max(1, [Environment]::ProcessorCount)
}
$exampleBinDirs = Get-ExampleBinDirs -AddonRoot $addonRoot -ExplicitExampleBinDir $ExampleBinDir

$selectedBackendCount = 0
if ($CpuOnly) { $selectedBackendCount++ }
if ($Cuda) { $selectedBackendCount++ }
if ($Vulkan) { $selectedBackendCount++ }
if ($Metal) { $selectedBackendCount++ }
if ($selectedBackendCount -gt 1) {
    throw "Select only one backend flag: -CpuOnly, -Cuda, -Vulkan, or -Metal."
}
if ($All -and $selectedBackendCount -gt 0) {
    throw "Do not combine -All with explicit backend flags."
}

if ($All) {
    $scriptPath = $MyInvocation.MyCommand.Path
    $allBackends = New-Object System.Collections.Generic.List[string]
    $allBackends.Add('cpu-only')

    if (Test-VulkanAvailable) {
        $allBackends.Add('vulkan')
    } else {
        Write-Warning "Skipping Vulkan in -All mode because no Vulkan SDK/runtime was detected."
    }

    if (Test-CudaAvailable) {
        $allBackends.Add('cuda')
    } else {
        Write-Warning "Skipping CUDA in -All mode because no CUDA toolkit/runtime was detected."
    }

    Write-Step "Building all available backend variants"
    Write-Host ("    Order: {0}" -f ($allBackends -join ' -> '))
    Write-Host "    Final canonical runtime priority: cuda > vulkan > cpu-only"

    $firstBuild = $true
    foreach ($backendName in $allBackends) {
        Invoke-SelfBuild `
            -BackendName $backendName `
            -ScriptPath $scriptPath `
            -SourceDir $SourceDir `
            -BuildDir $BuildDir `
            -InstallIncludeDir $InstallIncludeDir `
            -InstallLibDir $InstallLibDir `
            -InstallGgmlIncludeDir $InstallGgmlIncludeDir `
            -InstallGgmlLibDir $InstallGgmlLibDir `
            -VariantRootDir $VariantRootDir `
            -ExampleBinDir $ExampleBinDir `
            -InstallBinDir $InstallBinDir `
            -Configuration $Configuration `
            -SourceReleaseTag $SourceReleaseTag `
            -CudaArchitectures $CudaArchitectures `
            -Generator $Generator `
            -Jobs $Jobs `
            -GgmlReleaseTag $GgmlReleaseTag `
            -Clean `
            -BuildCli:$BuildCli `
            -DryRun:$DryRun `
            -SkipSourceRefresh:$(-not $firstBuild) `
            -UseSystemGgml:$UseSystemGgml `
            -UseBundledGgml:$UseBundledGgml `
            -OfxGgmlPath $OfxGgmlPath
        $firstBuild = $false
    }

    return
}

$enableCuda = $false
$enableVulkan = $false
$enableMetal = $false
$backendMode = "cpu-only"

if ($Cuda) {
    $enableCuda = $true
    $backendMode = "cuda"
} elseif ($Vulkan) {
    $enableVulkan = $true
    $backendMode = "vulkan"
} elseif ($Metal) {
    $enableMetal = $true
    $backendMode = "metal"
}

if ($buildDirWasImplicit) {
    if ((Test-IsWindowsHost) -and $enableVulkan) {
        $BuildDir = Get-ShortBuildDir -AddonRoot $addonRoot -DryRun:$DryRun
    } else {
        $BuildDir = Join-Path $addonRoot 'libs\stable-diffusion\build'
    }
}

$cmake = Get-CommandPathOrNull 'cmake.exe'
if (-not $cmake) {
    throw "cmake.exe was not found in PATH."
}

$effectiveSourceReleaseTag = if ([string]::IsNullOrWhiteSpace($SourceReleaseTag)) { $DefaultSourceReleaseTag } else { $SourceReleaseTag }
$releaseMetadata = if ($DryRun) {
    New-DryRunReleaseMetadata `
        -Tag $effectiveSourceReleaseTag `
        -Repository 'https://github.com/leejet/stable-diffusion.cpp'
} else {
    Get-ReleaseMetadata -Tag $effectiveSourceReleaseTag
}
$resolvedReleaseTag = $releaseMetadata.tag_name
$downloadRoot = Join-Path $env:TEMP 'ofxsd-source-release'
$cloneRoot = Join-Path $downloadRoot ('clone-' + $resolvedReleaseTag)
$git = Require-GitPath

if (-not $SkipSourceRefresh) {
    Write-Step "Refreshing stable-diffusion source from upstream snapshot"
    Write-Host ("    Release tag: {0}" -f $resolvedReleaseTag)
    Write-Host ("    Destination: {0}" -f $SourceDir)

    if ($DryRun) {
        Write-Host ("Clone https://github.com/leejet/stable-diffusion.cpp.git tag {0} with submodules" -f $resolvedReleaseTag)
        Write-Host ("Clone to {0}" -f $cloneRoot)
        Write-Host ("Replace contents of {0}" -f $SourceDir)
    } else {
        New-Item -ItemType Directory -Force -Path $downloadRoot | Out-Null
        if (Test-Path -LiteralPath $cloneRoot) {
            Remove-Item -LiteralPath $cloneRoot -Recurse -Force
        }

        Invoke-External -FilePath $git -Arguments @(
            'clone',
            '--depth', '1',
            '--branch', $resolvedReleaseTag,
            '--recurse-submodules',
            '--shallow-submodules',
            'https://github.com/leejet/stable-diffusion.cpp.git',
            $cloneRoot
        )

        $cmakeLists = Join-Path $cloneRoot 'CMakeLists.txt'
        if (-not (Test-Path -LiteralPath $cmakeLists)) {
            throw "The recursive clone for '$resolvedReleaseTag' did not contain a top-level CMakeLists.txt."
        }

        New-Item -ItemType Directory -Force -Path $SourceDir | Out-Null
        Remove-DirectoryContents -LiteralPath $SourceDir

        Get-ChildItem -LiteralPath $cloneRoot -Force |
            ForEach-Object {
                if ($_.Name -eq '.git') {
                    return
                }
                Copy-Item -LiteralPath $_.FullName -Destination $SourceDir -Recurse -Force
            }

        $resolvedCommit = Get-GitHeadCommit -GitPath $git -RepositoryRoot $cloneRoot
        Write-VendorPinFile `
            -Path (Join-Path $SourceDir 'OFX_VENDOR_PIN.txt') `
            -ReleaseMetadata $releaseMetadata `
            -Repository 'https://github.com/leejet/stable-diffusion.cpp' `
            -ResolvedCommit $resolvedCommit
    }

    if ($UseBundledGgml) {
        Refresh-GgmlVendorTree -GitPath $git -Tag $GgmlReleaseTag -TargetDir (Join-Path $SourceDir 'ggml') -DryRun:$DryRun
    }
} else {
    Write-Step "Using existing vendored stable-diffusion source snapshot"
    Write-Host ("    Source dir: {0}" -f $SourceDir)
}

if (-not (Test-Path -LiteralPath $SourceDir)) {
    if ($DryRun) {
        Write-Host "Dry run: source directory is not staged yet; configure command is shown for the planned path."
    } else {
        throw @"
stable-diffusion.cpp source was not found at:
  $SourceDir

Recommended workflow:
  1. Re-run scripts/build-stable-diffusion.ps1 so it can refresh the latest upstream source snapshot
  2. Re-run scripts/build-stable-diffusion.ps1

This addon uses ofxGgmlCore as the default ggml provider. Re-run with
-UseBundledGgml only when you need the compatibility fallback.
"@
    }
}

$sourceCmakeLists = Join-Path $SourceDir 'CMakeLists.txt'
if (-not (Test-Path -LiteralPath $sourceCmakeLists)) {
    if ($DryRun) {
        Write-Host "Dry run: CMakeLists.txt is not staged yet; source refresh would provide it."
    } else {
        throw "No CMakeLists.txt was found in $SourceDir. Vendor the full stable-diffusion.cpp source tree first."
    }
}

if ($UseBundledGgml) {
    Apply-GgufExtraDimensionFoldPatch -SourceDir $SourceDir -DryRun:$DryRun
}

# Handle system GGML configuration
if ($UseSystemGgml) {
    # Set default provider path if not provided. Core is the managed ecosystem default.
    if ([string]::IsNullOrWhiteSpace($OfxGgmlPath)) {
        $OfxGgmlPath = [System.IO.Path]::Combine($AddonRoot, '..', 'ofxGgmlCore')
    }

    # Convert to absolute path
    $OfxGgmlPath = [System.IO.Path]::GetFullPath($OfxGgmlPath)

    # Validate provider exists
    if (-not (Test-Path -LiteralPath $OfxGgmlPath)) {
        throw @"
Core ggml provider not found at:
  $OfxGgmlPath

Use -OfxGgmlPath to specify an ofxGgmlCore or compatible provider location.
"@
    }

    $providerManifest = Get-CoreProviderManifest -ProviderPath $OfxGgmlPath
    if ($providerManifest) {
        $ofxGgmlIncludeDir = [string]$providerManifest.GgmlIncludeDir
        $ofxGgmlLibDir = [string]$providerManifest.GgmlLibDir
        if (-not [bool]$providerManifest.ReadyForCompanions) {
            throw "Core ggml provider is not ready for companions. Run ofxGgmlCore\scripts\setup-ggml.ps1 first."
        }
    } else {
        $ofxGgmlIncludeDir = [System.IO.Path]::Combine($OfxGgmlPath, 'libs', 'ggml', 'include')
        $ofxGgmlLibDir = [System.IO.Path]::Combine($OfxGgmlPath, 'libs', 'ggml', 'lib')
    }

    # Check for GGML headers
    if (-not (Test-Path -LiteralPath $ofxGgmlIncludeDir)) {
        throw @"
System GGML headers not found at:
  $ofxGgmlIncludeDir

Build ofxGgmlCore first.
"@
    }

    Write-Step "Using system GGML from ofxGgmlCore-compatible provider"
    Write-Host ("    provider path: {0}" -f $OfxGgmlPath)
    Write-Host ("    provider manifest: {0}" -f ($(if ($providerManifest) { 'ON' } else { 'OFF' })))
}

if ($Clean -and (Test-Path -LiteralPath $BuildDir)) {
    Write-Step "Cleaning previous stable-diffusion build"
    if (-not $DryRun) {
        Remove-Item -LiteralPath $BuildDir -Recurse -Force
    }
}

if (-not $DryRun) {
    New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
    New-Item -ItemType Directory -Force -Path $InstallIncludeDir | Out-Null
    New-Item -ItemType Directory -Force -Path $InstallLibDir | Out-Null
    New-Item -ItemType Directory -Force -Path $InstallBinDir | Out-Null
    if ($stageBundledGgml) {
        New-Item -ItemType Directory -Force -Path $InstallGgmlIncludeDir | Out-Null
        New-Item -ItemType Directory -Force -Path $InstallGgmlLibDir | Out-Null
    }
    New-Item -ItemType Directory -Force -Path $VariantRootDir | Out-Null
    foreach ($binDir in $exampleBinDirs) {
        New-Item -ItemType Directory -Force -Path $binDir | Out-Null
    }
}

$vendorPinPath = Join-Path $SourceDir 'OFX_VENDOR_PIN.txt'
$vendoredCommit = Get-VendoredValue -Path $vendorPinPath -Prefix 'Upstream commit:'
$vendoredTargetCommit = Get-VendoredValue -Path $vendorPinPath -Prefix 'Upstream target commitish:'
$vendoredReleaseTag = Get-VendoredValue -Path $vendorPinPath -Prefix 'Upstream release tag:'
$vendoredVersion = $null
if ($vendoredCommit) {
    $vendoredVersion = "vendored-$($vendoredCommit.Substring(0, [Math]::Min(7, $vendoredCommit.Length)))"
} elseif ($vendoredTargetCommit) {
    $vendoredVersion = "vendored-$($vendoredTargetCommit.Substring(0, [Math]::Min(7, $vendoredTargetCommit.Length)))"
} elseif ($vendoredReleaseTag) {
    $vendoredVersion = "release-$vendoredReleaseTag"
}

$configureArgs = @(
    '-S', $SourceDir,
    '-B', $BuildDir,
    "-DCMAKE_BUILD_TYPE=$Configuration",
    '-DSD_BUILD_SHARED_LIBS=ON',
    ('-DSD_BUILD_EXAMPLES=' + ($(if ($BuildCli) { 'ON' } else { 'OFF' })))
)

if (Test-IsWindowsHost) {
    $configureArgs += '-DCMAKE_CXX_FLAGS=/bigobj'
}

if ($vendoredCommit -or $vendoredTargetCommit) {
    $cmakeVendoredCommit = if ($vendoredCommit) { $vendoredCommit } else { $vendoredTargetCommit }
    $configureArgs += @(
        "-DSDCPP_BUILD_COMMIT=$cmakeVendoredCommit",
        "-DSDCPP_BUILD_VERSION=$vendoredVersion"
    )
}

if (-not [string]::IsNullOrWhiteSpace($Generator)) {
    $configureArgs += @('-G', $Generator)
}

if ((Test-IsWindowsHost) -and ($enableCuda) -and
    ([string]::IsNullOrWhiteSpace($Generator) -or $Generator -like 'Visual Studio*')) {
    $configureArgs += @('-A', 'x64')
    if ($env:CUDA_PATH -and (Test-Path -LiteralPath $env:CUDA_PATH)) {
        $configureArgs += @('-T', "cuda=$env:CUDA_PATH")
    }
}

$configureArgs += @(
    ('-DSD_CUDA=' + ($(if ($enableCuda) { 'ON' } else { 'OFF' }))),
    ('-DSD_VULKAN=' + ($(if ($enableVulkan) { 'ON' } else { 'OFF' }))),
    ('-DSD_METAL=' + ($(if ($enableMetal) { 'ON' } else { 'OFF' })))
    '-DSD_WEBP=ON',
    '-DSD_WEBM=ON'
)

if ($enableCuda -and -not [string]::IsNullOrWhiteSpace($CudaArchitectures)) {
    $configureArgs += "-DCMAKE_CUDA_ARCHITECTURES=$CudaArchitectures"
}

# Add system GGML configuration if requested
if ($UseSystemGgml) {
    $ofxGgmlCmakeDir = [System.IO.Path]::Combine($BuildDir, 'ofxggml-cmake')
    $ofxGgmlGgmlDir = Join-Path $ofxGgmlCmakeDir 'ggml'
    if (-not $DryRun) {
        $ofxGgmlGgmlDir = New-OfxGgmlCmakePackage `
            -OfxGgmlPath $OfxGgmlPath `
            -PackageRoot $ofxGgmlCmakeDir `
            -EnableCuda:$enableCuda `
            -EnableVulkan:$enableVulkan
    }

    $configureArgs += @(
        '-DSD_USE_SYSTEM_GGML=ON',
        "-DCMAKE_PREFIX_PATH=$ofxGgmlCmakeDir",
        "-Dggml_DIR=$ofxGgmlGgmlDir"
    )
}

Write-Step "Configuring stable-diffusion native library"
Write-Host ("    Backend mode: {0} (CUDA={1}, Vulkan={2}, Metal={3})" -f
    $backendMode,
    $(if ($enableCuda) { 'ON' } else { 'OFF' }),
    $(if ($enableVulkan) { 'ON' } else { 'OFF' }),
    $(if ($enableMetal) { 'ON' } else { 'OFF' }))
if ($enableCuda -and -not [string]::IsNullOrWhiteSpace($CudaArchitectures)) {
    Write-Host ("    CUDA architectures: {0}" -f $CudaArchitectures)
}
if ($UseSystemGgml) {
    Write-Host ("    System GGML: ON (from {0})" -f $OfxGgmlPath)
} else {
    Write-Host "    System GGML: OFF (using bundled)"
}
if ($buildDirWasImplicit -and (Test-IsWindowsHost) -and $enableVulkan) {
    Write-Host "    Using short Windows build dir for Vulkan: $BuildDir"
}
if ($DryRun) {
    Write-Host "$cmake $($configureArgs -join ' ')"
} else {
    Invoke-External -FilePath $cmake -Arguments $configureArgs
}

$useVisualStudioBuild = (Test-IsWindowsHost) -and ([string]::IsNullOrWhiteSpace($Generator) -or $Generator -like 'Visual Studio*')
$buildArgs = @(
    '--build', $BuildDir,
    '--config', $Configuration,
    '--parallel', $Jobs
)
$msbuild = $null
$msbuildArgs = @()
if ($useVisualStudioBuild) {
    $msbuild = Require-MSBuildPath
    $msbuildArgs = @(
        (Join-Path $BuildDir 'ALL_BUILD.vcxproj'),
        "/p:Configuration=$Configuration",
        '/p:Platform=x64',
        '/m:1',
        '/nr:false',
        '/v:m',
        '/p:TrackFileAccess=false',
        '/p:UseMultiToolTask=false'
    )
}

Write-Step "Building stable-diffusion ($Configuration)"
if ($DryRun) {
    if ($useVisualStudioBuild) {
        Write-Host "$msbuild $($msbuildArgs -join ' ')"
    } else {
        Write-Host "$cmake $($buildArgs -join ' ')"
    }
} else {
    if ($useVisualStudioBuild) {
        $msbuildFailure = $null
        try {
            Invoke-MSBuildFreshPowerShell -MSBuildPath $msbuild -Arguments $msbuildArgs
        } catch {
            Write-Warning ("MSBuild failed on the first attempt; retrying once. {0}" -f $_.Exception.Message)
            Start-Sleep -Seconds 1
            try {
                Invoke-MSBuildFreshPowerShell -MSBuildPath $msbuild -Arguments $msbuildArgs
            } catch {
                $msbuildFailure = $_.Exception.Message
            }
        }
        if ($msbuildFailure) {
            $existingDllPath = Find-FirstFile -Root $BuildDir -Names @('stable-diffusion.dll')
            $existingLibPath = Find-FirstFile -Root $BuildDir -Names @('stable-diffusion.lib')
            if ($existingDllPath -and $existingLibPath) {
                Write-Warning ("MSBuild returned an error after producing stable-diffusion artifacts; continuing with existing build outputs. {0}" -f $msbuildFailure)
            } else {
                throw $msbuildFailure
            }
        }
    } else {
        Invoke-External -FilePath $cmake -Arguments $buildArgs
    }
}

if ($DryRun) {
    return
}

$headerPath = Find-FirstFile -Root $SourceDir -Names @('stable-diffusion.h')
$ggmlIncludeSourceDir = Join-Path $SourceDir 'ggml\include'
$dllPath = Find-FirstFile -Root $BuildDir -Names @('stable-diffusion.dll')
$libPath = Find-FirstFile -Root $BuildDir -Names @('stable-diffusion.lib')
$ggmlLibPath = Find-FirstFile -Root $BuildDir -Names @('ggml.lib')
$ggmlBaseLibPath = Find-FirstFile -Root $BuildDir -Names @('ggml-base.lib')
$ggmlCpuLibPath = Find-FirstFile -Root $BuildDir -Names @('ggml-cpu.lib')
$ggmlCudaLibPath = Find-FirstFile -Root $BuildDir -Names @('ggml-cuda.lib')
$ggmlVulkanLibPath = Find-FirstFile -Root $BuildDir -Names @('ggml-vulkan.lib')
$ggmlMetalLibPath = Find-FirstFile -Root $BuildDir -Names @('ggml-metal.lib')
$webpLibPath = Find-FirstFile -Root $BuildDir -Names @('libwebp.lib', 'webp.lib')
$webpmuxLibPath = Find-FirstFile -Root $BuildDir -Names @('libwebpmux.lib', 'webpmux.lib')
$sharpYuvLibPath = Find-FirstFile -Root $BuildDir -Names @('libsharpyuv.lib', 'sharpyuv.lib')
$webmLibPath = Find-FirstFile -Root $BuildDir -Names @('webm.lib', 'libwebm.lib')
$sdCliPath = Find-FirstFile -Root $BuildDir -Names @('sd-cli.exe')

if (-not $headerPath) {
    throw "Build completed, but stable-diffusion.h was not found under $SourceDir."
}
if (-not $dllPath) {
    throw "Build completed, but stable-diffusion.dll was not found under $BuildDir."
}
if (-not $libPath) {
    throw "Build completed, but stable-diffusion.lib was not found under $BuildDir."
}
if ($BuildCli -and -not $sdCliPath) {
    throw "Build completed, but sd-cli.exe was not found under $BuildDir even though -BuildCli was requested."
}

$optionalSupportLibs = @(
    @{ Name = 'libwebp.lib'; Path = $webpLibPath },
    @{ Name = 'libwebpmux.lib'; Path = $webpmuxLibPath },
    @{ Name = 'libsharpyuv.lib'; Path = $sharpYuvLibPath },
    @{ Name = 'webm.lib'; Path = $webmLibPath }
)
$missingSupportLibs = @($optionalSupportLibs | Where-Object { -not $_.Path })
if ($missingSupportLibs.Count -gt 0) {
    $missingNames = ($missingSupportLibs | ForEach-Object { $_.Name }) -join ', '
    Write-Warning "Optional WebP/WebM support libraries were not found under $BuildDir ($missingNames). Upstream builds may fold these into the main target or avoid staging standalone import libraries, so packaging will continue with the core stable-diffusion artifacts."
}

Write-Step "Staging stable-diffusion artifacts into the addon"
Copy-IfPresent -Path $headerPath -Destination $InstallIncludeDir
Copy-IfPresent -Path $dllPath -Destination $InstallLibDir
Copy-IfPresent -Path $libPath -Destination $InstallLibDir
Copy-IfPresent -Path $sdCliPath -Destination $InstallBinDir
Copy-IfPresent -Path $webpLibPath -Destination $InstallLibDir
Copy-IfPresent -Path $webpmuxLibPath -Destination $InstallLibDir
Copy-IfPresent -Path $sharpYuvLibPath -Destination $InstallLibDir
Copy-IfPresent -Path $webmLibPath -Destination $InstallLibDir
foreach ($binDir in $exampleBinDirs) {
    Copy-IfPresent -Path $dllPath -Destination $binDir -AllowLockedDestination
}

if ($stageBundledGgml) {
    Write-Step "Staging ggml artifacts into the addon"
    if (Test-Path -LiteralPath $ggmlIncludeSourceDir) {
        Get-ChildItem -LiteralPath $ggmlIncludeSourceDir -File |
            ForEach-Object {
                Copy-IfPresent -Path $_.FullName -Destination $InstallGgmlIncludeDir
            }
    } else {
        Write-Warning "ggml headers were not found under $ggmlIncludeSourceDir. Separate ggml headers will not be staged."
    }
    Copy-IfPresent -Path $ggmlLibPath -Destination $InstallGgmlLibDir
    Copy-IfPresent -Path $ggmlBaseLibPath -Destination $InstallGgmlLibDir
    Copy-IfPresent -Path $ggmlCpuLibPath -Destination $InstallGgmlLibDir
    Copy-IfPresent -Path $ggmlCudaLibPath -Destination $InstallGgmlLibDir
    Copy-IfPresent -Path $ggmlVulkanLibPath -Destination $InstallGgmlLibDir
    Copy-IfPresent -Path $ggmlMetalLibPath -Destination $InstallGgmlLibDir
} else {
    Write-Step "Skipping bundled ggml staging because Core/system ggml is enabled"
}

$variantGgmlStableDiffusionIncludeDir = Join-Path $VariantRootDir "$backendMode\stable-diffusion\include"
$variantGgmlStableDiffusionLibDir = Join-Path $VariantRootDir "$backendMode\stable-diffusion\lib\vs"
$variantGgmlStableDiffusionBinDir = Join-Path $VariantRootDir "$backendMode\stable-diffusion\bin\vs"
if ($stageBundledGgml) {
    $variantGgmlIncludeDir = Join-Path $VariantRootDir "$backendMode\ggml\include"
    $variantGgmlLibDir = Join-Path $VariantRootDir "$backendMode\ggml\lib\vs"
}

Write-Step "Snapshotting backend variant artifacts"
Copy-DirectoryContents -Source $InstallIncludeDir -Destination $variantGgmlStableDiffusionIncludeDir
Copy-DirectoryContents -Source $InstallLibDir -Destination $variantGgmlStableDiffusionLibDir
Copy-DirectoryContents -Source $InstallBinDir -Destination $variantGgmlStableDiffusionBinDir
if ($stageBundledGgml) {
    Copy-DirectoryContents -Source $InstallGgmlIncludeDir -Destination $variantGgmlIncludeDir
    Copy-DirectoryContents -Source $InstallGgmlLibDir -Destination $variantGgmlLibDir
}

Write-Host ""
Write-Host "stable-diffusion native build complete."
Write-Host "  source:  $SourceDir"
Write-Host "  build:   $BuildDir"
Write-Host "  include: $InstallIncludeDir"
Write-Host "  libs:    $InstallLibDir"
Write-Host "  bins:    $InstallBinDir"
if ($stageBundledGgml) {
    Write-Host "  ggml include: $InstallGgmlIncludeDir"
    Write-Host "  ggml libs:    $InstallGgmlLibDir"
} else {
    Write-Host "  ggml provider: $OfxGgmlPath"
}
Write-Host "  variant snapshot: $VariantRootDir\$backendMode"
if ($exampleBinDirs.Count -gt 0) {
    Write-Host "  runtimes:"
    foreach ($binDir in $exampleBinDirs) {
        Write-Host "    $binDir"
    }
}


