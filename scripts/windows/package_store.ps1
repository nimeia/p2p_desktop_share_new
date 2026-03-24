param(
  [ValidateSet("Debug", "Release")]
  [string]$Config = "Release",
  [ValidateSet("x64")]
  [string]$Arch = "x64",
  [string]$Version = "",
  [switch]$SkipBuild,
  [string]$OutputRoot = "",
  [Parameter(Mandatory = $true)]
  [string]$IdentityName,
  [Parameter(Mandatory = $true)]
  [string]$Publisher,
  [string]$PublisherDisplayName = "",
  [string]$DisplayName = "LAN Screen Share Host",
  [string]$Description = "LAN Screen Share desktop host for local viewer sessions.",
  [string]$ApplicationId = "LanScreenShareHostApp",
  [string]$Executable = "LanScreenShareHostApp.exe",
  [string]$AssetsDir = "",
  [string]$MinVersion = "10.0.19041.0",
  [string]$MaxVersionTested = "",
  [string]$BackgroundColor = "#050816",
  [string]$SignPfx = "",
  [string]$SignPassword = "",
  [string]$TimestampUrl = "http://timestamp.digicert.com",
  [switch]$SkipUploadBundle
)

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
. (Join-Path $scriptDir "common.ps1")
. (Join-Path $scriptDir "package_common.ps1")

function Find-WindowsSdkToolSet([string]$ArchName) {
  $kitsRoot = Join-Path ${env:ProgramFiles(x86)} "Windows Kits\10\bin"
  if (-not (Test-Path -LiteralPath $kitsRoot)) {
    Fail "Windows 10 SDK tools were not found under $kitsRoot"
  }

  $versionDirs = Get-ChildItem -LiteralPath $kitsRoot -Directory |
    Where-Object { $_.Name -match '^\d+\.\d+\.\d+\.\d+$' } |
    Sort-Object { [version]$_.Name } -Descending

  foreach ($dir in $versionDirs) {
    $toolDir = Join-Path $dir.FullName $ArchName
    $makeAppx = Join-Path $toolDir "makeappx.exe"
    $signTool = Join-Path $toolDir "signtool.exe"
    if ((Test-Path -LiteralPath $makeAppx) -and (Test-Path -LiteralPath $signTool)) {
      return [pscustomobject]@{
        Version  = $dir.Name
        MakeAppx = $makeAppx
        SignTool = $signTool
      }
    }
  }

  Fail "Unable to locate makeappx.exe and signtool.exe for architecture '$ArchName'."
}

function ConvertTo-AppxVersion([string]$Value) {
  $match = [regex]::Match($Value, '(\d+)\.(\d+)\.(\d+)(?:\.(\d+))?')
  if (-not $match.Success) {
    Fail "Version '$Value' cannot be converted to the 4-part numeric Appx/MSIX version format."
  }

  $major = [int]$match.Groups[1].Value
  $minor = [int]$match.Groups[2].Value
  $patch = [int]$match.Groups[3].Value
  $build = if ($match.Groups[4].Success) { [int]$match.Groups[4].Value } else { 0 }
  return "$major.$minor.$patch.$build"
}

function Get-SafeFileComponent([string]$Value) {
  if ([string]::IsNullOrWhiteSpace($Value)) { return "package" }
  return ($Value -replace '[^A-Za-z0-9._-]', '_')
}

