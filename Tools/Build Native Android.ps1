[CmdletBinding()]
param(
    [ValidateSet('Debug','Release')]
    [string]$Configuration = 'Release',
    [string]$NdkVersion = '',
    [string]$OutputDirectory = '',
    [int]$Jobs = 4
)

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$sdkRoot = if ($env:ANDROID_SDK_ROOT) { $env:ANDROID_SDK_ROOT } elseif ($env:ANDROID_HOME) { $env:ANDROID_HOME } else { Join-Path $env:LOCALAPPDATA 'Android\Sdk' }
$sdkRoot = (Resolve-Path $sdkRoot).Path
$ndkRoot = Join-Path $sdkRoot 'ndk'
if ($NdkVersion) {
    $ndk = Join-Path $ndkRoot $NdkVersion
} else {
    $ndk = Get-ChildItem -LiteralPath $ndkRoot -Directory | Sort-Object Name -Descending |
        Where-Object { Test-Path (Join-Path $_.FullName 'build\cmake\android.toolchain.cmake') } |
        Select-Object -First 1 -ExpandProperty FullName
}
if (-not $ndk -or -not (Test-Path (Join-Path $ndk 'build\cmake\android.toolchain.cmake'))) {
    throw "Android NDK not found under $ndkRoot. Install an NDK and pass -NdkVersion explicitly when multiple versions are present."
}

$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
$cmakePath = if ($cmakeCommand) { $cmakeCommand.Source } else { $null }
if (-not $cmakePath) {
    $cmakeCandidates = Get-ChildItem -LiteralPath (Join-Path $sdkRoot 'cmake') -Directory -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending |
        ForEach-Object { Join-Path $_.FullName 'bin\cmake.exe' } |
        Where-Object { Test-Path $_ }
    if ($cmakeCandidates) { $cmakePath = $cmakeCandidates | Select-Object -First 1 }
}
if (-not $cmakePath) {
    throw "CMake was not found. Install CMake 3.24+ or the Android SDK CMake component, then rerun this script."
}

$ninja = Get-Command ninja -ErrorAction SilentlyContinue
if (-not $ninja) { throw 'Ninja was not found on PATH. Install Ninja or use a CMake generator available on this machine.' }

$buildRoot = Join-Path $projectRoot 'Native\build-android-arm64'
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $projectRoot 'Assets\Plugins\Android\arm64-v8a' }
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
$toolchain = Join-Path $ndk 'build\cmake\android.toolchain.cmake'
$cmakeArgs = @(
    '-S', (Join-Path $projectRoot 'Native'),
    '-B', $buildRoot,
    '-G', 'Ninja',
    "-DCMAKE_TOOLCHAIN_FILE=$toolchain",
    '-DANDROID_ABI=arm64-v8a',
    '-DANDROID_PLATFORM=android-26',
    "-DCMAKE_BUILD_TYPE=$Configuration",
    '-DANDROID_STL=c++_static',
    '-DANDROID_SUPPORT_FLEXIBLE_PAGE_SIZES=ON',
    "-DIDAS3_ANDROID_PLUGIN_OUTPUT_DIRECTORY=$OutputDirectory"
)
& $cmakePath @cmakeArgs
if ($LASTEXITCODE -ne 0) { throw "Android CMake configure failed with exit code $LASTEXITCODE." }

& $cmakePath '--build' $buildRoot '--target' 'Idas3Unity' '--parallel' $Jobs
if ($LASTEXITCODE -ne 0) { throw "Android Native build failed with exit code $LASTEXITCODE." }

$plugin = Join-Path $OutputDirectory 'libIdas3Unity.so'
if (-not (Test-Path $plugin)) { throw "Native build completed but Unity plugin was not produced: $plugin" }
Write-Host "Built Android ARM64 Unity plugin: $plugin"
