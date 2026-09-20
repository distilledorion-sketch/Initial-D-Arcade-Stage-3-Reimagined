#requires -Version 5.1
<#
.SYNOPSIS
Preserve every original native runtime data file in a Unity deployment.
.DESCRIPTION
Run after the Unity player build to avoid importing 14,000+ native files into
the Unity editor. No file deletion, save migration or live-save writes occur.
The default destination is outside Assets. The source snapshot is read-only.
.EXAMPLE
.\Tools\Stage-GameData.ps1 -InventoryOnly
.EXAMPLE
.\Tools\Stage-GameData.ps1 -DestinationRoot .\Builds\Windows\InitialDUnity_Data\StreamingAssets\IDAS3
.EXAMPLE
.\Tools\Stage-GameData.ps1 -DestinationRoot .\Assets\StreamingAssets\IDAS3
This explicit destination is supported, but increases editor import/storage.
#>
[CmdletBinding()]
param(
    [string]$SourceRoot,
    [string]$DestinationRoot,
    [string]$ManifestPath,
    [switch]$InventoryOnly,
    [switch]$CreateFreshSaveSeed
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $SourceRoot) { $SourceRoot = Join-Path $projectRoot 'Native' }
if (-not $DestinationRoot) { $DestinationRoot = Join-Path $projectRoot 'RuntimeData/IDAS3' }
if (-not $ManifestPath) { $ManifestPath = Join-Path $projectRoot 'Staging/game-data-manifest.json' }
$SourceRoot = [IO.Path]::GetFullPath($SourceRoot).TrimEnd('\', '/')
$DestinationRoot = [IO.Path]::GetFullPath($DestinationRoot).TrimEnd('\', '/')
$ManifestPath = [IO.Path]::GetFullPath($ManifestPath)
$sourceData = Join-Path $SourceRoot 'data'
$targetData = Join-Path $DestinationRoot 'data'
if (-not (Test-Path -LiteralPath $sourceData -PathType Container)) { throw "Source data missing: $sourceData" }
if ($DestinationRoot.Equals($SourceRoot, [StringComparison]::OrdinalIgnoreCase) -or
    $DestinationRoot.StartsWith($SourceRoot + '\', [StringComparison]::OrdinalIgnoreCase) -or
    $SourceRoot.StartsWith($DestinationRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Source and deployment roots must be separate; staging must not modify the native snapshot.'
}
if ($ManifestPath.StartsWith($SourceRoot + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'The inventory manifest must be outside the native snapshot.'
}
if ($ManifestPath.StartsWith($targetData + '\', [StringComparison]::OrdinalIgnoreCase)) {
    throw 'The manifest must be outside the deployed data tree.'
}
function Assert-NoReparseAncestor([string]$Path) {
    $candidate = $Path
    while ($candidate) {
        if (Test-Path -LiteralPath $candidate) {
            $item = Get-Item -LiteralPath $candidate -Force
            if ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Reparse path is not a plain deployment folder: $candidate" }
        }
        $parent = [IO.Path]::GetDirectoryName($candidate)
        if ($parent -eq $candidate) { break }
        $candidate = $parent
    }
}
function Hash-File([string]$Path) { return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
function Relative-DataPath([string]$Path) { return $Path.Substring($sourceData.Length + 1).Replace('\', '/') }
function Write-Json([string]$Path, $Value) {
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($Path)) | Out-Null
    $json = $Value | ConvertTo-Json -Depth 10
    [IO.File]::WriteAllText($Path, $json + "`n", (New-Object Text.UTF8Encoding($false)))
}
Assert-NoReparseAncestor $sourceData
Assert-NoReparseAncestor $DestinationRoot
Assert-NoReparseAncestor $ManifestPath
$sourceEntries = @(Get-ChildItem -LiteralPath $sourceData -Force -Recurse)
foreach ($entry in $sourceEntries) {
    if ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Source data contains a link: $($entry.FullName)" }
}
$sourceFiles = @($sourceEntries | Where-Object { -not $_.PSIsContainer } | Sort-Object FullName)
if ($sourceFiles.Count -eq 0) { throw 'Source data is empty.' }

# These are the unchanged driving/control/session sources, not the Unity host.
# The full original_physics data tree is already included in the file inventory.
$handlingStems = @('physics','original_controls','original_dynamics','original_transmission',
    'original_vehicle','original_initialization','original_data','original_math','original_contact',
    'original_collision','original_contact_completion','original_host_input','original_frame_state',
    'original_matrix','original_road_contact','original_session_initialization','original_driving_session',
    'original_start_grid','original_race_start','original_race_rules','original_race_path','original_record_rules')
$handlingBefore = @()
foreach ($stem in $handlingStems) {
    foreach ($extension in @('cpp','h')) {
        $relative = "src/$stem.$extension"
        $path = Join-Path $SourceRoot $relative
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Required handling source missing: $relative" }
        $handlingBefore += [ordered]@{ path=$relative; sha256=(Hash-File $path) }
    }
}
$saveSource = Join-Path $SourceRoot 'userdata'
$saveBefore = @()
if (Test-Path -LiteralPath $saveSource) {
    foreach ($file in @(Get-ChildItem -LiteralPath $saveSource -File -Recurse -Force | Sort-Object FullName)) {
        $saveBefore += [ordered]@{ path=$file.FullName.Substring($SourceRoot.Length + 1).Replace('\','/'); sha256=(Hash-File $file.FullName) }
    }
}
Write-Host "Inventorying $($sourceFiles.Count) native files; no transcoding or asset filtering."
$records = [Collections.Generic.List[object]]::new()
$totalBytes = [long]0
$index = 0
foreach ($file in $sourceFiles) {
    $records.Add([ordered]@{ path=(Relative-DataPath $file.FullName); bytes=$file.Length; sha256=(Hash-File $file.FullName) })
    $totalBytes += $file.Length
    $index++
    if (($index % 1000) -eq 0) { Write-Progress -Activity 'Hashing native runtime data' -Status "$index / $($sourceFiles.Count)" -PercentComplete (100 * $index / $sourceFiles.Count) }
}
Write-Progress -Activity 'Hashing native runtime data' -Completed

if (-not $InventoryOnly) {
    if (Test-Path -LiteralPath $targetData) {
        foreach ($entry in @(Get-ChildItem -LiteralPath $targetData -Force -Recurse)) {
            if ($entry.Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Existing destination contains a link: $($entry.FullName)" }
        }
    }
    [IO.Directory]::CreateDirectory($targetData) | Out-Null
    # No /MIR or /PURGE: never remove user files or Unity-generated metadata.
    & robocopy.exe $sourceData $targetData /E /COPY:DAT /DCOPY:DAT /R:1 /W:1 /XJ /NFL /NDL /NP /NJH /NJS | Out-Host
    if ($LASTEXITCODE -ge 8) { throw "Runtime data staging failed (robocopy exit $LASTEXITCODE)." }
    $index = 0
    foreach ($record in $records) {
        $destination = Join-Path $targetData $record.path
        if (-not (Test-Path -LiteralPath $destination -PathType Leaf)) { throw "Staged file missing: $($record.path)" }
        $item = Get-Item -LiteralPath $destination
        if ($item.Length -ne $record.bytes -or (Hash-File $destination) -ne $record.sha256) { throw "Staged file differs: $($record.path)" }
        $index++
        if (($index % 1000) -eq 0) { Write-Progress -Activity 'Verifying deployed data' -Status "$index / $($records.Count)" -PercentComplete (100 * $index / $records.Count) }
    }
    Write-Progress -Activity 'Verifying deployed data' -Completed
    $expected = New-Object 'Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
    foreach ($record in $records) { [void]$expected.Add($record.path) }
    foreach ($item in @(Get-ChildItem -LiteralPath $targetData -File -Recurse -Force)) {
        $relative = $item.FullName.Substring($targetData.Length + 1).Replace('\','/')
        if (-not $expected.Contains($relative) -and $item.Extension -ne '.meta') {
            throw "Unexpected prior deployment file: $relative. Choose a fresh destination; the staging script never deletes files."
        }
    }
}

foreach ($record in @($handlingBefore) + @($saveBefore)) {
    if ((Hash-File (Join-Path $SourceRoot $record.path)) -ne $record.sha256) { throw "Source changed during staging: $($record.path)" }
}
$seedPath = $null
if ($CreateFreshSaveSeed) {
    # New defaults only, never copied profiles/progression or existing saves.
    $seedPath = Join-Path $projectRoot 'SaveSeeds/Fresh/userdata'
    Assert-NoReparseAncestor $seedPath
    [IO.Directory]::CreateDirectory($seedPath) | Out-Null
    $defaults = [ordered]@{ 'settings.txt'="0 0 0 0 0 1 1 0`n"; 'native_selection.txt'="0 0`n" }
    foreach ($name in $defaults.Keys) {
        $path = Join-Path $seedPath $name
        if (Test-Path -LiteralPath $path) {
            if ([IO.File]::ReadAllText($path) -ne $defaults[$name]) { throw "Fresh seed already differs; no overwrite: $path" }
        } else { [IO.File]::WriteAllText($path, $defaults[$name], (New-Object Text.UTF8Encoding($false))) }
    }
}
$manifest = [ordered]@{
    schema='idas3-unity-runtime-data-v1'; nativeRelease='0.3.29'; generatedUtc=[DateTime]::UtcNow.ToString('o')
    sourceRoot=$SourceRoot; destinationRoot=$DestinationRoot; staged=(-not [bool]$InventoryOnly)
    fileCount=$records.Count; bytes=$totalBytes; files=@($records.ToArray())
    handlingSources=$handlingBefore; handlingUnchanged=$true; sourceSaveFilesChecked=$saveBefore.Count; sourceSavesUnchanged=$true
    savesStaged=$false; optionalFreshSaveSeed=$seedPath
}
Write-Json $ManifestPath $manifest
if (-not $InventoryOnly) { Write-Json (Join-Path $DestinationRoot 'data.manifest.json') $manifest }
Write-Host ("{0}: {1:N0} files, {2:N0} bytes; {3} handling source hashes and {4} copied-source save hashes unchanged." -f $(if($InventoryOnly){'Inventoried'}else{'Staged and verified'}),$records.Count,$totalBytes,$handlingBefore.Count,$saveBefore.Count)
Write-Host "Manifest: $ManifestPath"
