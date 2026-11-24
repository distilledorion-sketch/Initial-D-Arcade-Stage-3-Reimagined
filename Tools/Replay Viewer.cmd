@echo off
setlocal
set "IDAS3_REPLAY_SCRIPT=%~f0"
set "IDAS3_REPLAY_ROOT=%~dp0"
set "IDAS3_REPLAY_FILE=%~f1"
powershell.exe -NoProfile -Command "$source=[IO.File]::ReadAllText($env:IDAS3_REPLAY_SCRIPT); & ([scriptblock]::Create($source.Substring($source.LastIndexOf('# PowerShell')+12)))"
if not "%errorlevel%"=="3" exit /b %errorlevel%
ver >nul
set "player=%~dp0Initial D Arcade Stage 3 Reimagined.exe"
if not exist "%player%" set "player=%~dp0InitialDUnity.exe"
if "%~1"=="" (
  start "Initial D" "%player%" -idas3-replay-viewer
) else (
  start "Initial D" "%player%" -idas3-replay-viewer "%~f1"
)
exit /b

# PowerShell
$ErrorActionPreference = 'Stop'
$root = $env:IDAS3_REPLAY_ROOT
$engine = Join-Path $root 'InitialDUnity.bin'
$game = Get-Process -Name InitialDUnity,InitialDUnity.bin,'Initial D Arcade Stage 3 Reimagined' -ErrorAction SilentlyContinue |
    Where-Object { $_.Path -in @($engine, (Join-Path $root 'InitialDUnity.exe'), (Join-Path $root 'Initial D Arcade Stage 3 Reimagined.exe')) } |
    Sort-Object @{Expression={$_.Path -eq $engine};Descending=$true} |
    Select-Object -First 1
if (!$game) { exit 3 }
if ($game.Path -ne $engine -and (Test-Path -LiteralPath $engine)) {
    Write-Host 'Game setup is still running. Open Replays once the game starts.'
    exit 4
}
$directory = [IO.Directory]::CreateDirectory((Join-Path $root 'UserData\Temp'))
$destination = Join-Path $directory.FullName ('replay-' + $game.Id + '.request')
$temporary = $destination + '.' + [guid]::NewGuid().ToString('N')
try {
    [IO.File]::WriteAllText($temporary, [string]$env:IDAS3_REPLAY_FILE)
    if ([IO.File]::Exists($destination)) {
        [IO.File]::Replace($temporary, $destination, [System.Management.Automation.Language.NullString]::Value)
    } else {
        [IO.File]::Move($temporary, $destination)
    }
} finally {
    if ([IO.File]::Exists($temporary)) { [IO.File]::Delete($temporary) }
}
exit 0
