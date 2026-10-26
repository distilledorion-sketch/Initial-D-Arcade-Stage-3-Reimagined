#requires -Version 5.1
[CmdletBinding()]
param([string]$Destination)
$ErrorActionPreference='Stop'

function Get-GameFileHash([string]$Path) {
    # Use the .NET runtime already required by this installer. Some Windows
    # PowerShell installations cannot load the Get-FileHash module.
    $algorithm=[Security.Cryptography.SHA256]::Create()
    $inputStream=$null
    try {
        $inputStream=[IO.File]::OpenRead([IO.Path]::GetFullPath($Path))
        return [BitConverter]::ToString($algorithm.ComputeHash($inputStream)).Replace('-','').ToLowerInvariant()
    } finally {
        if($inputStream){$inputStream.Dispose()}
        $algorithm.Dispose()
    }
}

function Get-ReleaseArchiveNames([string]$Version) {
    if($Version -cnotmatch '^(\d+\.\d+\.\d+)-community-replays\.(\d+)$'){throw 'Unsupported release version.'}
    $shortVersion=$Matches[1]+'.'+$Matches[2]
    return @("Initial-D-Arcade-Stage-3-Reimagined-$shortVersion-Windows-x64.zip","Initial-D-Additional-Courses-$Version.zip")
}

