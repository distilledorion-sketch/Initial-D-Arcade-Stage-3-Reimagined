param([switch]$Verify, [switch]$InstallOnly, [switch]$RestoreSaves, [string]$RecoverySource, [switch]$KeepCurrentSaves)
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$python = Join-Path $PSScriptRoot 'python\python.exe'
$setup = Join-Path $PSScriptRoot 'disc_setup.py'
$portable = Join-Path $PSScriptRoot 'disc_portable.py'
$cache = Join-Path $root 'InitialDUnity_Data\StreamingAssets\IDAS3'
$userData = Join-Path $root 'UserData'
$disc = Join-Path $root 'rom\gds-0033.chd'
$pic = Join-Path $root 'rom\317-0384-com.pic'
$region = 'gds-0033'
$japanDisc = Join-Path $root 'rom\gds-0032c.chd'
$japanPic = Join-Path $root 'rom\317-0379-jpn.pic'

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
function Import-DiscSecurity([switch]$IfAvailable) {
    $locations = @('--source-disc', $original)
    if ($previous -and [IO.Path]::IsPathRooted($previous) -and $previous -ne $original) {
        $locations += @('--source-disc', $previous)
    }
    $found = Invoke-SetupTool $portable (@('find-security', '--region', $region) + $locations) 'Could not check the existing arcade security files.' @(0,3) -ReturnCode
    if ($found -eq 0) { return }
    if ($IfAvailable) { return }
    $picName = Split-Path -Leaf $pic
    $security = Choose-Original "Choose your existing arcade ZIP or $picName" 'Arcade security files (*.pic;*.zip)|*.pic;*.zip' (Split-Path -LiteralPath $original)
    if (!$security) { throw "Setup needs the matching $picName from your arcade disc set." }
    Invoke-SetupTool $portable @('import', '--region', $region, '--security', $security) "The selected file must contain the matching $picName for $region."
}

function Try-OpenLock([string]$path) {
    try {
        return [IO.File]::Open($path, 'OpenOrCreate', 'ReadWrite', 'None')
    } catch [IO.IOException] {
        # Only sharing and lock violations mean another process is using the file.
        if (($_.Exception.GetBaseException().HResult -band 0xffff) -notin 32,33) { throw }
        return $null
    }
}

function Invoke-SetupTool([string]$tool, [string[]]$arguments, [string]$failure,
                          [int[]]$allowed = @(0), [switch]$ReadValue, [switch]$ReturnCode) {
    # Keep stderr even when Windows PowerShell represents it as ErrorRecords.
    $ErrorActionPreference = 'Continue'
    $lines = New-Object 'Collections.Generic.List[string]'
    $values = New-Object 'Collections.Generic.List[string]'
    $lastErrorLine = ''
    $global:LASTEXITCODE = $null
    $encoding = [Console]::OutputEncoding
    $previousPythonEncoding = $env:PYTHONIOENCODING
    try {
        [Console]::OutputEncoding = New-Object Text.UTF8Encoding($false)
        $env:PYTHONIOENCODING = 'utf-8:backslashreplace'
        & $python -B -u -X utf8 $tool @arguments 2>&1 | ForEach-Object {
            $line = $_.ToString()
            $lines.Add($line)
            if ($_ -is [Management.Automation.ErrorRecord]) {
                if ($line.Trim()) { $lastErrorLine = $line.Trim() }
            } elseif ($ReadValue) { $values.Add($line) }
            if (!$ReadValue) { [Console]::WriteLine($line) }
        }
        $code = $LASTEXITCODE
    } finally {
        $env:PYTHONIOENCODING = $previousPythonEncoding
        [Console]::OutputEncoding = $encoding
    }
    if ($null -eq $code -or $code -notin $allowed) {
        $command = ((Split-Path -Leaf $tool), $arguments[0] -join ' ')
        $status = if ($null -eq $code) { 'failed to start' } else { "exit code $code" }
        $reason = if ($lastErrorLine) { $lastErrorLine } elseif ($lines.Count) { $lines[$lines.Count - 1] }
        $errorDetail = New-Object InvalidOperationException("$failure ($command, $status)")
        $errorDetail.Data['Output'] = $lines -join [Environment]::NewLine
        $errorDetail.Data['Reason'] = $reason
        throw $errorDetail
    }
    if ($ReadValue) { return $values.ToArray() }
    if ($ReturnCode) { return $code }
}

