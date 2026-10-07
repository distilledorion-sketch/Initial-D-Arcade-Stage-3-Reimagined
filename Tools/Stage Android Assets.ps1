[CmdletBinding()]
param(
    [string]$DestinationRoot,
    [string]$ManifestPath
)

$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
if (-not $DestinationRoot) { $DestinationRoot = Join-Path $projectRoot 'Assets/StreamingAssets/IDAS3' }
if (-not $ManifestPath) { $ManifestPath = Join-Path $projectRoot 'Staging/android-game-data-manifest.json' }
$destination = [IO.Path]::GetFullPath($DestinationRoot).TrimEnd('\','/')
$manifestFile = [IO.Path]::GetFullPath($ManifestPath)
$runtimeSource = Join-Path $projectRoot 'RuntimeAssets'
if (-not (Test-Path -LiteralPath $runtimeSource -PathType Container)) { throw "RuntimeAssets is missing: $runtimeSource" }
if (-not (Test-Path -LiteralPath $manifestFile -PathType Leaf)) { throw "Base Android manifest is missing. Run Stage-GameData.ps1 first: $manifestFile" }
$manifest = Get-Content -LiteralPath $manifestFile -Raw | ConvertFrom-Json
if ($manifest.schema -ne 'idas3-unity-runtime-data-v1' -or $null -eq $manifest.files) { throw 'The Android data manifest has an unsupported schema.' }

function Hash-File([string]$Path) { return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant() }
$records = [Collections.Generic.List[object]]::new()
foreach ($file in @(Get-ChildItem -LiteralPath $runtimeSource -File -Recurse -Force | Sort-Object FullName)) {
    $relative = $file.FullName.Substring($runtimeSource.Length + 1).Replace('\','/')
    $recordPath = "RuntimeAssets/$relative"
    $target = Join-Path (Join-Path $destination 'data/RuntimeAssets') $relative
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target)) | Out-Null
    if (-not (Test-Path -LiteralPath $target -PathType Leaf) -or (Get-Item -LiteralPath $target).Length -ne $file.Length -or (Hash-File $target) -ne (Hash-File $file.FullName)) {
        Copy-Item -LiteralPath $file.FullName -Destination $target -Force
    }
    $records.Add([ordered]@{path=$recordPath;bytes=$file.Length;sha256=(Hash-File $file.FullName)})
}

$existing = @($manifest.files | Where-Object { $_.path -notlike 'RuntimeAssets/*' })
$all = @($existing) + @($records.ToArray())
$manifest.files = $all
$manifest.fileCount = $all.Count
$manifest.bytes = [long](($all | ForEach-Object { [long]$_.bytes } | Measure-Object -Sum).Sum)
$manifest.generatedUtc = [DateTime]::UtcNow.ToString('o')
$json = $manifest | ConvertTo-Json -Depth 10
[IO.File]::WriteAllText((Join-Path $destination 'data.manifest.json'),$json+"`n",(New-Object Text.UTF8Encoding($false)))
[IO.File]::WriteAllText($manifestFile,$json+"`n",(New-Object Text.UTF8Encoding($false)))
Write-Host ("Staged and hashed {0} RuntimeAssets files into {1}. Manifest now contains {2} files." -f $records.Count,$destination,$all.Count)
