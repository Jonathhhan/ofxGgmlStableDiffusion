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
        [string]$ProjectPath,
        [string]$ExpectedProjectFile
    )

    $pgOfRoot = Convert-ToShortPath $OfRoot
    $pgProjectPath = Convert-ToShortPath $ProjectPath

    & $ProjectGenerator `
        (Convert-ToPgOption "o" $pgOfRoot) `
        (Convert-ToPgOption "p" $PlatformName) `
        $pgProjectPath
    if ($LASTEXITCODE -ne 0) {
        if (-not [string]::IsNullOrWhiteSpace($ExpectedProjectFile) -and
            (Test-Path -LiteralPath $ExpectedProjectFile -PathType Leaf)) {
            Write-Warning "projectGenerator exited with $LASTEXITCODE for $ProjectPath after creating $ExpectedProjectFile; continuing with generated project assertions."
            return
        }
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

function Add-MapValue {
    param(
        [hashtable]$Map,
        [string]$Name,
        [string]$Value
    )
    if ([string]::IsNullOrWhiteSpace($Name) -or [string]::IsNullOrWhiteSpace($Value)) {
        return
    }
    if (!$Map.ContainsKey($Name)) {
        $Map[$Name] = New-Object System.Collections.Generic.List[string]
    }
    if (!$Map[$Name].Contains($Value)) {
        $Map[$Name].Add($Value)
    }
}

function Get-AddonConfigValues {
    param([string]$AddonRoot)

    $values = @{}
    $configPath = Join-Path $AddonRoot "addon_config.mk"
    if (!(Test-Path -LiteralPath $configPath -PathType Leaf)) {
        return $values
    }

    $section = ""
    Get-Content -LiteralPath $configPath | ForEach-Object {
        $line = ([string]$_ -replace "\s+#.*$", "").Trim()
        if ([string]::IsNullOrWhiteSpace($line)) {
            return
        }
        if ($line -match "^([A-Za-z0-9_/]+):\s*$") {
            $section = $matches[1]
            return
        }
        if ($section -ne "common" -and $section -ne "vs") {
            return
        }
        if ($line -match "^(ADDON_[A-Z_]+)\s*(?:\+)?=\s*(.+?)\s*$") {
            $name = $matches[1]
            foreach ($part in @($matches[2] -split "\s+" | Where-Object { ![string]::IsNullOrWhiteSpace([string]$_) })) {
                Add-MapValue -Map $values -Name $name -Value ([string]$part).Trim('"')
            }
        }
    }

    return $values
}

function Get-RelativePath {
    param(
        [string]$FromDirectory,
        [string]$ToPath
    )
    $fromUri = [System.Uri]((Resolve-Path -LiteralPath $FromDirectory).Path.TrimEnd("\") + "\")
    $toUri = [System.Uri](Resolve-Path -LiteralPath $ToPath).Path
    return [System.Uri]::UnescapeDataString($fromUri.MakeRelativeUri($toUri).ToString()).Replace("/", "\")
}

function Add-SemicolonValue {
    param(
        [System.Xml.XmlNode[]]$Nodes,
        [string]$Value
    )
    if ([string]::IsNullOrWhiteSpace($Value)) {
        return
    }
    foreach ($node in @($Nodes)) {
        $parts = New-Object System.Collections.Generic.List[string]
        foreach ($part in @($node.InnerText -split ";" | Where-Object { ![string]::IsNullOrWhiteSpace([string]$_) })) {
            $parts.Add([string]$part)
        }
        if (!$parts.Contains($Value)) {
            $parts.Add($Value)
            $node.InnerText = ($parts.ToArray() -join ";")
        }
    }
}

function Add-CompilerOption {
    param(
        [System.Xml.XmlNode[]]$Nodes,
        [string]$Option
    )
    if ([string]::IsNullOrWhiteSpace($Option)) {
        return
    }
    foreach ($node in @($Nodes)) {
        $parts = New-Object System.Collections.Generic.List[string]
        foreach ($part in @($node.InnerText -split "\s+" | Where-Object { ![string]::IsNullOrWhiteSpace([string]$_) })) {
            $parts.Add([string]$part)
        }
        if (!$parts.Contains($Option)) {
            $parts.Add($Option)
            $node.InnerText = ($parts.ToArray() -join " ")
        }
    }
}

function Add-ProjectItem {
    param(
        [xml]$Doc,
        [System.Xml.XmlNamespaceManager]$Namespace,
        [string]$Tag,
        [string]$Include
    )
    if ([string]::IsNullOrWhiteSpace($Include)) {
        return
    }
    if ($Doc.SelectSingleNode("//msb:$Tag[@Include='$Include']", $Namespace)) {
        return
    }
    $itemGroups = @($Doc.SelectNodes("//msb:ItemGroup", $Namespace))
    if ($itemGroups.Count -eq 0) {
        return
    }
    $item = $Doc.CreateElement($Tag, $Doc.DocumentElement.NamespaceURI)
    $item.SetAttribute("Include", $Include)
    [void]$itemGroups[0].AppendChild($item)
}

function ConvertTo-LibraryReference {
    param(
        [string]$AddonRoot,
        [string]$ProjectDir,
        [string]$Library
    )
    $value = ([string]$Library).Trim().Trim('"')
    if ([string]::IsNullOrWhiteSpace($value)) {
        return $null
    }
    $normalized = $value -replace "/", "\"
    $name = [System.IO.Path]::GetFileName($normalized)
    $parent = Split-Path -Parent $normalized
    $directory = ""
    if (![string]::IsNullOrWhiteSpace($parent)) {
        if ([System.IO.Path]::IsPathRooted($parent) -or $parent -match '^\$\(') {
            $directory = $parent
        } else {
            $candidate = Join-Path $AddonRoot $parent
            if (Test-Path -LiteralPath $candidate) {
                $directory = Get-RelativePath -FromDirectory $ProjectDir -ToPath $candidate
            } else {
                $directory = $parent
            }
        }
    }
    return [pscustomobject]@{
        Dependency = $name
        Directory = $directory
    }
}

function Get-AddonRoot {
    param(
        [string]$Addon,
        [string]$OwnerAddon,
        [string]$OwnerRoot,
        [string]$OfRoot
    )
    if ($Addon -eq $OwnerAddon) {
        return $OwnerRoot
    }
    return Join-Path (Join-Path $OfRoot "addons") $Addon
}

function Get-AddonSourceFiles {
    param(
        [string]$Addon,
        [string]$AddonRoot,
        [hashtable]$Config
    )
    $files = New-Object System.Collections.Generic.List[System.IO.FileInfo]
    if ($Config.ContainsKey("ADDON_SOURCES") -and $Config["ADDON_SOURCES"].Count -gt 0) {
        foreach ($source in @($Config["ADDON_SOURCES"])) {
            $path = Join-Path $AddonRoot ([string]$source)
            if (Test-Path -LiteralPath $path -PathType Leaf) {
                $files.Add((Get-Item -LiteralPath $path))
            }
        }
        return @($files)
    }

    $roots = @("src")
    if ($Addon -eq "ofxImGui") {
        $roots = @("src", "libs\imgui\src", "libs\imgui\backends", "libs\imgui\extras")
    }
    foreach ($root in $roots) {
        $path = Join-Path $AddonRoot $root
        if (Test-Path -LiteralPath $path -PathType Container) {
            Get-ChildItem -LiteralPath $path -Recurse -File |
                Where-Object { $_.Extension -in @(".cpp", ".cc", ".cxx") } |
                ForEach-Object { $files.Add($_) }
        }
    }
    return @($files)
}

function Repair-GeneratedProject {
    param(
        [string]$ProjectFile,
        [string]$ExamplePath,
        [string]$OwnerAddon,
        [string]$OwnerRoot,
        [string]$OfRoot
    )
    if (!(Test-Path -LiteralPath $ProjectFile -PathType Leaf)) {
        return
    }
    [xml]$doc = Get-Content -LiteralPath $ProjectFile -Raw
    $ns = New-Object System.Xml.XmlNamespaceManager($doc.NameTable)
    $ns.AddNamespace("msb", "http://schemas.microsoft.com/developer/msbuild/2003")
    $projectDir = Split-Path -Parent $ProjectFile
    $includeNodes = @($doc.SelectNodes("//msb:AdditionalIncludeDirectories", $ns))
    $dependencyNodes = @($doc.SelectNodes("//msb:AdditionalDependencies", $ns))
    $libraryDirNodes = @($doc.SelectNodes("//msb:AdditionalLibraryDirectories", $ns))
    $optionNodes = @($doc.SelectNodes("//msb:ClCompile/msb:AdditionalOptions", $ns))
    $addons = Get-Content -LiteralPath (Join-Path $ExamplePath "addons.make") |
        ForEach-Object { ([string]$_).Trim() } |
        Where-Object { ![string]::IsNullOrWhiteSpace([string]$_) }

    foreach ($addon in @($addons)) {
        $addonRoot = Get-AddonRoot -Addon $addon -OwnerAddon $OwnerAddon -OwnerRoot $OwnerRoot -OfRoot $OfRoot
        if (!(Test-Path -LiteralPath $addonRoot -PathType Container)) {
            continue
        }
        $config = Get-AddonConfigValues -AddonRoot $addonRoot
        $includes = New-Object System.Collections.Generic.List[string]
        foreach ($include in @($config["ADDON_INCLUDES"])) {
            if (![string]::IsNullOrWhiteSpace([string]$include) -and !$includes.Contains([string]$include)) {
                $includes.Add([string]$include)
            }
        }
        if ($includes.Count -eq 0 -and (Test-Path -LiteralPath (Join-Path $addonRoot "src") -PathType Container)) {
            $includes.Add("src")
        }
        if ($addon -eq "ofxImGui") {
            foreach ($include in @("src", "libs", "libs\imgui", "libs\imgui\src", "libs\imgui\backends", "libs\imgui\extras")) {
                if (!$includes.Contains($include) -and (Test-Path -LiteralPath (Join-Path $addonRoot $include) -PathType Container)) {
                    $includes.Add($include)
                }
            }
        }
        foreach ($include in @($includes)) {
            $path = Join-Path $addonRoot $include
            if (Test-Path -LiteralPath $path) {
                Add-SemicolonValue -Nodes $includeNodes -Value (Get-RelativePath -FromDirectory $projectDir -ToPath $path)
            }
        }
        foreach ($cflag in @($config["ADDON_CFLAGS"])) {
            Add-CompilerOption -Nodes $optionNodes -Option ([string]$cflag)
        }
        foreach ($library in @($config["ADDON_LIBS"])) {
            $reference = ConvertTo-LibraryReference -AddonRoot $addonRoot -ProjectDir $projectDir -Library ([string]$library)
            if ($reference) {
                Add-SemicolonValue -Nodes $dependencyNodes -Value ([string]$reference.Dependency)
                Add-SemicolonValue -Nodes $libraryDirNodes -Value ([string]$reference.Directory)
            }
        }
        foreach ($file in @(Get-AddonSourceFiles -Addon $addon -AddonRoot $addonRoot -Config $config)) {
            Add-ProjectItem -Doc $doc -Namespace $ns -Tag "ClCompile" -Include (Get-RelativePath -FromDirectory $projectDir -ToPath $file.FullName)
        }
    }
    $doc.Save($ProjectFile)
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
            Where-Object {
                $_.Name -notin @(".vs", "bin", "obj", "icon.rc") -and
                $_.Extension -notin @(".sln", ".vcxproj", ".filters", ".user")
            } |
            Copy-Item -Destination $target -Recurse -Force
        New-Item -ItemType Directory -Path (Join-Path $target "bin") -Force | Out-Null
        $projectFile = Join-Path $target "$example.vcxproj"
        Invoke-ProjectGenerator `
            -ProjectGenerator $projectGenerator `
            -OfRoot $ofRoot `
            -PlatformName $Platform `
            -ProjectPath $target `
            -ExpectedProjectFile $projectFile

        if (-not (Test-Path -LiteralPath $projectFile -PathType Leaf)) {
            throw "projectGenerator did not create $projectFile"
        }
        Repair-GeneratedProject `
            -ProjectFile $projectFile `
            -ExamplePath $target `
            -OwnerAddon "ofxGgmlStableDiffusion" `
            -OwnerRoot $addonRoot `
            -OfRoot $ofRoot

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
