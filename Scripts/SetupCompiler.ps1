# Makes this PC build UE 5.5 with the compiler it was made for, whatever Visual Studio updates install later.
#
# Visual Studio 2026 updates replace its C++ compiler (e.g. with MSVC 14.51), which can't compile parts of UE 5.5's
# engine source (missing <hash_map>, stricter warnings treated as errors). This script:
#   1. finds your Visual Studio installation(s),
#   2. adds the MSVC 14.38 build tools (Epic's recommendation for UE 5.5; falls back to 14.44) through Visual Studio's
#      own installer, next to what you have (asks for administrator rights once),
#   3. pins Unreal to it in %APPDATA%\Unreal Engine\UnrealBuildTool\BuildConfiguration.xml.
# After that, run BuildEditor.bat. The first build rebuilds the engine once with the pinned compiler.
#
# Usage (from the repo root or Scripts):  powershell -ExecutionPolicy Bypass -File Scripts\SetupCompiler.ps1
param(
    # Pin a specific installed version instead, e.g. 14.38.33130.
    [string]$Version = ""
)

$ErrorActionPreference = "Stop"

$installerDir = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer"
$vswhere = Join-Path $installerDir "vswhere.exe"
$setup = Join-Path $installerDir "setup.exe"
if (-not (Test-Path $vswhere)) { throw "Visual Studio Installer not found ($vswhere). Install Visual Studio first." }

# Visual Studio installations (any edition, incl. Build Tools).
$installs = @(& $vswhere -all -products * -format value -property installationPath) | Where-Object { $_ }
if (-not $installs) { throw "No Visual Studio installation found." }
Write-Host "Visual Studio installations:"; $installs | ForEach-Object { Write-Host "  $_" }

function Get-Toolsets {
    $found = @()
    foreach ($install in $installs) {
        $msvc = Join-Path $install "VC\Tools\MSVC"
        if (Test-Path $msvc) { $found += Get-ChildItem $msvc -Directory | ForEach-Object { $_.Name } }
    }
    return $found | Sort-Object -Unique
}

# Preferred toolsets for UE 5.5, best first: folder prefix and Visual Studio Installer component ID.
$preferred = @(
    @{ Prefix = "14.38."; Component = "Microsoft.VisualStudio.Component.VC.14.38.17.8.x86.x64" },
    @{ Prefix = "14.44."; Component = "Microsoft.VisualStudio.Component.VC.14.44.17.14.x86.x64" }
)

$pinned = $Version
if (-not $pinned) {
    $toolsets = Get-Toolsets
    Write-Host "Installed compilers: $($toolsets -join ', ')"

    foreach ($choice in $preferred) {
        $match = $toolsets | Where-Object { $_.StartsWith($choice.Prefix) } | Select-Object -Last 1
        if ($match) { $pinned = $match; break }
    }

    if (-not $pinned) {
        # Add the preferred toolset to the newest installation, through the Visual Studio Installer (elevated).
        $target = @(& $vswhere -latest -products * -format value -property installationPath)[0]
        foreach ($choice in $preferred) {
            Write-Host "Installing $($choice.Component) into $target (a Visual Studio Installer window may appear)..."
            $process = Start-Process -FilePath $setup -Verb RunAs -Wait -PassThru -ArgumentList @(
                "modify", "--installPath", "`"$target`"", "--add", $choice.Component, "--passive", "--norestart")
            $toolsets = Get-Toolsets
            $match = $toolsets | Where-Object { $_.StartsWith($choice.Prefix) } | Select-Object -Last 1
            if ($match) { $pinned = $match; break }
            Write-Host "  Not installed (installer exit code $($process.ExitCode)); trying the next option."
        }
    }

    if (-not $pinned) {
        throw "Couldn't install MSVC 14.38 or 14.44. Install 'Build Tools for Visual Studio 2022' with the 'MSVC v143 - VS 2022 C++ x64/x86 build tools (v14.38-17.8)' component, then run this script again."
    }
}

# Pin Unreal to it (replaces the whole file, so any other settings in it are dropped; the old file is kept as .bak).
$configDir = Join-Path $env:APPDATA "Unreal Engine\UnrealBuildTool"
$configFile = Join-Path $configDir "BuildConfiguration.xml"
New-Item -ItemType Directory -Force -Path $configDir | Out-Null
if (Test-Path $configFile) {
    Copy-Item $configFile "$configFile.bak" -Force
    Write-Host "Backed up the existing $configFile to BuildConfiguration.xml.bak"
}
@"
<?xml version="1.0" encoding="utf-8" ?>
<Configuration xmlns="https://www.unrealengine.com/BuildConfiguration">
  <WindowsPlatform>
    <CompilerVersion>$pinned</CompilerVersion>
  </WindowsPlatform>
</Configuration>
"@ | Set-Content -Path $configFile -Encoding UTF8

Write-Host ""
Write-Host "Unreal now builds with MSVC $pinned ($configFile)."
Write-Host "Next: run Scripts\BuildEditor.bat. The first build recompiles the engine once with this compiler."
