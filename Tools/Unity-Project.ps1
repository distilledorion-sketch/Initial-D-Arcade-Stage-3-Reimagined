param([ValidateSet('Open','Build')][string]$Action = 'Open', [string]$UnityPath)
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $UnityPath) { $UnityPath = $env:IDAS3_UNITY_EDITOR }
if (-not $UnityPath) {
    $editorRoots = @('C:\Program Files\Unity\Hub\Editor', 'D:\Program Files\Unity\Hub\Editor')
    $candidates = foreach ($editorRoot in $editorRoots) {
        if (Test-Path -LiteralPath $editorRoot) {
            Get-ChildItem -LiteralPath $editorRoot -Directory | ForEach-Object {
                $candidate = Join-Path $_.FullName 'Editor\Unity.exe'
                if (Test-Path -LiteralPath $candidate) { Get-Item -LiteralPath $candidate }
            }
        }
    }
    $UnityPath = ($candidates | Sort-Object FullName -Descending | Select-Object -First 1).FullName
}
if (-not $UnityPath -or -not (Test-Path -LiteralPath $UnityPath)) {
    Write-Host 'Unity Editor was not found. This project was verified with Unity 6000.6.0f1.'
    Write-Host 'For a custom installation: set IDAS3_UNITY_EDITOR to the full path of Editor\Unity.exe.'
    Write-Host "Project: $projectRoot"
    exit 2
}
$logsRoot = Join-Path $projectRoot 'Logs'
New-Item -ItemType Directory -Path $logsRoot -Force | Out-Null
$editorArgs = @('-projectPath', ('"' + $projectRoot + '"'), '-force-d3d11', '-logFile', ('"' + (Join-Path $logsRoot ($Action + '.log')) + '"'))
if ($Action -eq 'Build') {
    $editorArgs += @('-batchmode', '-quit', '-buildTarget', 'Win64', '-executeMethod', 'Idas3Build.BuildWindows')
    $process = Start-Process -FilePath $UnityPath -ArgumentList $editorArgs -WindowStyle Hidden -PassThru
    # Wait for the editor, not its long-lived licensing/package services.
    $process.WaitForExit()
    exit $process.ExitCode
}
# Open is explicitly the interactive editor launcher the user selected.
$editorArgs += @('-executeMethod', 'Idas3Build.Configure')
Start-Process -FilePath $UnityPath -ArgumentList $editorArgs
