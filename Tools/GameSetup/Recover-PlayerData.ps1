function Invoke-RecoveryTool([string[]]$Arguments) {
    $output = & $python -B (Join-Path $PSScriptRoot 'save_recovery.py') @Arguments
    if ($LASTEXITCODE -ne 0) { throw 'Save recovery could not finish. Close both games and retry. The source files are preserved.' }
    return $output
}

function Assert-GameClosed([string]$DataPath) {
    $folders = @($root)
    if ($DataPath) {
        $folder = [IO.Path]::GetFullPath($DataPath)
        if ((Split-Path -Leaf $folder) -ieq 'UserData') { $folder = Split-Path -Parent $folder }
        $folders += $folder
    }
    foreach ($proc in @(Get-Process -Name 'InitialDUnity','InitialDUnity.bin','Initial D Arcade Stage 3 Reimagined' -ErrorAction SilentlyContinue)) {
        if ($proc.Id -eq $env:IDAS3_LAUNCHER_PID -or !$proc.Path) { continue }
        $processFolder = Split-Path -Parent $proc.Path
        if ($processFolder -in $folders -or !(Test-Path -LiteralPath (Join-Path $processFolder 'UserData/migration-v1.json'))) {
            throw 'Close the running game before restoring saves and settings.'
        }
    }
}

function Select-PreviousFolder {
    Add-Type -AssemblyName System.Windows.Forms
    $browser = New-Object System.Windows.Forms.FolderBrowserDialog
    $browser.Description = 'Select your previous Initial D game folder, or its UserData folder.'
    $browser.ShowNewFolderButton = $false
    try {
        if ($browser.ShowDialog() -eq 'OK') { return $browser.SelectedPath }
    } finally { $browser.Dispose() }
}

function Invoke-PlayerRecovery([switch]$Manual, [string]$Source, [switch]$Keep) {
    $marker = Join-Path $userData 'save-recovery-v2.json'
    $pending = Join-Path $userData '.pending-save-recovery'
    if (!$Manual -and !$Source -and (Test-Path -LiteralPath $marker) -and !(Test-Path -LiteralPath $pending)) { return }
    if (Test-Path -LiteralPath $pending) { Assert-GameClosed }
    $state = (Invoke-RecoveryTool @('check')) | ConvertFrom-Json
    if ($Manual -and !$Source) {
        $Source = Select-PreviousFolder
        if (!$Source) { return }
    }
    if (!$Source -and !$Keep) {
        Add-Type -AssemblyName System.Windows.Forms
        $title = 'Initial D save recovery'
        foreach ($candidate in @($state.sources)) {
            if (!$candidate.missing) { continue }
            $message = "This previous installation has cards, settings or HUD selections missing here:`n`n" + $candidate.path +
                "`n`nRestore this profile? It has " + @($candidate.cards).Count + " saved card(s). Matching slots and settings will use the selected copy. Your current copies will be backed up."
            $choice = [Windows.Forms.MessageBox]::Show($message,$title,'YesNoCancel','Question','Button2')
            if ($choice -eq 'Yes') { $Source = $candidate.path; break }
            if ($choice -eq 'Cancel') { return }
        }
        if (!$Source -and @($state.current.cards).Count -eq 0 -and
            !(@($state.current.settings) -match '/(game-options|controls)\.json$') -and @($state.sources).Count -eq 0) {
            $choice = [Windows.Forms.MessageBox]::Show('Do you have a previous Initial D game folder to restore cards, settings and HUD customizations from? Choose No for a new profile.','Initial D save recovery','YesNo','Question','Button2')
            if ($choice -eq 'Yes') {
                $Source = Select-PreviousFolder
                if (!$Source) { return }
            }
        }
    }
    if ($Source) {
        Assert-GameClosed $Source
        if ($Manual -and !$RecoverySource) {
            $choice = [Windows.Forms.MessageBox]::Show("Restore from $Source into $root? Matching slots and settings will use the selected copy. Your current copies will be backed up.",'Restore saves and settings','OKCancel','Question','Button2')
            if ($choice -ne 'OK') { return }
        }
        Invoke-RecoveryTool @('copy','--source',$Source) | Out-Host
        $state = (Invoke-RecoveryTool @('check')) | ConvertFrom-Json
    }
    if (!$Keep -and $state.appearance -and @($state.appearance.PSObject.Properties).Count -gt 0) {
        Add-Type -AssemblyName System.Windows.Forms
        $choice = [Windows.Forms.MessageBox]::Show('A previous settings backup has a tachometer or ornament that is now set to default. Restore those selections and keep your other settings?','Restore race HUD','YesNo','Question','Button1')
        if ($choice -eq 'Yes') { Assert-GameClosed; Invoke-RecoveryTool @('appearance') | Out-Host }
    }
    Invoke-RecoveryTool @('done') | Out-Null
}
