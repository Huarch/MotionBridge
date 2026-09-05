[CmdletBinding()]
param(
    [switch]$CoreOnly,
    [switch]$Portable,
    [ValidateSet("Debug", "Release")]
    [string]$Configuration = "Release",
    [string]$QtPrefix = $env:QT_PREFIX,
    [string]$MinGwRoot = ""
)

$ErrorActionPreference = "Stop"
$workspace = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$buildRelative = if ($CoreOnly) { "out\core" } else { "out\app-qt-mingw-v2" }
$buildDirectory = Join-Path $workspace $buildRelative
$configure = @("-S", $workspace, "-B", $buildDirectory, "-DMOTION_BRIDGE_BUILD_GUI=$(-not $CoreOnly)")
if (-not $CoreOnly) {
    if ([string]::IsNullOrWhiteSpace($QtPrefix)) {
        $candidate = Join-Path $workspace ".toolchain\qt\6.8.3\mingw_64"
        if (Test-Path -LiteralPath $candidate) { $QtPrefix = $candidate }
        else { throw "QtPrefix is required for the GUI build. Run Install-MotionBridgeToolchain.ps1 first, then pass -QtPrefix <Qt kit>." }
    }
    $configure += "-DCMAKE_PREFIX_PATH=$QtPrefix"
    # Keep the Qt runtime visible to CTest and to binaries launched while the
    # build is still in progress. This prevents misleading missing-DLL errors
    # for Qt-based integration tests and smoke runs.
    $env:PATH = "$(Join-Path $QtPrefix 'bin');$env:PATH"
}

