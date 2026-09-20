param(
    [ValidateRange(300,36000)][int]$Frames = 3600,
    [ValidateRange(1,144)][int]$Cases = 72
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$reportDirectory = Join-Path $projectRoot ('Verification/rollback-run-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
$compilerSetup = 'C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat'
$cmakeExe = 'C:\Program Files\CMake\bin\cmake.exe'
foreach ($dependency in @($compilerSetup, $cmakeExe)) {
    if (!(Test-Path -LiteralPath $dependency)) { throw "Required build tool missing: $dependency" }
}
New-Item -ItemType Directory -Path $reportDirectory | Out-Null
Push-Location $projectRoot
try {
    $buildCommand = 'call "{0}" >nul && "{1}" -S Native -B Native/build-unity && "{1}" --build Native/build-unity --target rollback_physics_tests original_driving_session_tests' -f $compilerSetup, $cmakeExe
    & cmd.exe /d /c $buildCommand *> (Join-Path $reportDirectory 'build.log')
    if ($LASTEXITCODE -ne 0) { throw "Rollback test build failed. See $reportDirectory/build.log" }
    & Native/bin/original_driving_session_tests.exe Native *> (Join-Path $reportDirectory 'driving-regression.log')
    if ($LASTEXITCODE -ne 0) { throw "Driving regression failed. See $reportDirectory/driving-regression.log" }
    & Native/bin/rollback_physics_tests.exe Native (Join-Path $reportDirectory 'matrix.csv') $Frames $Cases |
        Tee-Object -FilePath (Join-Path $reportDirectory 'matrix.log')
    if ($LASTEXITCODE -ne 0) { throw "Rollback verification failed. See $reportDirectory/matrix.log" }
    Write-Output "Rollback physics experiment passed. Reports: $reportDirectory"
    Write-Output 'This is a simulated-network native test; Unity/Steam online rollback remains disabled.'
} finally {
    Pop-Location
}
