param()

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.Drawing

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = Split-Path -Parent $scriptDir
$transparent = [System.Drawing.Color]::FromArgb(0, 0, 0, 0)

function Ensure-Directory([string]$Path) {
  $dir = Split-Path -Parent $Path
  if ($dir) {
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
  }
}

function Get-AlphaBounds([System.Drawing.Bitmap]$Bitmap, [int]$Threshold = 16) {
  $minX = $Bitmap.Width
  $minY = $Bitmap.Height
  $maxX = -1
  $maxY = -1

  for ($y = 0; $y -lt $Bitmap.Height; $y++) {
    for ($x = 0; $x -lt $Bitmap.Width; $x++) {
      if ($Bitmap.GetPixel($x, $y).A -le $Threshold) {
        continue
      }
      if ($x -lt $minX) { $minX = $x }
      if ($y -lt $minY) { $minY = $y }
      if ($x -gt $maxX) { $maxX = $x }
      if ($y -gt $maxY) { $maxY = $y }
    }
  }

  if ($maxX -lt 0 -or $maxY -lt 0) {
    throw "No visible pixels were found in the source bitmap."
  }

  return [System.Drawing.Rectangle]::FromLTRB($minX, $minY, $maxX + 1, $maxY + 1)
}

function Get-TightBitmap([string]$Path, [int]$Threshold = 16) {
  $fullPath = (Resolve-Path $Path).Path
  $original = [System.Drawing.Bitmap]::FromFile($fullPath)
  try {
    $bounds = Get-AlphaBounds -Bitmap $original -Threshold $Threshold
    return $original.Clone($bounds, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  } finally {
    $original.Dispose()
  }
}

function New-FittedBitmap(
  [System.Drawing.Image]$Source,
  [int]$Width,
  [int]$Height,
  [double]$PaddingRatioX,
  [double]$PaddingRatioY,
  [System.Drawing.Color]$BackgroundColor
) {
  $bitmap = New-Object System.Drawing.Bitmap $Width, $Height
  $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
  try {
    $graphics.Clear($BackgroundColor)
    $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
    $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

    $innerWidth = [Math]::Max(1, [int][Math]::Round($Width * (1.0 - (2.0 * $PaddingRatioX))))
    $innerHeight = [Math]::Max(1, [int][Math]::Round($Height * (1.0 - (2.0 * $PaddingRatioY))))
    $scale = [Math]::Min($innerWidth / $Source.Width, $innerHeight / $Source.Height)
    $drawWidth = [Math]::Max(1, [int][Math]::Round($Source.Width * $scale))
    $drawHeight = [Math]::Max(1, [int][Math]::Round($Source.Height * $scale))
    $drawX = [int][Math]::Round(($Width - $drawWidth) / 2.0)
    $drawY = [int][Math]::Round(($Height - $drawHeight) / 2.0)

    $graphics.DrawImage($Source, (New-Object System.Drawing.Rectangle -ArgumentList $drawX, $drawY, $drawWidth, $drawHeight))
    return $bitmap
  } finally {
    $graphics.Dispose()
  }
}

function Draw-FittedImage(
  [System.Drawing.Graphics]$Graphics,
  [System.Drawing.Image]$Source,
  [System.Drawing.Rectangle]$Bounds
) {
  $scale = [Math]::Min($Bounds.Width / $Source.Width, $Bounds.Height / $Source.Height)
  $drawWidth = [Math]::Max(1, [int][Math]::Round($Source.Width * $scale))
  $drawHeight = [Math]::Max(1, [int][Math]::Round($Source.Height * $scale))
  $drawX = $Bounds.X + [int][Math]::Round(($Bounds.Width - $drawWidth) / 2.0)
  $drawY = $Bounds.Y + [int][Math]::Round(($Bounds.Height - $drawHeight) / 2.0)
  $Graphics.DrawImage($Source, (New-Object System.Drawing.Rectangle -ArgumentList $drawX, $drawY, $drawWidth, $drawHeight))
}

function Save-Png([System.Drawing.Bitmap]$Bitmap, [string]$Path) {
  Ensure-Directory $Path
  $Bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
}

function Get-PngBytes([System.Drawing.Bitmap]$Bitmap) {
  $stream = New-Object System.IO.MemoryStream
  try {
    $Bitmap.Save($stream, [System.Drawing.Imaging.ImageFormat]::Png)
    return $stream.ToArray()
  } finally {
    $stream.Dispose()
  }
}

function Write-IcoFile([string]$DestinationPath, [hashtable]$PngBySize) {
  $sizes = @($PngBySize.Keys | Sort-Object)
  $stream = New-Object System.IO.MemoryStream
  $writer = New-Object System.IO.BinaryWriter($stream)
  try {
    $writer.Write([UInt16]0)
    $writer.Write([UInt16]1)
    $writer.Write([UInt16]$sizes.Count)

    $offset = 6 + (16 * $sizes.Count)
    foreach ($size in $sizes) {
      $png = [byte[]]$PngBySize[$size]
      $sizeByte = if ($size -ge 256) { [byte]0 } else { [byte]$size }
      $writer.Write($sizeByte)
      $writer.Write($sizeByte)
      $writer.Write([byte]0)
      $writer.Write([byte]0)
      $writer.Write([UInt16]0)
      $writer.Write([UInt16]32)
      $writer.Write([UInt32]$png.Length)
      $writer.Write([UInt32]$offset)
      $offset += $png.Length
    }

    foreach ($size in $sizes) {
      $writer.Write([byte[]]$PngBySize[$size])
    }

    Ensure-Directory $DestinationPath
    [System.IO.File]::WriteAllBytes($DestinationPath, $stream.ToArray())
  } finally {
    $writer.Dispose()
    $stream.Dispose()
  }
}

function Write-BigEndianUInt32([System.IO.BinaryWriter]$Writer, [UInt32]$Value) {
  $bytes = [System.BitConverter]::GetBytes($Value)
  if ([System.BitConverter]::IsLittleEndian) {
    [Array]::Reverse($bytes)
  }
  $Writer.Write($bytes)
}

function Write-Ascii([System.IO.BinaryWriter]$Writer, [string]$Value) {
  $Writer.Write([System.Text.Encoding]::ASCII.GetBytes($Value))
}

function Write-IcnsFile([string]$DestinationPath, [hashtable]$ChunkDataByType, [string[]]$ChunkOrder) {
  $totalSize = 8
  foreach ($chunkType in $ChunkOrder) {
    $totalSize += 8 + ([byte[]]$ChunkDataByType[$chunkType]).Length
  }

  $stream = New-Object System.IO.MemoryStream
  $writer = New-Object System.IO.BinaryWriter($stream)
  try {
    Write-Ascii -Writer $writer -Value "icns"
    Write-BigEndianUInt32 -Writer $writer -Value ([UInt32]$totalSize)

    foreach ($chunkType in $ChunkOrder) {
      $data = [byte[]]$ChunkDataByType[$chunkType]
      Write-Ascii -Writer $writer -Value $chunkType
      Write-BigEndianUInt32 -Writer $writer -Value ([UInt32](8 + $data.Length))
      $writer.Write($data)
    }

    Ensure-Directory $DestinationPath
    [System.IO.File]::WriteAllBytes($DestinationPath, $stream.ToArray())
  } finally {
    $writer.Dispose()
    $stream.Dispose()
  }
}

function New-StoreAssetBitmap(
  [System.Drawing.Image]$Source,
  [int]$Width,
  [int]$Height,
  [string]$Title
) {
  $bitmap = New-Object System.Drawing.Bitmap $Width, $Height
  $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
  try {
    $graphics.Clear([System.Drawing.Color]::FromArgb(255, 5, 8, 22))
    $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
    $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
    $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
    $graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::ClearTypeGridFit

    $fullRect = New-Object System.Drawing.Rectangle -ArgumentList 0, 0, $Width, $Height
    $gradientBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
      $fullRect,
      ([System.Drawing.Color]::FromArgb(255, 5, 8, 22)),
      ([System.Drawing.Color]::FromArgb(255, 12, 22, 46)),
      35.0
    )
    try {
      $graphics.FillRectangle($gradientBrush, $fullRect)
    } finally {
      $gradientBrush.Dispose()
    }

    $orbBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(88, 78, 203, 255))
    try {
      $orbSize = [Math]::Max([int]($Width * 0.52), [int]($Height * 0.52))
      $graphics.FillEllipse($orbBrush, [int]($Width * 0.48), [int](-0.10 * $Height), $orbSize, $orbSize)
    } finally {
      $orbBrush.Dispose()
    }

    if ($Width -gt $Height) {
      $iconSize = [Math]::Max(48, [int][Math]::Round($Height * 0.58))
      $iconRect = New-Object System.Drawing.Rectangle -ArgumentList ([int][Math]::Round($Width * 0.08)), ([int][Math]::Round(($Height - $iconSize) / 2.0)), $iconSize, $iconSize
      Draw-FittedImage -Graphics $graphics -Source $Source -Bounds $iconRect

      $titleFontSize = [Math]::Max(18, [int][Math]::Round($Height * 0.17))
      $captionFontSize = [Math]::Max(11, [int][Math]::Round($Height * 0.10))
      $titleFont = New-Object System.Drawing.Font("Segoe UI Semibold", $titleFontSize, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
      $captionFont = New-Object System.Drawing.Font("Segoe UI", $captionFontSize, [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)
      $titleBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 244, 251, 255))
      $captionBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 167, 184, 211))
      try {
        $textX = [int][Math]::Round($Width * 0.08) + $iconSize + [int][Math]::Round($Width * 0.05)
        $graphics.DrawString($Title, $titleFont, $titleBrush, [single]$textX, [single][Math]::Round($Height * 0.24))
        $graphics.DrawString("ViewMesh", $captionFont, $captionBrush, [single]$textX, [single][Math]::Round($Height * 0.55))
      } finally {
        $titleFont.Dispose()
        $captionFont.Dispose()
        $titleBrush.Dispose()
        $captionBrush.Dispose()
      }
    } else {
      $iconInset = [Math]::Max(4, [int][Math]::Round($Width * 0.14))
      $iconRect = New-Object System.Drawing.Rectangle -ArgumentList $iconInset, $iconInset, ($Width - (2 * $iconInset)), ($Height - (2 * $iconInset))
      Draw-FittedImage -Graphics $graphics -Source $Source -Bounds $iconRect
    }

    return $bitmap
  } finally {
    $graphics.Dispose()
  }
}

