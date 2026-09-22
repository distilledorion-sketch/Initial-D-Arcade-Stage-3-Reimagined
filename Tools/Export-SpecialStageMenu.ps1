param([Parameter(Mandatory=$true)][string]$PackRoot,[Parameter(Mandatory=$true)][string]$Background,[Parameter(Mandatory=$true)][string]$PreviewDirectory)
Add-Type -AssemblyName System.Drawing
$ErrorActionPreference='Stop'
$base=$PackRoot
$manifest=Get-Content -LiteralPath (Join-Path $base 'manifest.json') -Raw | ConvertFrom-Json
$title=$manifest.title
$tileTitle=@{12='Myogi SS';13='Usui SS';14='Momiji'}[[int]$manifest.courseId]
$first=$manifest.raceMarkers[0].startIndex;$last=$manifest.raceMarkers[0].goalIndex
[IO.Directory]::CreateDirectory($PreviewDirectory) | Out-Null
$source=[Drawing.Bitmap]::FromFile($Background)
function TextPath($g,$text,$size,$x,$y,$fill,$outline,$stroke){
 $path=[Drawing.Drawing2D.GraphicsPath]::new()
 $path.AddString($text,[Drawing.FontFamily]::new('Times New Roman'),3,[single]$size,[Drawing.PointF]::new($x,$y),[Drawing.StringFormat]::GenericTypographic)
 $pen=[Drawing.Pen]::new($outline,$stroke);$pen.LineJoin=[Drawing.Drawing2D.LineJoin]::Round
 $brush=[Drawing.SolidBrush]::new($fill);$g.DrawPath($pen,$path);$g.FillPath($brush,$path)
 $path.Dispose();$pen.Dispose();$brush.Dispose()
}
$card=[Drawing.Bitmap]::new(640,312)
$g=[Drawing.Graphics]::FromImage($card);$g.SmoothingMode='AntiAlias';$g.InterpolationMode='HighQualityBicubic'
$g.DrawImage($source,[Drawing.Rectangle]::new(0,0,640,360),0,0,$source.Width,$source.Height,[Drawing.GraphicsUnit]::Pixel)
$shade=[Drawing.SolidBrush]::new([Drawing.Color]::FromArgb(35,0,0,0));$g.FillRectangle($shade,0,0,640,312)
TextPath $g ($title -replace ' \(Special Stage\)','') 52 22 35 ([Drawing.Color]::Black) ([Drawing.Color]::White) 4
if($manifest.courseId -ne 14){TextPath $g 'SPECIAL STAGE' 21 24 88 ([Drawing.Color]::White) ([Drawing.Color]::Black) 2}
$g.DrawLine([Drawing.Pens]::White,0,117,397,117)
$r=[IO.BinaryReader]::new([IO.File]::OpenRead((Join-Path $base 'road.bin')));$null=$r.ReadBytes(4);$count=$r.ReadInt32();$points=@()
# Match the original Special Stage course-select atlas: screen axes follow world X/Z.
for($i=0;$i -lt $count;$i++){ $x=$r.ReadSingle();$null=$r.ReadSingle();$z=$r.ReadSingle();if($i -ge $first -and $i -le $last -and ($i%4 -eq 0 -or $i -eq $first -or $i -eq $last)){$points+=,[double[]]@($x,$z)} };$r.Dispose()
$minX=($points|ForEach-Object {$_[0]}|Measure-Object -Minimum).Minimum;$maxX=($points|ForEach-Object {$_[0]}|Measure-Object -Maximum).Maximum
$minY=($points|ForEach-Object {$_[1]}|Measure-Object -Minimum).Minimum;$maxY=($points|ForEach-Object {$_[1]}|Measure-Object -Maximum).Maximum
$scale=[Math]::Min(165/($maxX-$minX),228/($maxY-$minY))
$line=[Drawing.PointF[]]@($points|ForEach-Object {[Drawing.PointF]::new((435+($_[0]-$minX)*$scale),(42+($_[1]-$minY)*$scale))})
$pen=[Drawing.Pen]::new([Drawing.Color]::FromArgb(150,0,0,0),5);$g.DrawLines($pen,$line);$pen.Dispose()
$pen=[Drawing.Pen]::new([Drawing.Color]::White,2);$g.DrawLines($pen,$line);$pen.Dispose()
foreach($end in @(0,($line.Length-1))){$point=$line[$end];$g.FillEllipse([Drawing.Brushes]::White,($point.X-3),($point.Y-3),6,6)}
# Place labels outside nearby bends, while retaining their respective start/end anchors.
$downY=[Math]::Max(8,$line[0].Y-20);$upY=[Math]::Min(275,$line[-1].Y+5)
switch([int]$manifest.courseId){
 12 {$downY=$line[0].Y+8;$upY=$line[-1].Y-20}
 13 {$downY=$line[0].Y+6;$upY=$line[-1].Y+25}
 14 {$upY=42+($maxY-$minY)*$scale+7}
}
TextPath $g 'DOWNHILL' 14 ([Math]::Min(543,$line[0].X-33)) $downY ([Drawing.Color]::White) ([Drawing.Color]::Black) 2
TextPath $g 'UPHILL' 14 ([Math]::Min(550,$line[-1].X-22)) $upY ([Drawing.Color]::White) ([Drawing.Color]::Black) 2
$g.DrawLine([Drawing.Pens]::White,435,292,[single](435+1000*$scale),292)
TextPath $g '1 km' 13 452 294 ([Drawing.Color]::White) ([Drawing.Color]::Black) 2
$g.Dispose();$card.Save((Join-Path $PreviewDirectory 'menu-card.png'))
$tile=[Drawing.Bitmap]::new(96,64);$g=[Drawing.Graphics]::FromImage($tile);$g.InterpolationMode='HighQualityBicubic';$g.SmoothingMode='AntiAlias';$g.DrawImage($source,0,0,96,64)
$g.FillRectangle([Drawing.Brushes]::DarkOrange,0,0,36,14)
$font=[Drawing.Font]::new('Arial',11,[Drawing.FontStyle]::Bold,[Drawing.GraphicsUnit]::Pixel);$g.DrawString('HARD',$font,[Drawing.Brushes]::White,[Drawing.PointF]::new(1,0));$font.Dispose()
TextPath $g $tileTitle 19 6 37 ([Drawing.Color]::White) ([Drawing.Color]::Black) 2
$g.Dispose();$tile.Save((Join-Path $PreviewDirectory 'menu-tile.png'))
$writer=[IO.BinaryWriter]::new([IO.File]::Create((Join-Path $base 'menu.idastex')))
$writer.Write([Text.Encoding]::ASCII.GetBytes("IDAS3T1`0"));$writer.Write([uint32]1);$writer.Write([uint32]2)
$index=0
foreach($bitmap in @($card,$tile)){
 $writer.Write([uint32]$index);$writer.Write([uint32]$bitmap.Width);$writer.Write([uint32]$bitmap.Height);$writer.Write([uint32]($bitmap.Width*$bitmap.Height*4))
 $bits=$bitmap.LockBits([Drawing.Rectangle]::new(0,0,$bitmap.Width,$bitmap.Height),[Drawing.Imaging.ImageLockMode]::ReadOnly,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
 $bytes=[byte[]]::new($bitmap.Width*$bitmap.Height*4);[Runtime.InteropServices.Marshal]::Copy($bits.Scan0,$bytes,0,$bytes.Length);$bitmap.UnlockBits($bits)
 for($i=0;$i -lt $bytes.Length;$i+=4){$b=$bytes[$i];$bytes[$i]=$bytes[$i+2];$bytes[$i+2]=$b};$writer.Write($bytes);$index++
 $bitmap.Dispose()
}
$writer.Dispose();$source.Dispose();$shade.Dispose()
