param([string]$UnityPath = 'C:\Program Files\Unity\Hub\Editor\6000.6.0f1\Editor\Unity.exe', [switch]$TestBuild, [switch]$AuthorityBuild, [switch]$InputFixBuild)
$ErrorActionPreference = 'Stop'
$project = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (!(Test-Path -LiteralPath $UnityPath)) { throw "Unity editor missing: $UnityPath" }
$log = Join-Path $project 'Logs/BuildUnityScene.log'
$method = if ($InputFixBuild) { 'Idas3Build.BuildInputFixPlayer' } elseif ($AuthorityBuild) { 'Idas3Build.BuildAuthorityPlayer' } elseif ($TestBuild) { 'Idas3Build.BuildTestPlayer' } else { 'Idas3Build.BuildUnityScene' }
$arguments = '-batchmode -quit -force-d3d11 -projectPath "' + $project + '" -buildTarget Win64 -executeMethod ' + $method + ' -logFile "' + $log + '"'
$editor = Start-Process -FilePath $UnityPath -ArgumentList $arguments -WindowStyle Hidden -PassThru
$editor.WaitForExit()
if ($editor.ExitCode -ne 0) { Get-Content -LiteralPath $log -Tail 55; throw "Unity scene build failed ($($editor.ExitCode))." }
Get-Content -LiteralPath $log -Tail 12
exit 0
