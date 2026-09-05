[CmdletBinding()]
param(
    [string]$Python = "py",
    [string]$QtVersion = "6.8.3",
    [string]$QtRoot = "",
    [ValidateSet("mingw_64", "win64_msvc2022_64")]
    [string]$Kit = "mingw_64"
)

$ErrorActionPreference = "Stop"
$workspace = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
if ([string]::IsNullOrWhiteSpace($QtRoot)) {
    $QtRoot = Join-Path $workspace ".toolchain\qt"
}

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "CMake is required. Install it with: scoop install cmake"
}

# Qt's official online installer is not needed for development. aqtinstall places
# an isolated LGPL Qt SDK under .toolchain so it is never mixed with F8Studio.
& $Python -m pip install --user --upgrade aqtinstall
if ($LASTEXITCODE -ne 0) { throw "Could not install aqtinstall." }
$aqtArchitecture = if ($Kit -eq "mingw_64") { "win64_mingw" } else { $Kit }
& $Python -m aqt install-qt windows desktop $QtVersion $aqtArchitecture `
    --outputdir $QtRoot --modules qtquick3d qtshadertools qtserialport qtwebsockets
if ($LASTEXITCODE -ne 0) { throw "Could not install the required Qt modules." }

$kit = Get-ChildItem -LiteralPath (Join-Path $QtRoot $QtVersion) -Directory |
    Where-Object { $_.Name -eq $Kit } | Select-Object -First 1
if ($null -eq $kit) { throw "Qt kit was installed but the requested desktop kit was not found." }

if ($Kit -eq "mingw_64") {
    $toolRoot = Join-Path $workspace ".toolchain\qt-tools"
    & $Python -m aqt install-tool windows desktop tools_mingw1310 --outputdir $toolRoot
    if ($LASTEXITCODE -ne 0) { throw "Could not install the matching MinGW compiler." }
    $compiler = Join-Path $toolRoot "Tools\mingw1310_64\bin\g++.exe"
    if (-not (Test-Path -LiteralPath $compiler)) { throw "MinGW compiler was not installed at $compiler" }
    Write-Output "MinGW compiler: $compiler"
}

Write-Output "Qt installed: $($kit.FullName)"
Write-Output "Build with: .\tools\Build-MotionBridge.ps1 -QtPrefix $($kit.FullName)"
