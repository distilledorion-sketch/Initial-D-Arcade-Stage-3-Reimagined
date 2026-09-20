param([string]$UnityEditor = 'C:\Program Files\Unity\Hub\Editor\6000.6.0f1\Editor')
$ErrorActionPreference = 'Stop'
$project = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$native = Join-Path $project 'Native'
$testRoot = Join-Path $project 'Verification\unity-audio'
New-Item -ItemType Directory -Path $testRoot -Force | Out-Null
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'MSVC x64 is required.' }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
$sourceFiles = @('tests\unity_audio_output_tests.cpp', 'src\unity_audio_output.cpp', 'src\audio.cpp', 'src\original_audio.cpp', 'src\original_menu_audio.cpp')
$arguments = @('/nologo','/std:c++20','/EHsc','/O2','/MT','/fp:strict','/DNOMINMAX','/DWIN32_LEAN_AND_MEAN',('/I"' + (Join-Path $native 'src') + '"'))
$arguments += $sourceFiles | ForEach-Object { '"' + (Join-Path $native $_) + '"' }
$arguments += @('/Fe:unity_audio_output_tests.exe', '/link', ('"' + (Join-Path $native 'build-unity\idas3_original.lib') + '"'), 'winmm.lib')
$rsp = Join-Path $testRoot 'native.rsp'
[IO.File]::WriteAllLines($rsp, $arguments)
$cmdFile = Join-Path $testRoot 'build-test.cmd'
$cmdLines = @('@echo off', ('call "' + $vcvars + '" x64 >nul'), 'if errorlevel 1 exit /b %errorlevel%', ('cd /d "' + $testRoot + '"'), 'cl @native.rsp', 'if errorlevel 1 exit /b %errorlevel%', ('unity_audio_output_tests.exe "' + $native + '"'))
[IO.File]::WriteAllLines($cmdFile, $cmdLines)
& $env:COMSPEC /d /c ('"' + $cmdFile + '"') 2>&1 | Tee-Object -FilePath (Join-Path $testRoot 'native.log')
if ($LASTEXITCODE -ne 0) { throw 'Native Unity audio test failed.' }

# Compile the actual component against installed Unity assemblies, without
# starting the Editor or running a Unity project build.
$data = Join-Path $UnityEditor 'Data'
$dotnet = Join-Path $data 'DotNetSdk\dotnet.exe'
$compiler = Get-ChildItem -LiteralPath (Join-Path $data 'DotNetSdk\sdk') -Directory | Sort-Object Name -Descending | Select-Object -First 1
$csc = Join-Path $compiler.FullName 'Roslyn\bincore\csc.dll'
$reference = Join-Path $data 'DotNetSdk\packs\NETStandard.Library.Ref\2.1.0\ref\netstandard2.1'
$managedArguments = @('/nologo','/noconfig','/nostdlib+','/target:library','/langversion:9.0',('/out:"' + (Join-Path $testRoot 'Idas3UnityAudio.syntax.dll') + '"'))
$managedArguments += Get-ChildItem -LiteralPath $reference -Filter '*.dll' | ForEach-Object { '/r:"' + $_.FullName + '"' }
$managedArguments += @('UnityEngine.CoreModule.dll','UnityEngine.AudioModule.dll') | ForEach-Object { '/r:"' + (Join-Path $data ('Managed\UnityEngine\' + $_)) + '"' }
$managedArguments += '"' + (Join-Path $project 'Assets\Scripts\Idas3UnityAudio.cs') + '"'
$managedRsp = Join-Path $testRoot 'managed.rsp'
[IO.File]::WriteAllLines($managedRsp, $managedArguments)
& $dotnet $csc ('@' + $managedRsp) 2>&1 | Tee-Object -FilePath (Join-Path $testRoot 'managed.log')
if ($LASTEXITCODE -ne 0) { throw 'Unity audio component C# API compile failed.' }
Write-Output 'Unity audio native tests and installed-Unity C# API compile passed. Unity audio device playback is a separate integration check.'