function Get-RequestedRegion([string]$installed) {
    $path = Join-Path $userData 'userdata-unity-scene\game-options.json'
    if (![IO.File]::Exists($path)) { return $installed }
    try { $saved = [IO.File]::ReadAllText($path) | ConvertFrom-Json } catch { return $installed }
    if ($saved.version -ne 1) { return $installed }
    if ($null -ne $saved.romLanguage) {
        if ($saved.romLanguage -eq 0) { return 'gds-0033' }
        if ($saved.romLanguage -eq 1) { return 'gds-0032c' }
    }
    if ($saved.arcadeTextLanguage -eq 1) { return 'gds-0032c' }
    return $installed
}

function Select-InstalledRegion([string]$installed) {
    $requested = Get-RequestedRegion $installed
    if ($requested -eq $installed) { return $installed }
    $label = if ($requested -eq 'gds-0032c') { 'Japanese Rev C' } else { 'English' }
    $picName = if ($requested -eq 'gds-0032c') { '317-0379-jpn.pic' } else { '317-0384-com.pic' }
    $requestedDisc = Join-Path $root "rom\$requested.chd"
    $requestedPic = Join-Path $root "rom\$picName"
    $selected = $requestedDisc
    $canAsk = !$silentTest -and !$InstallOnly
    if (![IO.File]::Exists($requestedDisc)) {
        if (!$canAsk) { return $installed }
        $selected = Choose-Original "ROM Language: choose your $label disc ($requested.chd), or cancel to keep playing" 'Arcade Stage 3 disc image (*.chd)|*.chd' ''
        if (!$selected) { return $installed }
        Invoke-SetupTool $portable @('import', '--region', $requested, '--chd', $selected, '--keep-source') "Could not import the $label disc."
    }
    if (![IO.File]::Exists((Join-Path $regionCache "$requested\disc-receipt.json"))) {
        $found = Invoke-SetupTool $portable @('find-security', '--region', $requested, '--source-disc', $selected, '--source-disc', $requestedDisc) 'Could not check the disc security file.' @(0,3) -ReturnCode
        if ($found -eq 3) {
            if (!$canAsk) { return $installed }
            $security = Choose-Original "ROM Language: choose $picName or the matching arcade ZIP" 'Arcade security files (*.pic;*.zip)|*.pic;*.zip' (Split-Path -LiteralPath $selected)
            if (!$security) { return $installed }
            Invoke-SetupTool $portable @('import', '--region', $requested, '--security', $security) "Could not import $picName."
        }
    }
    $lease = Try-OpenLock $regionLock
    if (!$lease) { throw 'Close the running game before switching ROM Language.' }
    try {
        $gamePath = Join-Path $root 'InitialDUnity.bin'
        if (Get-Process -Name 'InitialDUnity','InitialDUnity.bin' -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $gamePath }) {
            throw 'Close the running game before switching ROM Language.'
        }
        Invoke-SetupTool $setup @('--switch-region', $requested, '--chd', $requestedDisc, '--security', $requestedPic, '--output', $cache) "Could not switch to the $label ROM."
    } finally { $lease.Dispose() }
    return $requested
}

