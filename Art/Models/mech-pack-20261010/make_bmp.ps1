# Convert every jpg/png in <dir> to a 24-bit BMP of at most <size> px (ASCII-only script: PS 5.1 reads BOM-less files as ANSI).
# Usage: make_bmp.ps1 -Dir <out>\tex [-Size 512]
param([string]$Dir, [int]$Size = 512)
Add-Type -AssemblyName System.Drawing
Get-ChildItem -LiteralPath $Dir | Where-Object { $_.Extension -in ".jpg", ".png" } | ForEach-Object {
  $src = [System.Drawing.Image]::FromFile($_.FullName)
  $w = [Math]::Min($Size, $src.Width); $h = [Math]::Min($Size, $src.Height)
  $bmp = New-Object System.Drawing.Bitmap($w, $h, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
  $g.DrawImage($src, 0, 0, $w, $h)
  $g.Dispose(); $src.Dispose()
  $out = [System.IO.Path]::ChangeExtension($_.FullName, ".bmp")
  $bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Bmp); $bmp.Dispose()
}
"done"