function Copy-RequiredPayload([string]$DesktopDir, [string]$RepoRoot, [string]$StageDir) {
  $required = @(
    (Join-Path $DesktopDir "LanScreenShareHostApp.exe"),
    (Join-Path $DesktopDir "lan_screenshare_server.exe"),
    (Join-Path $DesktopDir "www"),
    (Join-Path $DesktopDir "webui"),
    (Join-Path $RepoRoot "scripts\windows\Run-NetworkDiagnostics.ps1"),
    (Join-Path $RepoRoot "scripts\windows\Check-WebView2Runtime.ps1"),
    (Join-Path $RepoRoot "scripts\windows\common.ps1")
  )
  foreach ($item in $required) {
    if (-not (Test-Path -LiteralPath $item)) {
      Fail "Missing required Store package input: $item"
    }
  }

  Copy-Item -Path (Join-Path $DesktopDir "*") -Destination $StageDir -Recurse -Force
  Get-ChildItem -LiteralPath $StageDir -Recurse -File -ErrorAction SilentlyContinue |
    Where-Object { @(".pdb", ".ilk", ".exp", ".lib") -contains $_.Extension.ToLowerInvariant() } |
    Remove-Item -Force -ErrorAction SilentlyContinue

  $helperStageDir = Join-Path $StageDir "scripts\windows"
  New-Item -ItemType Directory -Force -Path $helperStageDir | Out-Null
  @(
    "common.ps1",
    "Run-NetworkDiagnostics.ps1",
    "Check-WebView2Runtime.ps1"
  ) | ForEach-Object {
    Copy-Item -LiteralPath (Join-Path $RepoRoot "scripts\windows\$_") -Destination (Join-Path $helperStageDir $_) -Force
  }
}

function ConvertTo-DrawingColor([string]$Hex) {
  $value = $Hex.Trim()
  if ($value.StartsWith("#")) { $value = $value.Substring(1) }
  if ($value.Length -ne 6) {
    Fail "Color '$Hex' must be in #RRGGBB format."
  }
  $r = [Convert]::ToInt32($value.Substring(0, 2), 16)
  $g = [Convert]::ToInt32($value.Substring(2, 2), 16)
  $b = [Convert]::ToInt32($value.Substring(4, 2), 16)
  return [System.Drawing.Color]::FromArgb(255, $r, $g, $b)
}