if ([string]::IsNullOrWhiteSpace($MinGwRoot)) {
    $MinGwRoot = Join-Path $workspace ".toolchain\qt-tools\Tools\mingw1310_64\bin"
}
$compiler = Join-Path $MinGwRoot "g++.exe"
if (Test-Path -LiteralPath $compiler) {
    $env:PATH = "$MinGwRoot;$env:PATH"
    $configure += "-G"
    $configure += "MinGW Makefiles"
    $configure += "-DCMAKE_CXX_COMPILER=$compiler"
    $resourceCompiler = Join-Path $MinGwRoot "windres.exe"
    if (Test-Path -LiteralPath $resourceCompiler) {
        # windres crashes while launching its preprocessor when its own
        # executable path contains spaces. Preserve support for ordinary
        # Windows checkout paths by giving CMake the equivalent 8.3 path.
        $shortResourceCompiler = (& $env:ComSpec /d /c "for %I in (`"$resourceCompiler`") do @echo %~sI").Trim()
        if (-not [string]::IsNullOrWhiteSpace($shortResourceCompiler)) {
            $shortResourceCompiler = $shortResourceCompiler.Replace("\", "/")
            $configure += "-DCMAKE_RC_COMPILER=$shortResourceCompiler"
        }
    }
}

cmake @configure
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build $buildDirectory --config $Configuration
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
ctest --test-dir $buildDirectory -C $Configuration --output-on-failure --interactive-debug-mode 0
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if ($Portable -and -not $CoreOnly) {
    $deploy = Join-Path $QtPrefix "bin\windeployqt.exe"
    if (-not (Test-Path -LiteralPath $deploy)) { throw "windeployqt was not found in $QtPrefix" }
    # Always package from an isolated staging directory. This keeps an old
    # extracted portable copy (and its user config) out of the new ZIP.
    $portableDir = Join-Path $workspace "out\MotionBridge-portable-stage"
    if (Test-Path -LiteralPath $portableDir) {
        Remove-Item -LiteralPath $portableDir -Recurse -Force
    }
    New-Item -ItemType Directory -Path $portableDir -Force | Out-Null
    Copy-Item (Join-Path $buildDirectory "MotionBridge.exe") (Join-Path $portableDir "MotionBridge.exe") -Force
    # MotionBridge.exe already embeds this icon. Keep a standalone copy too,
    # so a user-created shortcut can use the same identity immediately.
    Copy-Item (Join-Path $workspace "assets\icons\motion-bridge.ico") (Join-Path $portableDir "MotionBridge.ico") -Force
    Copy-Item (Join-Path $workspace "portable.mode") (Join-Path $portableDir "portable.mode") -Force
    Copy-Item (Join-Path $workspace "README.md") (Join-Path $portableDir "README.md") -Force
    Copy-Item (Join-Path $workspace "README-ZH.md") (Join-Path $portableDir "README-ZH.md") -Force
    Copy-Item (Join-Path $workspace "THIRD_PARTY_NOTICES.md") (Join-Path $portableDir "THIRD_PARTY_NOTICES.md") -Force
    Copy-Item (Join-Path $workspace "assets\models\sr6\LICENSE-osr-emu.txt") (Join-Path $portableDir "LICENSE-osr-emu.txt") -Force
    # Deploy the native runtime and only the two useful Qt translation packs.
    # The application has its own Chinese translation embedded as a resource.
    # QML imports are copied explicitly below to omit unused control styles.
    & $deploy --no-quick-import --skip-plugin-types qmltooling,generic --translations en,zh_CN `
        --dir $portableDir (Join-Path $portableDir "MotionBridge.exe")
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    # --no-quick-import omits the QML module DLLs as intended. Copy the small
    # Basic-style dependency set explicitly, alongside the selected QML files.
    @(
        "Qt6QuickControls2.dll", "Qt6QuickControls2Basic.dll",
        "Qt6QuickControls2BasicStyleImpl.dll", "Qt6QuickControls2Impl.dll",
        "Qt6QuickTemplates2.dll", "Qt6QuickLayouts.dll"
    ) | ForEach-Object {
        Copy-Item (Join-Path $QtPrefix "bin\$_") (Join-Path $portableDir $_) -Force
    }

    $runtimeQml = Join-Path $QtPrefix "qml"
    $portableQml = Join-Path $portableDir "qml"
    New-Item -ItemType Directory -Path $portableQml -Force | Out-Null

    function Copy-QmlRootFiles([string]$relativePath) {
        $source = Join-Path $runtimeQml $relativePath
        $destination = Join-Path $portableQml $relativePath
        if (-not (Test-Path -LiteralPath $source)) { throw "Required QML module was not found: $source" }
        New-Item -ItemType Directory -Path $destination -Force | Out-Null
        Get-ChildItem -LiteralPath $source -File | Copy-Item -Destination $destination -Force
    }

    function Copy-QmlModule([string]$relativePath) {
        $source = Join-Path $runtimeQml $relativePath
        $destination = Join-Path $portableQml $relativePath
        if (-not (Test-Path -LiteralPath $source)) { throw "Required QML module was not found: $source" }
        Copy-Item -LiteralPath $source -Destination (Split-Path -Parent $destination) -Recurse -Force
    }

    # The UI forces Qt Quick Controls' Basic style at startup. Keeping only
    # this dependency closure avoids bundling every built-in style and its
    # assets (the main source of the previous thousand-file release).
    @("QML", "QtQml", "QtQuick", "QtQuick3D", "QtQuick\Controls") |
        ForEach-Object { Copy-QmlRootFiles $_ }
    @(
        "QtQml\Models", "QtQml\WorkerScript",
        "QtQuick\Controls\Basic", "QtQuick\Controls\Basic\impl", "QtQuick\Controls\impl",
        "QtQuick\Templates", "QtQuick\Window", "QtQuick\Layouts", "QtQuick\Dialogs", "QtQuick\Shapes"
    ) | ForEach-Object { Copy-QmlModule $_ }

    # Keep the user's portable config beside the application, but never leak it
    # into a distributable ZIP. Each extracted copy creates/migrates its own INI.
    $packagePaths = Get-ChildItem -LiteralPath $portableDir |
        Where-Object { $_.Name -ne "config" } |
        ForEach-Object { $_.FullName }
    Compress-Archive -Path $packagePaths -DestinationPath (Join-Path $workspace "dist\MotionBridge-portable.zip") -Force
}
