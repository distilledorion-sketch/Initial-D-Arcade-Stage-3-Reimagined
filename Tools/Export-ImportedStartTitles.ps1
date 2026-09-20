$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
$folder=Join-Path $PSScriptRoot '../Native/data/original_assets/hud/imported_start_names'
[IO.Directory]::CreateDirectory($folder)|Out-Null
# Imported courses have no D3 title texture. Match the existing serif italic
# menu type and blue/white race-title treatment; retain this reproducible source.
$writer=[IO.BinaryWriter]::new([IO.File]::Create((Join-Path $folder 'titles.idastex')))
try {
 $writer.Write([Text.Encoding]::ASCII.GetBytes("IDAS3T1`0"));$writer.Write([uint32]1);$writer.Write([uint32]2)
 $index=0
 foreach($name in @('Hakone','Sadamine')){
  $path=[Drawing.Drawing2D.GraphicsPath]::new()
  $font=[Drawing.FontFamily]::new('Times New Roman')
  $path.AddString($name,$font,3,52,[Drawing.PointF]::new(0,0),[Drawing.StringFormat]::GenericTypographic)
  $bounds=$path.GetBounds();$matrix=[Drawing.Drawing2D.Matrix]::new();$matrix.Translate((5-$bounds.X),(5-$bounds.Y));$path.Transform($matrix)
  $bitmap=[Drawing.Bitmap]::new([int][Math]::Ceiling($bounds.Width+10),[int][Math]::Ceiling($bounds.Height+10))
  $g=[Drawing.Graphics]::FromImage($bitmap);$g.SmoothingMode='AntiAlias'
  $dark=[Drawing.Pen]::new([Drawing.Color]::FromArgb(255,10,30,110),7);$dark.LineJoin='Round'
  $white=[Drawing.Pen]::new([Drawing.Color]::FromArgb(255,220,248,255),3);$white.LineJoin='Round'
  $brush=[Drawing.Drawing2D.LinearGradientBrush]::new([Drawing.PointF]::new(0,5),[Drawing.PointF]::new(0,($bitmap.Height-5)),[Drawing.Color]::FromArgb(255,108,207,255),[Drawing.Color]::FromArgb(255,19,40,123))
  $g.DrawPath($dark,$path);$g.DrawPath($white,$path);$g.FillPath($brush,$path)
  $bitmap.Save((Join-Path $folder ($name.ToLower()+'.png')),[Drawing.Imaging.ImageFormat]::Png)
  $writer.Write([uint32]$index);$writer.Write([uint32]$bitmap.Width);$writer.Write([uint32]$bitmap.Height);$writer.Write([uint32]($bitmap.Width*$bitmap.Height*4))
  for($y=0;$y -lt $bitmap.Height;$y++){for($x=0;$x -lt $bitmap.Width;$x++){$c=$bitmap.GetPixel($x,$y);$writer.Write([byte]$c.R);$writer.Write([byte]$c.G);$writer.Write([byte]$c.B);$writer.Write([byte]$c.A)}}
  $g.Dispose();$bitmap.Dispose();$path.Dispose();$font.Dispose();$matrix.Dispose();$dark.Dispose();$white.Dispose();$brush.Dispose();$index++
 }
} finally {$writer.Dispose()}