function New-PlaceholderAsset(
  [string]$Path,
  [int]$Width,
  [int]$Height,
  [string]$Label,
  [string]$Title,
  [string]$BackgroundHex
) {
  Add-Type -AssemblyName System.Drawing

  $background = ConvertTo-DrawingColor $BackgroundHex
  $accent = [System.Drawing.Color]::FromArgb(255, 78, 203, 255)
  $text = [System.Drawing.Color]::FromArgb(255, 244, 251, 255)
  $muted = [System.Drawing.Color]::FromArgb(255, 167, 184, 211)

  $bitmap = New-Object System.Drawing.Bitmap $Width, $Height
  $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
  try {
    $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
    $graphics.TextRenderingHint = [System.Drawing.Text.TextRenderingHint]::ClearTypeGridFit

    $graphics.Clear($background)
    $gradientRect = New-Object System.Drawing.Rectangle 0, 0, $Width, $Height
    $gradientBrush = New-Object System.Drawing.Drawing2D.LinearGradientBrush `
      $gradientRect, `
      ([System.Drawing.Color]::FromArgb(255, 5, 8, 22)), `
      ([System.Drawing.Color]::FromArgb(255, 12, 22, 46)), `
      35
    try {
      $graphics.FillRectangle($gradientBrush, $gradientRect)
    } finally {
      $gradientBrush.Dispose()
    }

    $orbBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(88, $accent))
    try {
      $orbSize = [Math]::Max([int]($Width * 0.55), [int]($Height * 0.55))
      $graphics.FillEllipse($orbBrush, [int]($Width * 0.46), [int](-0.12 * $Height), $orbSize, $orbSize)
    } finally {
      $orbBrush.Dispose()
    }

    $lineBrush = New-Object System.Drawing.SolidBrush ([System.Drawing.Color]::FromArgb(255, 20, 28, 52))
    try {
      $graphics.FillRectangle($lineBrush, 0, [int]($Height * 0.74), $Width, [Math]::Max(6, [int]($Height * 0.06)))
    } finally {
      $lineBrush.Dispose()
    }

    $titleFontSize = if ($Width -ge 300) { [Math]::Max(22, [int]($Height * 0.12)) } else { [Math]::Max(9, [int]($Height * 0.24)) }
    $badgeFontSize = if ($Width -ge 300) { [Math]::Max(42, [int]($Height * 0.32)) } else { [Math]::Max(14, [int]($Height * 0.34)) }

    $titleFont = New-Object System.Drawing.Font("Segoe UI Semibold", $titleFontSize, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
    $badgeFont = New-Object System.Drawing.Font("Segoe UI", $badgeFontSize, [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
    $captionFont = New-Object System.Drawing.Font("Segoe UI", [Math]::Max(10, [int]($Height * 0.1)), [System.Drawing.FontStyle]::Regular, [System.Drawing.GraphicsUnit]::Pixel)

    $titleBrush = New-Object System.Drawing.SolidBrush $text
    $captionBrush = New-Object System.Drawing.SolidBrush $muted
    try {
      if ($Width -gt $Height) {
        $graphics.DrawString($Title, $titleFont, $titleBrush, 24.0, [Math]::Max(20.0, $Height * 0.18))
        $graphics.DrawString($Label, $badgeFont, $titleBrush, 24.0, [Math]::Max(58.0, $Height * 0.34))
      } else {
        $labelFormat = New-Object System.Drawing.StringFormat
        try {
          $labelFormat.Alignment = [System.Drawing.StringAlignment]::Center
          $labelFormat.LineAlignment = [System.Drawing.StringAlignment]::Center
          $graphics.DrawString($Label, $badgeFont, $titleBrush, (New-Object System.Drawing.RectangleF(0, 0, $Width, $Height * 0.72)), $labelFormat)
        } finally {
          $labelFormat.Dispose()
        }
        $graphics.DrawString($Title, $captionFont, $captionBrush, 8.0, [Math]::Max(6.0, $Height * 0.78))
      }
    } finally {
      $titleFont.Dispose()
      $badgeFont.Dispose()
      $captionFont.Dispose()
      $titleBrush.Dispose()
      $captionBrush.Dispose()
    }

    $dir = Split-Path -Parent $Path
    if ($dir) { New-Item -ItemType Directory -Force -Path $dir | Out-Null }
    $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
  } finally {
    $graphics.Dispose()
    $bitmap.Dispose()
  }
}

function Ensure-StoreAssets(
  [string]$DestinationDir,
  [string]$SourceDir,
  [string]$BackgroundHex,
  [string]$DisplayTitle
) {
  $assetSpecs = @(
    @{ Name = "StoreLogo.png"; Width = 50; Height = 50; Label = "LAN" },
    @{ Name = "Square44x44Logo.png"; Width = 44; Height = 44; Label = "LAN" },
    @{ Name = "Square71x71Logo.png"; Width = 71; Height = 71; Label = "LAN" },
    @{ Name = "Square150x150Logo.png"; Width = 150; Height = 150; Label = "LAN" },
    @{ Name = "Square310x310Logo.png"; Width = 310; Height = 310; Label = "LAN" },
    @{ Name = "Wide310x150Logo.png"; Width = 310; Height = 150; Label = "HOST" },
    @{ Name = "SplashScreen.png"; Width = 620; Height = 300; Label = "HOST" }
  )

  New-Item -ItemType Directory -Force -Path $DestinationDir | Out-Null
  $generated = New-Object System.Collections.Generic.List[string]

  foreach ($spec in $assetSpecs) {
    $destPath = Join-Path $DestinationDir $spec.Name
    $copied = $false

    if ($SourceDir) {
      $sourcePath = Join-Path $SourceDir $spec.Name
      if (Test-Path -LiteralPath $sourcePath) {
        Copy-Item -LiteralPath $sourcePath -Destination $destPath -Force
        $copied = $true
      }
    }

    if (-not $copied) {
      New-PlaceholderAsset -Path $destPath `
        -Width $spec.Width `
        -Height $spec.Height `
        -Label $spec.Label `
        -Title $DisplayTitle `
        -BackgroundHex $BackgroundHex
      $generated.Add($destPath) | Out-Null
    }
  }

  return $generated
}

function ConvertTo-XmlText([string]$Value) {
  if ($null -eq $Value) { return "" }
  return [System.Security.SecurityElement]::Escape($Value)
}

function Write-AppxManifest(
  [string]$Path,
  [string]$Identity,
  [string]$PublisherName,
  [string]$VersionText,
  [string]$ProcessorArchitecture,
  [string]$DisplayTitle,
  [string]$PublisherTitle,
  [string]$DescriptionText,
  [string]$AppId,
  [string]$ExeName,
  [string]$MinOsVersion,
  [string]$MaxOsVersion,
  [string]$BackgroundHex
) {
  $manifest = @"
<?xml version="1.0" encoding="utf-8"?>
<Package
  xmlns="http://schemas.microsoft.com/appx/manifest/foundation/windows10"
  xmlns:uap="http://schemas.microsoft.com/appx/manifest/uap/windows10"
  xmlns:desktop="http://schemas.microsoft.com/appx/manifest/desktop/windows10"
  xmlns:rescap="http://schemas.microsoft.com/appx/manifest/foundation/windows10/restrictedcapabilities"
  IgnorableNamespaces="uap desktop rescap">
  <Identity
    Name="$(ConvertTo-XmlText $Identity)"
    Publisher="$(ConvertTo-XmlText $PublisherName)"
    Version="$VersionText"
    ProcessorArchitecture="$ProcessorArchitecture" />
  <Properties>
    <DisplayName>$(ConvertTo-XmlText $DisplayTitle)</DisplayName>
    <PublisherDisplayName>$(ConvertTo-XmlText $PublisherTitle)</PublisherDisplayName>
    <Description>$(ConvertTo-XmlText $DescriptionText)</Description>
    <Logo>Assets\StoreLogo.png</Logo>
  </Properties>
  <Dependencies>
    <TargetDeviceFamily Name="Windows.Desktop" MinVersion="$MinOsVersion" MaxVersionTested="$MaxOsVersion" />
    <PackageDependency
      Name="Microsoft.VCLibs.140.00.UWPDesktop"
      MinVersion="14.0.30704.0"
      Publisher="CN=Microsoft Corporation, O=Microsoft Corporation, L=Redmond, S=Washington, C=US" />
  </Dependencies>
  <Resources>
    <Resource Language="en-us" />
  </Resources>
  <Applications>
    <Application Id="$(ConvertTo-XmlText $AppId)" Executable="$(ConvertTo-XmlText $ExeName)" EntryPoint="Windows.FullTrustApplication">
      <uap:VisualElements
        DisplayName="$(ConvertTo-XmlText $DisplayTitle)"
        Description="$(ConvertTo-XmlText $DescriptionText)"
        BackgroundColor="$BackgroundHex"
        Square150x150Logo="Assets\Square150x150Logo.png"
        Square44x44Logo="Assets\Square44x44Logo.png">
        <uap:DefaultTile
          Wide310x150Logo="Assets\Wide310x150Logo.png"
          Square71x71Logo="Assets\Square71x71Logo.png"
          Square310x310Logo="Assets\Square310x310Logo.png" />
        <uap:SplashScreen Image="Assets\SplashScreen.png" BackgroundColor="$BackgroundHex" />
      </uap:VisualElements>
      <Extensions>
        <desktop:Extension Category="windows.fullTrustProcess" Executable="$(ConvertTo-XmlText $ExeName)" />
      </Extensions>
    </Application>
  </Applications>
  <Capabilities>
    <Capability Name="internetClient" />
    <Capability Name="privateNetworkClientServer" />
    <rescap:Capability Name="runFullTrust" />
  </Capabilities>
</Package>
"@

  [System.IO.File]::WriteAllText($Path, $manifest, (New-Object System.Text.UTF8Encoding($false)))
}

function New-SymbolPackage([string]$SearchRoot, [string]$DestinationPath) {
  $symbols = Get-ChildItem -LiteralPath $SearchRoot -Filter *.pdb -File -Recurse -ErrorAction SilentlyContinue
  if (-not $symbols -or $symbols.Count -eq 0) {
    return $null
  }

  $tmpDir = Join-Path ([System.IO.Path]::GetTempPath()) ("lan_msixsym_" + [guid]::NewGuid().ToString("N"))
  New-Item -ItemType Directory -Force -Path $tmpDir | Out-Null
  try {
    foreach ($symbol in $symbols) {
      $relative = $symbol.FullName.Substring($SearchRoot.Length).TrimStart('\')
      $target = Join-Path $tmpDir $relative
      $parent = Split-Path -Parent $target
      if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
      Copy-Item -LiteralPath $symbol.FullName -Destination $target -Force
    }

    if (Test-Path -LiteralPath $DestinationPath) { Remove-Item -LiteralPath $DestinationPath -Force }
    $zipPath = [System.IO.Path]::ChangeExtension($DestinationPath, ".zip")
    if (Test-Path -LiteralPath $zipPath) { Remove-Item -LiteralPath $zipPath -Force }
    Compress-Archive -Path (Join-Path $tmpDir "*") -DestinationPath $zipPath -Force
    Move-Item -LiteralPath $zipPath -Destination $DestinationPath -Force
    return $DestinationPath
  } finally {
    Remove-Item -LiteralPath $tmpDir -Recurse -Force -ErrorAction SilentlyContinue
  }
}

function New-UploadBundle([string]$MsixPath, [string]$SymbolsPath, [string]$DestinationPath) {
  $tmpDir = Join-Path ([System.IO.Path]::GetTempPath()) ("lan_msixupload_" + [guid]::NewGuid().ToString("N"))
  New-Item -ItemType Directory -Force -Path $tmpDir | Out-Null
  try {
    Copy-Item -LiteralPath $MsixPath -Destination (Join-Path $tmpDir ([System.IO.Path]::GetFileName($MsixPath))) -Force
    if ($SymbolsPath -and (Test-Path -LiteralPath $SymbolsPath)) {
      Copy-Item -LiteralPath $SymbolsPath -Destination (Join-Path $tmpDir ([System.IO.Path]::GetFileName($SymbolsPath))) -Force
    }

    if (Test-Path -LiteralPath $DestinationPath) { Remove-Item -LiteralPath $DestinationPath -Force }
    $zipPath = [System.IO.Path]::ChangeExtension($DestinationPath, ".zip")
    if (Test-Path -LiteralPath $zipPath) { Remove-Item -LiteralPath $zipPath -Force }
    Compress-Archive -Path (Join-Path $tmpDir "*") -DestinationPath $zipPath -Force
    Move-Item -LiteralPath $zipPath -Destination $DestinationPath -Force
  } finally {
    Remove-Item -LiteralPath $tmpDir -Recurse -Force -ErrorAction SilentlyContinue
  }
}

$repoRoot = Get-RepoRoot
if (-not $OutputRoot) { $OutputRoot = Join-Path $repoRoot "out\package\windows-store" }

$desktopDir = Join-Path $repoRoot ("out\desktop_host\{0}\{1}" -f $Arch, $Config)
$rawVersion = Get-PackageVersion -RepoRoot $repoRoot -Version $Version
$appxVersion = ConvertTo-AppxVersion $rawVersion
$publisherDisplay = if ($PublisherDisplayName) { $PublisherDisplayName } else { $global:LanPublisher }
$sdkTools = Find-WindowsSdkToolSet $Arch
if (-not $MaxVersionTested) { $MaxVersionTested = $sdkTools.Version }

$packageStem = "{0}_{1}_store-{2}" -f (Get-SafeFileComponent $IdentityName), $appxVersion, $Arch
$packageRoot = Join-Path $OutputRoot $packageStem
$stageDir = Join-Path $packageRoot "stage"
$msixPath = Join-Path $packageRoot ($packageStem + ".msix")
$uploadPath = Join-Path $packageRoot ($packageStem + ".msixupload")
$symbolsPath = Join-Path $packageRoot ($packageStem + ".msixsym")
$manifestPath = Join-Path $stageDir "AppxManifest.xml"
$assetsStageDir = Join-Path $stageDir "Assets"
$metadataPath = Join-Path $packageRoot "store_package_manifest.json"
$resolvedAssetsDir = $AssetsDir
if (-not $resolvedAssetsDir) {
  $defaultAssetsDir = Join-Path $repoRoot "src\resources\icons\windows\store"
  if (Test-Path -LiteralPath $defaultAssetsDir) {
    $resolvedAssetsDir = $defaultAssetsDir
  }
}

if (-not $SkipBuild) {
  Write-Section "Build desktop + server"
  & (Join-Path $repoRoot "scripts\build.ps1") -Config $Config -Target all
  if ($LASTEXITCODE -ne 0) { Fail "Build failed while preparing the Store package." }
}

Write-Section "Prepare Store package staging"
if (Test-Path -LiteralPath $packageRoot) { Remove-Item -LiteralPath $packageRoot -Recurse -Force }
New-Item -ItemType Directory -Force -Path $stageDir | Out-Null

Copy-RequiredPayload -DesktopDir $desktopDir -RepoRoot $repoRoot -StageDir $stageDir
$generatedAssets = Ensure-StoreAssets -DestinationDir $assetsStageDir -SourceDir $resolvedAssetsDir -BackgroundHex $BackgroundColor -DisplayTitle $DisplayName
Write-AppxManifest -Path $manifestPath `
  -Identity $IdentityName `
  -PublisherName $Publisher `
  -VersionText $appxVersion `
  -ProcessorArchitecture $Arch `
  -DisplayTitle $DisplayName `
  -PublisherTitle $publisherDisplay `
  -DescriptionText $Description `
  -AppId $ApplicationId `
  -ExeName $Executable `
  -MinOsVersion $MinVersion `
  -MaxOsVersion $MaxVersionTested `
  -BackgroundHex $BackgroundColor

$metadata = [ordered]@{
  identity_name = $IdentityName
  publisher = $Publisher
  publisher_display_name = $publisherDisplay
  display_name = $DisplayName
  description = $Description
  appx_version = $appxVersion
  raw_version = $rawVersion
  arch = $Arch
  config = $Config
  generated_at = (Get-Date).ToString("s")
  desktop_dir = $desktopDir
  stage_dir = $stageDir
  package_path = $msixPath
  upload_path = if ($SkipUploadBundle) { "" } else { $uploadPath }
  assets_dir = if ($resolvedAssetsDir) { $resolvedAssetsDir } else { "" }
  generated_placeholder_assets = @($generatedAssets)
  sign_pfx = $SignPfx
  sdk_version = $sdkTools.Version
  runtime_note = "The current desktop host still writes diagnostics/share bundles under AppDir()\\out, which is read-only inside MSIX installs. Harden writable paths before Store submission."
}
$metadata | ConvertTo-Json -Depth 5 | Out-File -LiteralPath $metadataPath -Encoding utf8

Write-Section "Create MSIX"
Invoke-External -File $sdkTools.MakeAppx -ArgList @("pack", "/d", $stageDir, "/p", $msixPath, "/o") -WorkingDir $repoRoot

if ($SignPfx) {
  Write-Section "Sign MSIX"
  if (-not (Test-Path -LiteralPath $SignPfx)) {
    Fail "SignPfx file not found: $SignPfx"
  }

  $signArgs = @("sign", "/fd", "SHA256", "/f", $SignPfx)
  if ($SignPassword) { $signArgs += @("/p", $SignPassword) }
  if ($TimestampUrl) { $signArgs += @("/tr", $TimestampUrl, "/td", "SHA256") }
  $signArgs += $msixPath
  Invoke-External -File $sdkTools.SignTool -ArgList $signArgs -WorkingDir $repoRoot
} else {
  Write-Host "Skipping signing because -SignPfx was not provided." -ForegroundColor Yellow
}

$symbols = New-SymbolPackage -SearchRoot $desktopDir -DestinationPath $symbolsPath

if (-not $SkipUploadBundle) {
  Write-Section "Create MSIX upload bundle"
  New-UploadBundle -MsixPath $msixPath -SymbolsPath $symbols -DestinationPath $uploadPath
}

Write-Host ""
Write-Host "StageDir:     $stageDir" -ForegroundColor Green
Write-Host "Manifest:     $manifestPath" -ForegroundColor Green
Write-Host "MSIX:         $msixPath" -ForegroundColor Green
if (-not $SkipUploadBundle) {
  Write-Host "MSIXUPLOAD:   $uploadPath" -ForegroundColor Green
}
if ($symbols) {
  Write-Host "MSIXSYM:      $symbols" -ForegroundColor Green
}
if ($generatedAssets.Count -gt 0) {
  Write-Host ""
  Write-Host "Generated placeholder Store assets because no matching PNGs were supplied:" -ForegroundColor Yellow
  $generatedAssets | ForEach-Object { Write-Host "  $_" -ForegroundColor Yellow }
}

Write-Host ""
Write-Host "Store packaging note: current runtime writes under AppDir()\\out, which is not writable inside an installed MSIX. Fix writable-path usage before submitting to Partner Center." -ForegroundColor Yellow
