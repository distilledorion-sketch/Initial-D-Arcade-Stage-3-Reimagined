#requires -Version 5.1
[CmdletBinding()]
param([string]$Destination=(Join-Path $PSScriptRoot 'Initial D Arcade Stage 3'))
$ErrorActionPreference='Stop'

function Assert-PlainPath([string]$Path) {
    for($part=[IO.Path]::GetFullPath($Path);$part;$part=[IO.Path]::GetDirectoryName($part)) {
        if((Test-Path -LiteralPath $part) -and ((Get-Item -LiteralPath $part -Force).Attributes -band [IO.FileAttributes]::ReparsePoint)) {throw 'Installation paths must not contain links.'}
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
function Expand-GameArchives([string[]]$Archives,[string]$Target) {
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    Assert-PlainPath $Target
    if((Test-Path -LiteralPath $Target) -and @(Get-ChildItem -LiteralPath $Target -Force).Count){throw 'Choose an empty folder. Use the in-game updater for an existing installation.'}
    $seen=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    $opened=[Collections.Generic.List[object]]::new(); $entries=[Collections.Generic.List[object]]::new(); $expanded=[long]0
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
                $entries.Add(@{entry=$entry;path=$path})
            }
        }
        foreach($name in @('InitialDUnity.exe','UnityPlayer.dll','InitialDUnity_Data/globalgamemanagers','InitialDUnity_Data/Managed/Assembly-CSharp.dll','InitialDUnity_Data/StreamingAssets/GUNSAI/menu.idastex','InitialDUnity_Data/StreamingAssets/ODAWARA/menu.idastex')) {if(!$seen.Contains($name)){throw "Game package is incomplete: $name"}}
        if(([IO.DriveInfo]::new([IO.Path]::GetPathRoot([IO.Path]::GetFullPath($Target)))).AvailableFreeSpace -lt $expanded+128MB){throw 'Not enough disk space to install the game.'}
        foreach($item in $entries) {
            [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($item.path))|Out-Null
            $entryInput=$item.entry.Open();$entryOutput=[IO.File]::Open($item.path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write)
            try{$entryInput.CopyTo($entryOutput)}finally{$entryOutput.Dispose();$entryInput.Dispose()}
            if((Get-Item -LiteralPath $item.path).Length -ne $item.entry.Length){throw 'Extracted file size mismatch.'}
        }
    } finally {foreach($zip in $opened){$zip.Dispose()}}
}

# Dot-sourcing exposes only the validation/extraction functions for private tests.
if($MyInvocation.InvocationName -eq '.'){return}
$Destination=[IO.Path]::GetFullPath($Destination)
Assert-PlainPath $Destination
if((Test-Path -LiteralPath $Destination) -and @(Get-ChildItem -LiteralPath $Destination -Force).Count){throw 'The installation folder must be empty. Existing players should use the game updater.'}
$version='0.3.95-community-replays.44';$tag='v'+$version
$repo='distilledorion-sketch/Initial-D-Arcade-Stage-3-Reimagined'
$web=[Net.WebClient]::new();$web.Headers['User-Agent']='Initial-D-Setup';[Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12
$cache=Join-Path $PSScriptRoot ('.initial-d-download-'+[Guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($cache)|Out-Null
try {
    Write-Host 'Checking the official release...'
    $release=$web.DownloadString("https://api.github.com/repos/$repo/releases/tags/$tag")|ConvertFrom-Json
    if($release.draft -or $release.prerelease -or $release.tag_name -ne $tag){throw 'The requested release is not available.'}
    $names=@('Initial-D-Arcade-Stage-3-Reimagined-0.3.95.44-Windows-x64.zip',"Initial-D-Additional-Courses-$version.zip")
    $archives=@()
    foreach($name in $names) {
        $assetMatches=@($release.assets|Where-Object{$_.name -ceq $name})
        if($assetMatches.Count -ne 1){throw "Required download is missing: $name"}
        $asset=$assetMatches[0];$expected="https://github.com/$repo/releases/download/$tag/$name"
        if($asset.state -ne 'uploaded' -or $asset.size -le 0 -or $asset.size -ge 2GB -or $asset.browser_download_url -cne $expected -or $asset.digest -cnotmatch '^sha256:[0-9a-f]{64}$'){throw 'Release download verification metadata is invalid.'}
        if(([IO.DriveInfo]::new([IO.Path]::GetPathRoot($cache))).AvailableFreeSpace -lt $asset.size+128MB){throw 'Not enough disk space for the download.'}
        $path=Join-Path $cache $name;Write-Host ("Downloading {0} ({1:N0} MB)..." -f $name,($asset.size/1MB))
        $web.DownloadFile($expected,$path)
        if((Get-Item -LiteralPath $path).Length -ne $asset.size -or ('sha256:'+(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()) -cne $asset.digest){throw 'Download checksum mismatch. Nothing was installed.'}
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