try {
    $env:IDAS3_LEGACY_TEMP = [IO.Path]::GetTempPath()
    $localTemp = [IO.Directory]::CreateDirectory((Join-Path $userData 'Temp')).FullName
    $env:TEMP = $localTemp
    $env:TMP = $localTemp
    if ($Verify) {
        Invoke-SetupTool $setup @('--verify', '--output', $cache) 'Could not verify the installation.'
        exit 0
    }
    $forwardedArgs = @()
    if ($env:IDAS3_DISC_ARGS) {
        $forwardedArgs = ConvertFrom-Json ([Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($env:IDAS3_DISC_ARGS)))
    }
    $silentTest = $forwardedArgs -contains '-idas3-test-silent' -or $forwardedArgs -contains '-batchmode'
    $fromDesktop = $env:IDAS3_LAUNCHER_PID -and !$silentTest -and !$InstallOnly -and !$RestoreSaves
    $japanese = (Get-UICulture).TwoLetterISOLanguageName -eq 'ja'
    if (!$InstallOnly -and !$RestoreSaves -and !$RecoverySource -and !$KeepCurrentSaves -and !$forwardedArgs) {
        $launchLock = Try-OpenLock (Join-Path $userData 'launch.lock')
        if (!$launchLock) {
            $message = if ($japanese) { 'ゲームの起動準備中です。Alt＋Tab でセットアップ画面が開いていないか確認してください。準備が終わるとゲームが起動します。' }
                       else { 'The game is already starting. Check for a setup window with Alt+Tab. The game will open when preparation finishes.' }
            [Console]::WriteLine($message)
            if ($fromDesktop) {
                Add-Type -AssemblyName System.Windows.Forms
                [void][Windows.Forms.MessageBox]::Show($message, 'Initial D Arcade Stage 3 Reimagined', 'OK', 'Information')
            }
            exit 0
        }
    }
    $lockPath = Join-Path $userData 'setup.lock'
    $prepareLock = Try-OpenLock $lockPath
    if (!$prepareLock) {
        $message = if ($japanese) { '別の処理がゲームファイルを準備しています。Alt＋Tab でセットアップ画面を確認してください。準備が終わるまで待機します。' }
                   else { 'Waiting for another process to finish preparing the game files. Check for a setup window with Alt+Tab.' }
        [Console]::WriteLine($message)
        if ($fromDesktop) {
            Add-Type -AssemblyName System.Windows.Forms
            $waitWindow = New-Object Windows.Forms.Form
            $waitWindow.Text = 'Initial D Arcade Stage 3 Reimagined'
            $waitWindow.ClientSize = New-Object Drawing.Size(430, 90)
            $label = New-Object Windows.Forms.Label
            $label.Dock = 'Fill'
            $label.Padding = New-Object Windows.Forms.Padding(12)
            $label.Text = $message
            $waitWindow.Controls.Add($label)
            $waitWindow.Show()
        }
        $waiting = [Diagnostics.Stopwatch]::StartNew()
        while (!$prepareLock) {
            if ($waitWindow) {
                [Windows.Forms.Application]::DoEvents()
                if ($waitWindow.IsDisposed) { exit 0 }
            }
            if ($waiting.Elapsed.TotalMinutes -ge 10) {
                if ($japanese) { throw 'ゲームファイルがまだ使用中です。セットアップの完了後に、ゲームをもう一度起動してください。' }
                throw 'The game files are still in use. Let the open setup finish, then try again.'
            }
            Start-Sleep -Milliseconds 250
            $prepareLock = Try-OpenLock $lockPath
        }
        if ($waitWindow) { $waitWindow.Dispose() }
    }
    . (Join-Path $PSScriptRoot 'Recover-PlayerData.ps1')
    if ($RestoreSaves) {
        Invoke-PlayerRecovery -Manual -Source $RecoverySource
        exit 0
    }
    $runtimeMessage = if ($japanese) { '起動に必要なゲームファイルを準備できませんでした。' } else { 'Setup could not prepare the game files.' }
    Invoke-SetupTool $portable @('runtime') $runtimeMessage
    $regionCache = Join-Path $root 'InitialDUnity_Data\RegionCache'
    $regionLock = Join-Path $userData 'rom-region.lock'
    if ([IO.File]::Exists((Join-Path $regionCache 'switch-from.txt'))) {
        Invoke-SetupTool $setup @('--recover-region', '--output', $cache) 'Could not recover the previous ROM selection.'
    }
    $source = Join-Path $cache 'disc-source.txt'
    $ready = Test-Path -LiteralPath (Join-Path $cache 'disc-receipt.json') -PathType Leaf
    $previous = ''
    if (Test-Path -LiteralPath $source -PathType Leaf) { $previous = [IO.File]::ReadAllText($source).Trim() }
    $baseRegion = ''
    if ($ready) {
        $baseRegion = Invoke-SetupTool $setup @('--base-region', '--output', $cache) 'Could not identify the installed disc data.' -ReadValue
        $selectedRegion = Select-InstalledRegion $baseRegion
        if ($selectedRegion -ne $baseRegion) {
            $baseRegion = $selectedRegion
            $previous = [IO.File]::ReadAllText($source).Trim()
        }
    }
    if (!$ready -and [IO.File]::Exists($japanDisc)) {
        $baseRegion = if ([IO.File]::Exists($disc)) { Get-RequestedRegion 'gds-0033' } else { 'gds-0032c' }
    }
    if ($baseRegion -eq 'gds-0032c') {
        $region = 'gds-0032c'
        $disc = $japanDisc
        $pic = $japanPic
    }
    $original = $disc
    if (!(Test-Path -LiteralPath $disc -PathType Leaf)) {
        if ($previous -and [IO.Path]::IsPathRooted($previous) -and (Test-Path -LiteralPath $previous -PathType Leaf)) {
            $original = $previous
        } else {
            $original = Choose-Original 'Select your English or Japanese Rev C Arcade Stage 3 disc' 'Arcade Stage 3 disc image (*.chd)|*.chd' ''
            if (!$original) { throw 'Setup needs the original Arcade Stage 3 disc. Launch again to continue.' }
        }
        $region = Invoke-SetupTool $portable @('disc-region', '--chd', $original) 'Could not identify the original disc.' -ReadValue
        if ($ready -and $baseRegion -and $region -ne $baseRegion) {
            throw "This installation is using $baseRegion. Select its disc to resume, or change ROM Language in Options."
        }
        if ($region -eq 'gds-0032c') {
            $disc = $japanDisc
            $pic = $japanPic
        } else {
            $disc = Join-Path $root 'rom\gds-0033.chd'
            $pic = Join-Path $root 'rom\317-0384-com.pic'
        }
        Invoke-SetupTool $portable @('import', '--region', $region, '--chd', $original) 'Could not import the original disc.'
    } elseif ($ready -and $previous -ne "rom/$region.chd") {
        Invoke-SetupTool $portable @('import', '--region', $region, '--chd', $disc) 'Could not verify the disc in the game folder.'
    }
    if ($ready -and $previous -and [IO.Path]::IsPathRooted($previous) -and !(Test-Path -LiteralPath $pic -PathType Leaf)) {
        Import-DiscSecurity -IfAvailable
    }
    if (!$ready) {
        Import-DiscSecurity
        Invoke-SetupTool $setup @('--chd', $disc, '--security', $pic, '--output', $cache) 'Disc extraction did not complete.'
    }
    $activation = Invoke-SetupTool $setup @('--activate', '--chd', $disc, '--output', $cache) 'Could not prepare the installed game files.' @(0,3,4) -ReturnCode
    if ($activation -eq 3 -or $activation -eq 4) {
        Import-DiscSecurity
        if ($activation -eq 4) {
            Invoke-SetupTool $setup @('--chd', $disc, '--security', $pic, '--output', $cache) 'Could not upgrade the previous disc extraction.'
            $baseRegion = $region
        }
        Invoke-SetupTool $setup @('--activate', '--chd', $disc, '--security', $pic, '--output', $cache) 'Could not prepare the installed game files.'
    }
    if (!(Test-Path -LiteralPath (Join-Path $userData 'migration-v1.json'))) {
        $other = Get-Process -Name InitialDUnity,'Initial D Arcade Stage 3 Reimagined' -ErrorAction SilentlyContinue | Where-Object { $_.Path -and (Split-Path -Parent $_.Path) -ne $root -and !(Test-Path -LiteralPath (Join-Path (Split-Path -Parent $_.Path) 'UserData\migration-v1.json')) }
        if ($other) { throw 'Close the other running copy of Initial D before moving the previous saves.' }
        Invoke-SetupTool $portable @('migrate') 'Previous files could not all be moved. Close other copies of the game and retry in a writable folder.'
    }
    if (!$silentTest -or $RecoverySource -or $KeepCurrentSaves) {
        Invoke-PlayerRecovery -Source $RecoverySource -Keep:$KeepCurrentSaves
    }
    if (!(Test-Path -LiteralPath (Join-Path $userData 'disc-upgrade-v1.json')) -or (Test-Path -LiteralPath (Join-Path $userData '.disc-cache-upgrade'))) {
        Invoke-SetupTool $portable @('finish') 'Could not finish migrating the previous installation. Close other copies and try again.'
    }
    if (!$InstallOnly) {
        $playing = [IO.File]::Open($regionLock, 'OpenOrCreate', 'ReadWrite', 'ReadWrite')
    }
    $prepareLock.Dispose()
    if ($launchLock) { $launchLock.Dispose() }
    if ($InstallOnly) { exit 0 }
    & $python -B $portable play
    exit $LASTEXITCODE
} catch {
    $failure = $_
    $message = $failure.Exception.Message
    if ($failure.Exception.Data['Reason']) {
        $message += [Environment]::NewLine + $failure.Exception.Data['Reason']
    }
    try {
        $logs = [IO.Directory]::CreateDirectory((Join-Path $userData 'Logs')).FullName
        $report = Join-Path $logs ("Setup-{0:yyyyMMdd-HHmmss-fff}-{1}.log" -f [DateTime]::UtcNow, $PID)
        $versionFile = Join-Path $PSScriptRoot 'package-version.txt'
        $version = if ([IO.File]::Exists($versionFile)) { [IO.File]::ReadAllText($versionFile).Trim() }
        $details = @([DateTime]::UtcNow.ToString('o'), "Game folder: $root", "Version: $version", $failure.Exception.ToString(),
                     $failure.ScriptStackTrace, $failure.Exception.Data['Output'])
        [IO.File]::WriteAllLines($report, [string[]]$details)
        $message += [Environment]::NewLine + $report
        [IO.File]::WriteAllText((Join-Path $logs 'Launcher-error.txt'), $message)
    } catch { [Console]::Error.WriteLine("Could not save the setup failure report: " + $_.Exception.Message) }
    [Console]::Error.WriteLine($message)
    exit 1
} finally {
    if ($playing) { $playing.Dispose() }
    if ($waitWindow) { $waitWindow.Dispose() }
    if ($launchLock) { $launchLock.Dispose() }
    if ($prepareLock) { $prepareLock.Dispose() }
}
