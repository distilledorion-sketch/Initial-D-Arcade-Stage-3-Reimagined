#requires -Version 5.1
[CmdletBinding()]
param([string]$Session,[string]$Destination,[string]$CacheRoot,[switch]$ListOnly)
$ErrorActionPreference='Stop'

function Assert-RecoveryPath([string]$Path) {
    for($part=[IO.Path]::GetFullPath($Path);$part;$part=[IO.Path]::GetDirectoryName($part)) {
        try {$attributes=[IO.File]::GetAttributes($part)}
        catch [IO.FileNotFoundException] {continue}
        catch [IO.DirectoryNotFoundException] {continue}
        if($attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Recovery paths must not contain links.'}
    }
}
function Get-InitialDRecoverySessions([string]$Root) {
    $Root=[IO.Path]::GetFullPath($Root).TrimEnd('\')
    if([IO.Path]::GetFileName($Root) -cne 'InitialDUpdates'){throw 'Choose the InitialDUpdates cache folder.'}
    Assert-RecoveryPath $Root
    if(![IO.Directory]::Exists($Root)){return}
    foreach($folder in [IO.Directory]::GetDirectories($Root)) {
        if([IO.Path]::GetFileName($folder) -cnotmatch '^[0-9a-fA-F]{32}$'){continue}
        try {
            Assert-RecoveryPath $folder
            $ready=Join-Path $folder 'ready';$plan=Join-Path $folder 'install.plan';$status=Join-Path $folder 'result.json'
            foreach($path in @($ready,$plan,$status)){Assert-RecoveryPath $path}
            if(![IO.File]::Exists($ready) -or ([IO.FileInfo]::new($ready)).Length -gt 32 -or [IO.File]::ReadAllText($ready) -cne 'Prepared'){continue}
            if(![IO.File]::Exists($plan) -or ([IO.FileInfo]::new($plan)).Length -gt 64MB){continue}
            if([IO.File]::Exists($status)) {
                if(([IO.FileInfo]::new($status)).Length -gt 32768){continue}
                $result=[IO.File]::ReadAllText($status)|ConvertFrom-Json
                if($result.schema -ne 1 -or $result.passed -or $result.cleanupSafe -or !$result.rollbackNeeded){continue}
            }
            $reader=[IO.BinaryReader]::new([IO.File]::OpenRead($plan),[Text.Encoding]::Unicode)
            try {
                if([Text.Encoding]::ASCII.GetString($reader.ReadBytes(8)) -cne 'IDUPD002'){continue}
                $length=$reader.ReadUInt32()
                if($length -le 0 -or $length -ge 32768){continue}
                $bytes=$reader.ReadBytes($length*2)
                if($bytes.Length -ne $length*2){continue}
                $game=[Text.Encoding]::Unicode.GetString($bytes)
                if($game.Contains([string][char]0)){continue}
                $game=[IO.Path]::GetFullPath($game);Assert-RecoveryPath $game
                [pscustomobject]@{Session=$folder;GameFolder=$game;PreparedUtc=[IO.File]::GetLastWriteTimeUtc($ready).ToString('u')}
            } finally {$reader.Dispose()}
        } catch {
            [Console]::Error.WriteLine('Could not inspect recovery session '+[IO.Path]::GetFileName($folder)+'. Its files were kept.')
        }
    }
}

# Discovery is read-only. The native helper performs full validation, locking,
# backup verification and restoration; this script never copies/deletes saves.
if($MyInvocation.InvocationName -eq '.'){return}
[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false)
$toolRoot=$PSScriptRoot
if([string]::IsNullOrWhiteSpace($toolRoot)){$toolRoot=[IO.Path]::GetDirectoryName($MyInvocation.MyCommand.Path)}
if([string]::IsNullOrWhiteSpace($CacheRoot)){$CacheRoot=Join-Path ([IO.Path]::GetTempPath()) 'InitialDUpdates'}
$choices=@(Get-InitialDRecoverySessions $CacheRoot|Sort-Object GameFolder,Session)
if(![string]::IsNullOrWhiteSpace($Destination)) {
    $Destination=[IO.Path]::GetFullPath($Destination).TrimEnd('\')
    $choices=@($choices|Where-Object{$_.GameFolder.TrimEnd('\').Equals($Destination,[StringComparison]::OrdinalIgnoreCase)})
}
if(![string]::IsNullOrWhiteSpace($Session)) {
    $Session=[IO.Path]::GetFullPath($Session).TrimEnd('\')
    $choices=@($choices|Where-Object{$_.Session.Equals($Session,[StringComparison]::OrdinalIgnoreCase)})
    if($choices.Count -ne 1){throw 'That session is not an unfinished recoverable update. No game files were changed.'}
}
if($ListOnly){ConvertTo-Json -InputObject $choices -Depth 3;exit 0}
if(!$choices.Count){Write-Host 'No unfinished updates were found for this Windows account. No game files were changed.';exit 0}
$index=0
if($choices.Count -gt 1) {
    for($number=0;$number -lt $choices.Count;$number++){Write-Host ('{0}. {1}  ({2})' -f ($number+1),$choices[$number].GameFolder,$choices[$number].PreparedUtc)}
    $answer=Read-Host 'Choose the installation to recover (number, or Enter to cancel)'
    $selected=0
    if([string]::IsNullOrWhiteSpace($answer)){exit 0}
    if(![int]::TryParse($answer,[ref]$selected) -or $selected -lt 1 -or $selected -gt $choices.Count){throw 'Invalid selection. No game files were changed.'}
    $index=$selected-1
}
$choice=$choices[$index];$helper=Join-Path $toolRoot 'Idas3UpdateInstaller.exe'
Assert-RecoveryPath $helper
if(![IO.File]::Exists($helper)){throw 'Recovery helper missing. Extract the complete setup ZIP and run Recover Initial D.cmd.'}
Write-Host ('Recovering the previous installation: '+$choice.GameFolder)
Write-Host 'Checking saved backups and restoring game files. Keep this window open until verification finishes.'
$start=[Diagnostics.ProcessStartInfo]::new()
$start.FileName=$helper;$start.Arguments='"'+(Join-Path $choice.Session 'install.plan')+'" --recover'
$start.UseShellExecute=$false;$start.CreateNoWindow=$true;$start.RedirectStandardError=$true
$process=[Diagnostics.Process]::Start($start)
try {
    $errors=$process.StandardError.ReadToEndAsync();$process.WaitForExit();$detail=$errors.GetAwaiter().GetResult()
    if($process.ExitCode -ne 0) {
        if(![string]::IsNullOrWhiteSpace($detail)){Write-Host $detail.Trim()}
        throw 'Recovery did not complete. The recovery backups were kept. Resolve the error above, then try again.'
    }
} finally {$process.Dispose()}
Write-Host 'Previous game files restored and verified. Your saves, ROM, replays and music were preserved.'
Write-Host 'Launch InitialDUnity.exe normally, then retry the update.'