Write-Host "Regenerating application icons from the current high-resolution master..." -ForegroundColor Cyan

$appSource = Get-TightBitmap -Path (Join-Path $repoRoot "src\resources\icons\macos\AppIcon.iconset\icon_512x512@2x.png")
try {
  $windowsSizes = @(16, 20, 24, 32, 40, 48, 64, 128, 256)
  $windowsPngBytes = @{}
  foreach ($size in $windowsSizes) {
    $bitmap = New-FittedBitmap -Source $appSource -Width $size -Height $size -PaddingRatioX 0.05 -PaddingRatioY 0.05 -BackgroundColor $transparent
    try {
      $path = Join-Path $repoRoot ("src\resources\icons\windows\png\app\app_icon_{0}.png" -f $size)
      Save-Png -Bitmap $bitmap -Path $path
      $windowsPngBytes[$size] = Get-PngBytes -Bitmap $bitmap
    } finally {
      $bitmap.Dispose()
    }
  }
  Write-IcoFile -DestinationPath (Join-Path $repoRoot "src\resources\icons\windows\app_icon_v2.ico") -PngBySize $windowsPngBytes

  $linuxSizes = @(16, 24, 32, 48, 64, 128, 256, 512)
  foreach ($size in $linuxSizes) {
    $bitmap = New-FittedBitmap -Source $appSource -Width $size -Height $size -PaddingRatioX 0.05 -PaddingRatioY 0.05 -BackgroundColor $transparent
    try {
      $path = Join-Path $repoRoot ("src\resources\icons\linux\hicolor\{0}x{0}\apps\lanscreenshare.png" -f $size)
      Save-Png -Bitmap $bitmap -Path $path
    } finally {
      $bitmap.Dispose()
    }
  }

  $macIconSpecs = @(
    @{ Name = "icon_16x16.png"; Size = 16; Chunk = "icp4" },
    @{ Name = "icon_16x16@2x.png"; Size = 32; Chunk = "ic11" },
    @{ Name = "icon_32x32.png"; Size = 32; Chunk = "icp5" },
    @{ Name = "icon_32x32@2x.png"; Size = 64; Chunk = "ic12" },
    @{ Name = "icon_128x128.png"; Size = 128; Chunk = "ic07" },
    @{ Name = "icon_128x128@2x.png"; Size = 256; Chunk = "ic13" },
    @{ Name = "icon_256x256.png"; Size = 256; Chunk = "ic08" },
    @{ Name = "icon_256x256@2x.png"; Size = 512; Chunk = "ic14" },
    @{ Name = "icon_512x512.png"; Size = 512; Chunk = "ic09" },
    @{ Name = "icon_512x512@2x.png"; Size = 1024; Chunk = "ic10" }
  )
  $icnsChunks = @{}
  $icnsOrder = New-Object System.Collections.Generic.List[string]
  foreach ($spec in $macIconSpecs) {
    $bitmap = New-FittedBitmap -Source $appSource -Width $spec.Size -Height $spec.Size -PaddingRatioX 0.05 -PaddingRatioY 0.05 -BackgroundColor $transparent
    try {
      $path = Join-Path $repoRoot ("src\resources\icons\macos\AppIcon.iconset\{0}" -f $spec.Name)
      Save-Png -Bitmap $bitmap -Path $path
      $icnsChunks[$spec.Chunk] = Get-PngBytes -Bitmap $bitmap
      $icnsOrder.Add($spec.Chunk) | Out-Null
    } finally {
      $bitmap.Dispose()
    }
  }
  Write-IcnsFile -DestinationPath (Join-Path $repoRoot "src\resources\icons\macos\AppIcon.icns") -ChunkDataByType $icnsChunks -ChunkOrder $icnsOrder.ToArray()

  $storeSpecs = @(
    @{ Name = "StoreLogo.png"; Width = 50; Height = 50; Title = "Store" },
    @{ Name = "Square44x44Logo.png"; Width = 44; Height = 44; Title = "Tile" },
    @{ Name = "Square71x71Logo.png"; Width = 71; Height = 71; Title = "Tile" },
    @{ Name = "Square150x150Logo.png"; Width = 150; Height = 150; Title = "Tile" },
    @{ Name = "Square310x310Logo.png"; Width = 310; Height = 310; Title = "Tile" },
    @{ Name = "Wide310x150Logo.png"; Width = 310; Height = 150; Title = "ViewMesh Host" },
    @{ Name = "SplashScreen.png"; Width = 620; Height = 300; Title = "ViewMesh Host" }
  )
  foreach ($spec in $storeSpecs) {
    $bitmap = New-StoreAssetBitmap -Source $appSource -Width $spec.Width -Height $spec.Height -Title $spec.Title
    try {
      $path = Join-Path $repoRoot ("src\resources\icons\windows\store\{0}" -f $spec.Name)
      Save-Png -Bitmap $bitmap -Path $path
    } finally {
      $bitmap.Dispose()
    }
  }
} finally {
  $appSource.Dispose()
}

Write-Host "Icon regeneration completed." -ForegroundColor Green
