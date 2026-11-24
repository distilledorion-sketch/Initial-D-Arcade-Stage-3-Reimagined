param([switch]$Verify, [switch]$InstallOnly)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$python = Join-Path $PSScriptRoot 'python\python.exe'
$setup = Join-Path $PSScriptRoot 'disc_setup.py'
$portable = Join-Path $PSScriptRoot 'disc_portable.py'
$cache = Join-Path $root 'InitialDUnity_Data\StreamingAssets\IDAS3'
$userData = Join-Path $root 'UserData'
$disc = Join-Path $root 'rom\gds-0033.chd'
$pic = Join-Path $root 'rom\317-0384-com.pic'

function Choose-Original([string]$title, [string]$filter, [string]$directory) {
    Add-Type -AssemblyName System.Windows.Forms
    $dialog = New-Object System.Windows.Forms.OpenFileDialog
    try {
        $dialog.Title = $title
        $dialog.Filter = $filter
        $dialog.CheckFileExists = $true
        if ($directory) { $dialog.InitialDirectory = $directory }
        if ($dialog.ShowDialog() -eq [Windows.Forms.DialogResult]::OK) { return $dialog.FileName }
    } finally { $dialog.Dispose() }
}
function Choose-DiscSecurity([string]$original) {
    $directory = Split-Path -LiteralPath $original
    foreach ($name in @('317-0384-com.pic','initdv3e.zip')) {
        $candidate = Join-Path $directory $name
        if (Test-Path -LiteralPath $candidate -PathType Leaf) { return $candidate }
    }
    return (Choose-Original 'Select the matching 317-0384-COM security PIC or initdv3e.zip' 'Arcade security files (*.pic;*.zip)|*.pic;*.zip' $directory)
}
try {
    $env:IDAS3_LEGACY_TEMP = [IO.Path]::GetTempPath()
    $localTemp = [IO.Directory]::CreateDirectory((Join-Path $userData 'Temp')).FullName
    $env:TEMP = $localTemp
    $env:TMP = $localTemp
    if ($Verify) {
        & $python -B $setup --output $cache --verify
        exit $LASTEXITCODE
    }
    $prepareLock = [IO.File]::Open((Join-Path $userData 'setup.lock'), 'OpenOrCreate', 'ReadWrite', 'None')
    & $python -B $portable runtime
    if ($LASTEXITCODE -ne 0) { throw 'Could not install the game runtime beside the launcher.' }
    $source = Join-Path $cache 'disc-source.txt'
    $ready = Test-Path -LiteralPath (Join-Path $cache 'disc-receipt.json') -PathType Leaf
    $previous = ''
    if (Test-Path -LiteralPath $source -PathType Leaf) { $previous = [IO.File]::ReadAllText($source).Trim() }
    $original = $disc
    if (!(Test-Path -LiteralPath $disc -PathType Leaf)) {
        if ($previous -and [IO.Path]::IsPathRooted($previous) -and (Test-Path -LiteralPath $previous -PathType Leaf)) {
            $original = $previous
        } else {
            $original = Choose-Original 'Select your original GDS-0033 disc for the game folder' 'Arcade Stage 3 disc image (*.chd)|*.chd' ''
            if (!$original) { throw 'Setup needs the original GDS-0033 disc. Launch again to continue.' }
        }
        & $python -B $portable import --chd $original
        if ($LASTEXITCODE -ne 0) { throw 'Could not import the original disc.' }
    } elseif ($ready -and $previous -ne 'rom/gds-0033.chd') {
        & $python -B $portable import --chd $disc
        if ($LASTEXITCODE -ne 0) { throw 'Could not verify the disc in the game folder.' }
    }
    if ($ready -and !(Test-Path -LiteralPath $pic -PathType Leaf)) {
        # Older installations recorded the disc path but did not retain the PIC.
        $directory = Split-Path -LiteralPath $original
        foreach ($name in @('317-0384-com.pic','initdv3e.zip')) {
            $candidate = Join-Path $directory $name
            if (Test-Path -LiteralPath $candidate -PathType Leaf) {
                & $python -B $portable import --security $candidate
                if ($LASTEXITCODE -ne 0) { throw 'Could not verify the security file beside the original disc.' }
                break
            }
        }
    }
    if (!$ready) {
        if (!(Test-Path -LiteralPath $pic -PathType Leaf)) {
            $security = Choose-DiscSecurity $original
            if (!$security) { throw 'Setup needs the matching security PIC or initdv3e.zip. Launch again to continue.' }
            & $python -B $portable import --security $security
            if ($LASTEXITCODE -ne 0) { throw 'Could not import the security file.' }
        }
        & $python -B $setup --chd $disc --security $pic --output $cache
        if ($LASTEXITCODE -ne 0) { throw 'Disc extraction did not complete. See the installation log beside the game cache.' }
    }
    & $python -B $setup --activate --chd $disc --output $cache
    $activation = $LASTEXITCODE
    if ($activation -eq 3 -or $activation -eq 4) {
        if (!(Test-Path -LiteralPath $pic -PathType Leaf)) {
            $security = Choose-DiscSecurity $original
            if (!$security) { throw 'Setup needs the matching security PIC or initdv3e.zip. Launch again to continue.' }
            & $python -B $portable import --security $security
            if ($LASTEXITCODE -ne 0) { throw 'Could not import the security file.' }
        }
        if ($activation -eq 4) {
            & $python -B $setup --chd $disc --security $pic --output $cache
            if ($LASTEXITCODE -ne 0) { throw 'Could not upgrade the previous disc extraction. See the installation log.' }
        }
        & $python -B $setup --activate --chd $disc --security $pic --output $cache
    }
    if ($LASTEXITCODE -ne 0) { throw 'Could not prepare the installed game files.' }
    if (!(Test-Path -LiteralPath (Join-Path $userData 'migration-v1.json'))) {
        $other = Get-Process -Name InitialDUnity,'Initial D Arcade Stage 3 Reimagined' -ErrorAction SilentlyContinue | Where-Object { $_.Path -and (Split-Path -Parent $_.Path) -ne $root -and !(Test-Path -LiteralPath (Join-Path (Split-Path -Parent $_.Path) 'UserData\migration-v1.json')) }
        if ($other) { throw 'Close the other running copy of Initial D before moving the previous saves.' }
        & $python -B $portable migrate
        if ($LASTEXITCODE -ne 0) { throw 'Previous files could not all be moved. Close other copies of the game and retry in a writable folder.' }
    }
    if (!(Test-Path -LiteralPath (Join-Path $userData 'disc-upgrade-v1.json')) -or (Test-Path -LiteralPath (Join-Path $userData '.disc-cache-upgrade'))) {
        & $python -B $portable finish
        if ($LASTEXITCODE -ne 0) { throw 'Could not finish migrating the previous installation. Close other copies and try again.' }
    }
    $prepareLock.Dispose()
    if ($InstallOnly) { exit 0 }
    & $python -B $portable play
    exit $LASTEXITCODE
} catch {
    [Console]::Error.WriteLine($_.Exception.Message)
    try {
        $logs = [IO.Directory]::CreateDirectory((Join-Path $userData 'Logs')).FullName
        [IO.File]::WriteAllText((Join-Path $logs 'Launcher-error.txt'), $_.Exception.Message)
    } catch {}
    exit 1
} finally {
    if ($prepareLock) { $prepareLock.Dispose() }
}