function Assert-PlainPath([string]$Path) {
    for($part=[IO.Path]::GetFullPath($Path);$part;$part=[IO.Path]::GetDirectoryName($part)) {
        try {$attributes=[IO.File]::GetAttributes($part)}
        catch [IO.FileNotFoundException] {continue}
        catch [IO.DirectoryNotFoundException] {continue}
        if($attributes -band [IO.FileAttributes]::ReparsePoint) {throw 'Installation paths must not contain links.'}
    }
}
function Get-GameEntryPath([string]$Root,[string]$Name) {
    if(!$Name -or $Name.Length -gt 220){throw 'Invalid archive path length.'}
    $name=$Name.Replace('\','/'); $parts=$name.Split('/')
    foreach($part in $parts) {
        if(!$part -or $part -in @('.','..','rom','userdata','userdata-unity-scene','community-times','replays','Custom Music','custom-music','identity.json','game-options.json','admin-access.txt','deploy.private.json','library.json','pending.json') -or $part -match '[<>:"|?*\x00-\x1f]' -or $part -match '[. ]$' -or $part -match '^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(\.|$)') {throw 'Unsafe archive path.'}
    }
    if($parts.Count -gt 1) {if($parts[0] -notin @('InitialDUnity_Data','MonoBleedingEdge','D3D12')){throw 'Unexpected archive folder.'}}
    elseif($name -notin @('InitialDUnity.exe','UnityPlayer.dll','UnityCrashHandler64.exe','dstorage.dll','dstoragecore.dll','steam_appid.txt','READ ME.txt','Replay Viewer.cmd','MULTIPLAYER TEST.txt')){throw 'Unexpected game file.'}
    $target=[IO.Path]::GetFullPath((Join-Path $Root $name))
    if(!$target.StartsWith([IO.Path]::GetFullPath($Root).TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Archive path escaped the installation.'}
    Assert-PlainPath $target
    return $target
}
function Remove-SetupStaging([string]$Stage,[string]$Parent) {
    $stageRoot=[IO.Path]::GetFullPath($Stage)
    $parentRoot=[IO.Path]::GetFullPath($Parent)
    if(![IO.Path]::GetDirectoryName($stageRoot).Equals($parentRoot,[StringComparison]::OrdinalIgnoreCase) -or
        [IO.Path]::GetFileName($stageRoot) -cnotmatch '^\.initial-d-extract-[0-9a-f]{32}$') {throw 'Unexpected setup cleanup path.'}
    Assert-PlainPath $stageRoot
    if(!(Test-Path -LiteralPath $stageRoot)){return}
    # Validate every descendant before deleting anything. Delete exact files
    # and then empty directories; never follow a link or recursively remove
    # an unvalidated tree. This folder belongs to this invocation only.
    $pending=[Collections.Generic.Stack[string]]::new();$pending.Push($stageRoot)
    $folders=[Collections.Generic.List[string]]::new();$files=[Collections.Generic.List[string]]::new()
    while($pending.Count) {
        $directory=$pending.Pop();Assert-PlainPath $directory;$folders.Add($directory)
        foreach($entry in [IO.Directory]::GetFileSystemEntries($directory)) {
            $entry=[IO.Path]::GetFullPath($entry)
            if(!$entry.StartsWith($stageRoot+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Setup cleanup escaped its temporary folder.'}
            Assert-PlainPath $entry
            if([IO.File]::GetAttributes($entry) -band [IO.FileAttributes]::Directory){$pending.Push($entry)}else{$files.Add($entry)}
        }
    }
    foreach($file in $files){Assert-PlainPath $file;[IO.File]::Delete($file)}
    for($index=$folders.Count-1;$index -ge 0;$index--){Assert-PlainPath $folders[$index];[IO.Directory]::Delete($folders[$index],$false)}
}
function Expand-GameArchives([string[]]$Archives,[string]$Target) {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $Target=[IO.Path]::GetFullPath($Target)
    $volumeRoot=[IO.Path]::GetPathRoot($Target)
    if($Target.TrimEnd([char[]]'\/').Equals($volumeRoot.TrimEnd([char[]]'\/'),[StringComparison]::OrdinalIgnoreCase)){throw 'Choose a game folder, not the root of a drive.'}
    # GetDirectoryName treats a trailing separator as a child boundary. Strip
    # it so the temporary folder always remains a sibling of the destination.
    $Target=$Target.TrimEnd([char[]]'\/')
    Assert-PlainPath $Target
    if((Test-Path -LiteralPath $Target) -and @(Get-ChildItem -LiteralPath $Target -Force).Count){throw 'Choose an empty folder. Use the in-game updater for an existing installation.'}
    $targetExisted=[IO.Directory]::Exists($Target)
    $parent=[IO.Path]::GetDirectoryName($Target)
    if(!$parent){throw 'Choose a game folder, not the root of a drive.'}
    $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $opened=[Collections.Generic.List[object]]::new(); $entries=[Collections.Generic.List[object]]::new(); $expanded=[long]0
    $stage=$null;$removedEmptyTarget=$false;$originalAccess=$null;$originalAttributes=$null
    try {
        # Validate every archive before creating game files.
        foreach($archive in $Archives) {
            $zip=[IO.Compression.ZipFile]::OpenRead($archive);$opened.Add($zip)
            foreach($entry in $zip.Entries) {
                $path=Get-GameEntryPath $Target $entry.FullName.TrimEnd('/')
                if(($entry.ExternalAttributes -band 0x400) -or (($entry.ExternalAttributes -shr 16) -band 0xf000) -eq 0xa000){throw 'Archive links are not allowed.'}
                if(!$entry.Name){continue}
                if(!$seen.Add($entry.FullName.Replace('\','/'))){throw 'Duplicate game file across downloads.'}
                $expanded+=$entry.Length;if($expanded -gt 24GB){throw 'Archive exceeds game size limit.'}
                $entries.Add(@{entry=$entry;name=$entry.FullName.Replace('\','/')})
            }
        }
        foreach($name in @('InitialDUnity.exe','UnityPlayer.dll','InitialDUnity_Data/globalgamemanagers','InitialDUnity_Data/Managed/Assembly-CSharp.dll','InitialDUnity_Data/StreamingAssets/GUNSAI/menu.idastex','InitialDUnity_Data/StreamingAssets/ODAWARA/menu.idastex')) {if(!$seen.Contains($name)){throw "Game package is incomplete: $name"}}
        if(([IO.DriveInfo]::new([IO.Path]::GetPathRoot([IO.Path]::GetFullPath($Target)))).AvailableFreeSpace -lt $expanded+128MB){throw 'Not enough disk space to install the game.'}
        # The final folder must stay absent/empty until both archives extract.
        # A sibling keeps the final directory rename on the same volume.
        $candidate=Join-Path $parent ('.initial-d-extract-'+[Guid]::NewGuid().ToString('N'))
        Assert-PlainPath $candidate
        if(Test-Path -LiteralPath $candidate){throw 'Temporary installation folder already exists.'}
        [IO.Directory]::CreateDirectory($candidate)|Out-Null;$stage=$candidate
        if([IO.Directory]::Exists($Target) -ne $targetExisted){throw 'The installation folder changed while extracting. Nothing was replaced.'}
        if($targetExisted) {
            $originalAccess=[IO.Directory]::GetAccessControl($Target)
            $originalAttributes=[IO.File]::GetAttributes($Target)
            [IO.Directory]::SetAccessControl($stage,$originalAccess)
        }
        foreach($item in $entries) {
            $path=Get-GameEntryPath $stage $item.name
            [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($path))|Out-Null
            $entryInput=$null;$entryOutput=$null
            try {
                $entryInput=$item.entry.Open();$entryOutput=[IO.File]::Open($path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write)
                $entryInput.CopyTo($entryOutput)
            } finally {if($entryOutput){$entryOutput.Dispose()};if($entryInput){$entryInput.Dispose()}}
            if(([IO.FileInfo]::new($path)).Length -ne $item.entry.Length){throw 'Extracted file size mismatch.'}
        }
        Assert-PlainPath $Target;Assert-PlainPath $stage
        if([IO.Directory]::Exists($Target) -ne $targetExisted){throw 'The installation folder changed while extracting. Nothing was replaced.'}
        if($targetExisted) {
            # Delete only an empty destination. Concurrently created data makes
            # this fail safely, instead of replacing somebody else's files.
            if(@(Get-ChildItem -LiteralPath $Target -Force).Count){throw 'The installation folder changed while extracting. Nothing was replaced.'}
            if($null -ne $originalAttributes){[IO.File]::SetAttributes($stage,$originalAttributes)}
            [IO.Directory]::Delete($Target,$false);$removedEmptyTarget=$true
        }
        [IO.Directory]::Move($stage,$Target);$stage=$null;$removedEmptyTarget=$false
    } finally {
        foreach($zip in $opened){$zip.Dispose()}
        if($removedEmptyTarget -and !(Test-Path -LiteralPath $Target)) {
            Assert-PlainPath $Target;[IO.Directory]::CreateDirectory($Target)|Out-Null
            if($originalAccess){[IO.Directory]::SetAccessControl($Target,$originalAccess)}
            if($null -ne $originalAttributes){[IO.File]::SetAttributes($Target,$originalAttributes)}
        }
        if($stage){try{Remove-SetupStaging $stage $parent}catch{Write-Warning ('Could not clean temporary setup files: '+$stage)}}
    }
}

# Dot-sourcing exposes only the validation/extraction functions for private tests.
if($MyInvocation.InvocationName -eq '.'){return}
# Windows PowerShell 5.1 can leave PSScriptRoot empty while binding parameter
# defaults under -File. Resolve the default here, after script metadata is ready.
$installerRoot=$PSScriptRoot
if([string]::IsNullOrWhiteSpace($installerRoot)) {
    $scriptPath=$MyInvocation.MyCommand.Path
    if([string]::IsNullOrWhiteSpace($scriptPath)){throw 'Cannot locate the installer. Extract the setup ZIP and run Install Initial D.cmd.'}
    $installerRoot=[IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($scriptPath))
}
if([string]::IsNullOrWhiteSpace($Destination)){$Destination=Join-Path $installerRoot 'Initial D Arcade Stage 3'}
$Destination=[IO.Path]::GetFullPath($Destination)
Assert-PlainPath $Destination
if((Test-Path -LiteralPath $Destination) -and @(Get-ChildItem -LiteralPath $Destination -Force).Count){throw 'The installation folder must be empty. Existing players should use the game updater.'}
$version='0.3.95-community-replays.53';$tag='v'+$version
$repo='distilledorion-sketch/Initial-D-Arcade-Stage-3-Reimagined'
$web=[Net.WebClient]::new();$web.Headers['User-Agent']='Initial-D-Setup';[Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12
$cache=Join-Path $installerRoot ('.initial-d-download-'+[Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($cache)|Out-Null
try {
    Write-Host 'Checking the official release...'
    $release=$web.DownloadString("https://api.github.com/repos/$repo/releases/tags/$tag")|ConvertFrom-Json
    if($release.draft -or $release.prerelease -or $release.tag_name -ne $tag){throw 'The requested release is not available.'}
    $names=Get-ReleaseArchiveNames $version
    $archives=@()
    foreach($name in $names) {
        $assetMatches=@($release.assets|Where-Object{$_.name -ceq $name})
        if($assetMatches.Count -ne 1){throw "Required download is missing: $name"}
        $asset=$assetMatches[0];$expected="https://github.com/$repo/releases/download/$tag/$name"
        if($asset.state -ne 'uploaded' -or $asset.size -le 0 -or $asset.size -ge 2GB -or $asset.browser_download_url -cne $expected -or $asset.digest -cnotmatch '^sha256:[0-9a-f]{64}$'){throw 'Release download verification metadata is invalid.'}
        if(([IO.DriveInfo]::new([IO.Path]::GetPathRoot($cache))).AvailableFreeSpace -lt $asset.size+128MB){throw 'Not enough disk space for the download.'}
        $path=Join-Path $cache $name;Write-Host ("Downloading {0} ({1:N0} MB)..." -f $name,($asset.size/1MB))
        $web.DownloadFile($expected,$path)
        if((Get-Item -LiteralPath $path).Length -ne $asset.size -or ('sha256:'+(Get-GameFileHash $path)) -cne $asset.digest){throw 'Download checksum mismatch. Nothing was installed.'}
        $archives+=$path
    }
    Write-Host 'Installing game and course files...'
    Expand-GameArchives $archives $Destination
    [IO.Directory]::CreateDirectory((Join-Path $Destination 'rom'))|Out-Null
    [IO.File]::WriteAllText((Join-Path $Destination 'rom/README.txt'),'Place your legally obtained gds-0033.chd here. The original ROM is required and is not included.')
    [IO.Directory]::CreateDirectory((Join-Path $Destination 'Custom Music'))|Out-Null
    # Delete only this invocation's verified download files, never the game folder.
    foreach($path in $archives){if([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($path)) -ne $cache){throw 'Unexpected cleanup path'};Remove-Item -LiteralPath $path}
    Remove-Item -LiteralPath $cache
    Write-Host "Installed successfully: $Destination"
    Write-Host 'Place your original ROM in the rom folder, then run InitialDUnity.exe.'
} finally {$web.Dispose()}
