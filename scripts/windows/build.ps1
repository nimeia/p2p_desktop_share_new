param(
  [ValidateSet("Debug","Release")] [string]$Config = "Debug",
  [ValidateSet("x64")] [string]$Arch = "x64",
  [ValidateSet("all","server","desktop_host")] [string]$Target = "all",
  [ValidateSet("auto","ninja","vs")] [string]$Generator = "auto",
  [ValidateSet("x64-windows","x64-windows-static")] [string]$Triplet = "x64-windows",
  [string]$VcpkgRoot = "auto",
  [switch]$Clean,
  [int]$Port = 9443,
  [int]$MaxViewers = 10,
  [switch]$SkipVcpkgInstall,
  [switch]$SkipServerSmoke,
  [switch]$SkipBrowserSmoke,
  [switch]$SkipDesktopValidation,
  [switch]$SkipDesktopHostRestore,
  [switch]$VerboseCommands,
  [string]$BuildRoot = "auto"
)

. (Join-Path $PSScriptRoot "common.ps1")

function Copy-VcpkgRuntimeDlls(
  [string]$RepoRoot,
  [string]$Triplet,
  [string]$Config,
  [string]$DestinationDir
) {
  if (-not (Test-Path $DestinationDir)) {
    New-Item -ItemType Directory -Force -Path $DestinationDir | Out-Null
  }

  $runtimeBase = Join-Path $RepoRoot ("vcpkg_installed\" + $Triplet)
  $runtimeDir = if ($Config -eq "Debug") {
    Join-Path $runtimeBase "debug\bin"
  } else {
    Join-Path $runtimeBase "bin"
  }

  if (-not (Test-Path $runtimeDir)) {
    Write-Host "Runtime DLL dir not found: $runtimeDir" -ForegroundColor Yellow
    return
  }

  Get-ChildItem -Path $runtimeDir -Filter "*.dll" -File -ErrorAction SilentlyContinue | ForEach-Object {
    Copy-Item -Force $_.FullName (Join-Path $DestinationDir $_.Name)
  }
}

function Assert-PathExists(
  [string]$Path,
  [string]$Description
) {
  if (-not (Test-Path $Path)) {
    Fail "$Description missing: $Path"
  }
}

function Validate-ServerOutputLayout(
  [string]$ServerBinDir
) {
  Assert-PathExists (Join-Path $ServerBinDir "lan_screenshare_server.exe") "server executable"
  Assert-PathExists (Join-Path $ServerBinDir "www\host.html") "host page"
  Assert-PathExists (Join-Path $ServerBinDir "www\viewer.html") "viewer page"
  Assert-PathExists (Join-Path $ServerBinDir "webui\index.html") "admin shell index"
  Assert-PathExists (Join-Path $ServerBinDir "webui\app.js") "admin shell app"
  Assert-PathExists (Join-Path $ServerBinDir "webui\style.css") "admin shell style"
}

$root = Get-RepoRoot
$outDir = Join-Path $root "out"
$serverOutDir = Join-Path $outDir "server\$Config"
$desktopOutDir = Join-Path $outDir "desktop_host\$Arch\$Config"

$needServer = ($Target -eq "server" -or $Target -eq "all")
$needDesktopHost = ($Target -eq "desktop_host" -or $Target -eq "all")

# Resolve generator early (auto -> vs/ninja)
if ($Generator -eq "auto") {
  # Prefer Visual Studio generator on Windows for the most reliable MSVC toolchain
  $msbuild = Find-MSBuild
  if ($msbuild) {
    $Generator = "vs"
  } elseif (Get-Command ninja -ErrorAction SilentlyContinue) {
    $Generator = "ninja"
  } else {
    $Generator = "vs"
  }
}


# Compute build directory.
# If the repo path is long, prefer a short build root to avoid MSBuild/CMake TryCompile path-length issues.
# IMPORTANT: Build roots must be unique per source directory. Otherwise, if you unzip a new source tree
# into a different folder and re-run the build script without cleaning, CMake will refuse to reuse the
# existing cache with an error like:
#   "The source ... does not match the source ... used to generate cache"
$defaultBuildDir = Join-Path $outDir ("build\windows-" + $Generator + "-" + $Triplet + "-" + $Config)
if ($BuildRoot -eq "auto") {
  if ($defaultBuildDir.Length -gt 200 -or $root.Length -gt 80) {
    # Use LocalAppData rather than TEMP to avoid MSBuild warnings about Temp output directories.
    # Include the repo folder name to keep the build cache unique per source tree.
    $repoLeaf = Split-Path $root -Leaf
    $BuildRoot = Join-Path $env:LOCALAPPDATA ("lan_screenshare_build\\" + $repoLeaf)
  } else {
    $BuildRoot = $outDir
  }
}

if ($BuildRoot -eq $outDir) {
  $buildDir = $defaultBuildDir
} else {
  New-Item -ItemType Directory -Force -Path $BuildRoot | Out-Null
  $buildDir = Join-Path $BuildRoot ("windows-" + $Generator + "-" + $Triplet + "-" + $Config)
}

# Build log file (captured stdout/stderr of external commands)
$logsDir = Join-Path $outDir "logs"
$ts = Get-Date -Format "yyyyMMdd_HHmmss"
$logFile = Join-Path $logsDir ("build_windows_{0}_{1}_{2}_{3}.log" -f $Generator,$Triplet,$Config,$ts)
Write-Host ("LogFile: " + $logFile) -ForegroundColor DarkGray

Write-Section "Check toolchain"

if ($needServer) {
  Ensure-Command cmake "Install CMake >= 3.24 and ensure it's in PATH."
  if ($Generator -eq "ninja") { Ensure-Command ninja "Install Ninja and ensure it's in PATH." }
}

if ($needDesktopHost) {
  $msbuild = Find-MSBuild
  if (-not $msbuild) { Fail "MSBuild not found. Install Visual Studio 2022 (Desktop development with C++) or Build Tools." }
  Write-Host "MSBuild: $msbuild"
}

# Clean build directory after resolution
if ($Clean -and (Test-Path $buildDir)) { Remove-Item -Recurse -Force $buildDir }


if ($needServer) {
  Write-Section "Resolve vcpkg"
  $vcpkgRootResolved = Find-Vcpkg $VcpkgRoot
  if (-not $vcpkgRootResolved) {
    Fail "vcpkg not found. Set -VcpkgRoot <path> or set VCPKG_ROOT env var."
  }
  $env:VCPKG_ROOT = $vcpkgRootResolved
  $vcpkgExe = Join-Path $vcpkgRootResolved "vcpkg.exe"
  $toolchain = Join-Path $vcpkgRootResolved "scripts\buildsystems\vcpkg.cmake"
  if (-not (Test-Path $toolchain)) { Fail "vcpkg toolchain not found: $toolchain" }

  if (-not $SkipVcpkgInstall) {
    Write-Section "vcpkg install (manifest)"
    Invoke-External $vcpkgExe @("install", "--triplet", $Triplet) -WorkingDir $root -Echo:$VerboseCommands -LogFile $logFile
  }

  $legacyServerBinDir = Join-Path $outDir "bin"
  $legacyServerWwwDir = Join-Path $outDir "www"
  if (Test-Path $legacyServerBinDir) { Remove-Item -Recurse -Force $legacyServerBinDir -ErrorAction SilentlyContinue }
  if (Test-Path $legacyServerWwwDir) { Remove-Item -Recurse -Force $legacyServerWwwDir -ErrorAction SilentlyContinue }

  Write-Section "Configure CMake"
  New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
  $cmakeArgs = @("-S", $root, "-B", $buildDir, "-Wno-dev", "-DCMAKE_TOOLCHAIN_FILE=$toolchain", "-DVCPKG_TARGET_TRIPLET=$Triplet")
  if ($Generator -eq "ninja") {
    $cmakeArgs += @("-G", "Ninja", "-DCMAKE_BUILD_TYPE=$Config")
  } else {
    # Visual Studio generator (multi-config)
    $cmakeArgs += @("-G", "Visual Studio 17 2022", "-A", $Arch)
  }
  Write-Host "CMake Arguments:" -ForegroundColor Yellow
  $cmakeArgs | ForEach-Object { Write-Host "  $_" }
  Write-Host "Running: cmake $($cmakeArgs -join ' ')" -ForegroundColor Yellow
  Write-Host "(This may take 1-2 minutes on first run...)" -ForegroundColor Gray
  $startTime = Get-Date
  Invoke-External "cmake" $cmakeArgs -Echo:$VerboseCommands -LogFile $logFile
  $duration = (Get-Date) - $startTime
  Write-Host "CMake completed in $($duration.TotalSeconds) seconds" -ForegroundColor Green
}

if ($needServer) {
  Write-Section "Build C++ server"
  $buildArgs = @("--build", $buildDir, "--target", "lan_screenshare_server")
  if ($Generator -ne "ninja") { $buildArgs += @("--config", $Config) }
try {
    Invoke-External "cmake" $buildArgs -Echo:$VerboseCommands -LogFile $logFile 
  } catch {
    Write-Host ""
    Write-Host "Build failed. Retrying with verbose output..." -ForegroundColor Yellow
    $buildArgsVerbose = $buildArgs + @("--verbose")
    try { Invoke-External "cmake" $buildArgsVerbose -Echo:$true -LogFile $logFile } catch {}

    $ninjaLog = Join-Path $buildDir ".ninja_log"
    if (Test-Path $ninjaLog) {
      Write-Host ""
      Write-Host "=== .ninja_log (tail 50) ===" -ForegroundColor Yellow
      Get-Content $ninjaLog -Tail 50 | ForEach-Object { Write-Host $_ }
    }

    $cmakeErr = Join-Path $buildDir "CMakeFiles\CMakeError.log"
    $cmakeOut = Join-Path $buildDir "CMakeFiles\CMakeOutput.log"
    if (Test-Path $cmakeErr) {
      Write-Host ""
      Write-Host "=== CMakeError.log (tail 200) ===" -ForegroundColor Yellow
      Get-Content $cmakeErr -Tail 200 | ForEach-Object { Write-Host $_ }
    }
    if (Test-Path $cmakeOut) {
      Write-Host ""
      Write-Host "=== CMakeOutput.log (tail 50) ===" -ForegroundColor Yellow
      Get-Content $cmakeOut -Tail 50 | ForEach-Object { Write-Host $_ }
    }
    throw
  }
# Locate built exe (generator output paths vary; search recursively)
# NOTE: Do NOT pass FileInfo objects directly into Copy-Item. PowerShell may coerce them via .ToString(),
# which can become a relative path (e.g. "lan_screenshare_server.exe") and resolve under $PWD, causing
# confusing "file not found" errors even though the build succeeded.
$serverExeItem = Get-ChildItem -Path $buildDir -Recurse -Filter "lan_screenshare_server.exe" -File -ErrorAction SilentlyContinue |
  Sort-Object -Property LastWriteTime -Descending |
  Select-Object -First 1

if (-not $serverExeItem) {
  Write-Section "Diagnostics: list build outputs"
  if (Test-Path $buildDir) {
    Write-Host "BuildDir: $buildDir"
    Write-Host "CMakeCache: " (Test-Path (Join-Path $buildDir "CMakeCache.txt"))
    Write-Host "Files (top 200):"
    Get-ChildItem -Path $buildDir -Recurse -File -ErrorAction SilentlyContinue |
      Sort-Object -Property LastWriteTime -Descending |
      Select-Object -First 200 |
      ForEach-Object { Write-Host ("  " + $_.FullName) }
    Write-Host "Try: cmake --build \"$buildDir\" --target lan_screenshare_server --verbose"
  }
  Fail "Built server exe not found under: $buildDir. Run with -VerboseCommands and paste the printed cmake commands + any cmake output."
}

$serverExe = $serverExeItem.FullName

$binDir = $serverOutDir
New-Item -ItemType Directory -Force -Path $binDir | Out-Null
Copy-Item -Force $serverExe (Join-Path $binDir "lan_screenshare_server.exe")
Copy-VcpkgRuntimeDlls -RepoRoot $root -Triplet $Triplet -Config $Config -DestinationDir $binDir

# Copy PDB if present (useful for debugging)
$pdbCandidate = [System.IO.Path]::ChangeExtension($serverExe, ".pdb")
if (Test-Path $pdbCandidate) {
  Copy-Item -Force $pdbCandidate (Join-Path $binDir "lan_screenshare_server.pdb")
}
  # Copy www into the server runtime bundle
  $wwwOut = Join-Path $binDir "www"
  if (Test-Path $wwwOut) { Remove-Item -Recurse -Force $wwwOut }
  Copy-Item -Recurse -Force (Join-Path $root "www") $wwwOut

  # Copy admin webui into the server runtime bundle
  $adminOut = Join-Path $binDir "webui"
  if (Test-Path $adminOut) { Remove-Item -Recurse -Force $adminOut }
  Copy-Item -Recurse -Force (Join-Path $root "src\desktop_host\webui") $adminOut

  Write-Section "Validate server output"
  Validate-ServerOutputLayout -ServerBinDir $binDir

  if (-not $SkipServerSmoke) {
    Write-Section "Smoke test server output"
    & (Join-Path $root "scripts\windows\smoke_server.ps1") -Config $Config
  }

  if (-not $SkipBrowserSmoke) {
    Write-Section "Browser smoke validation"
    & (Join-Path $root "scripts\windows\browser_smoke.ps1") -Config $Config -Triplet $Triplet -Generator $Generator -BuildDir $buildDir
  }
}

if ($Target -eq "desktop_host" -or $Target -eq "all") {
  Write-Section "Build desktop host app"

  $sln = Join-Path $root "src\desktop_host\LanScreenShareHostApp.sln"
  if (-not (Test-Path $sln)) { Fail "Desktop host solution not found: $sln" }

  $projDir = Join-Path $root "src\desktop_host"
  $legacyObjDir = Join-Path $projDir "obj"
  $legacyBinDir = Join-Path (Split-Path $sln -Parent) "bin"
  $desktopObjDir = Join-Path $outDir "obj\desktop_host"
  if ($Clean) {
    Write-Host "Cleaning build artifacts..." -ForegroundColor Gray
    if (Test-Path $legacyObjDir) { Remove-Item -Path $legacyObjDir -Recurse -Force -ErrorAction SilentlyContinue }
    if (Test-Path $legacyBinDir) { Remove-Item -Path $legacyBinDir -Recurse -Force -ErrorAction SilentlyContinue }
    if (Test-Path $desktopObjDir) { Remove-Item -Path $desktopObjDir -Recurse -Force -ErrorAction SilentlyContinue }
    if (Test-Path $desktopOutDir) { Remove-Item -Path $desktopOutDir -Recurse -Force -ErrorAction SilentlyContinue }
    Write-Host "Clean completed." -ForegroundColor Gray
  } else {
    Write-Host "Preserving desktop host intermediates for incremental build." -ForegroundColor Gray
  }

  $msbuild = Find-MSBuild
  if (-not $msbuild) { Fail "MSBuild not found. Install Visual Studio 2022 (Desktop development with C++) or Build Tools." }

  # Restore + build (NuGet packages: Microsoft.WindowsAppSDK, Microsoft.Windows.CppWinRT)
  # IMPORTANT: pass the SLN path WITHOUT pre-quoting.
  # Invoke-External will quote args when needed. Pre-quoting turns into a literal \"...\" and MSBuild
  # will treat it as part of the filename (MSB1009: project file does not exist).
  $msbArgs = @(
    $sln,
    "/m",
    "/p:RestoreConfigFile=$root\\NuGet.Config",
    "/p:RestorePackagesConfig=true",
    "/p:Configuration=$Config",
    "/p:Platform=$Arch"
  )
  if (-not $SkipDesktopHostRestore) { $msbArgs = @($sln, "/restore") + $msbArgs[1..($msbArgs.Count-1)] }
  Write-Host ("NuGet package cache: " + (Join-Path $root "third_party\\nuget")) -ForegroundColor DarkGray
  if (-not $SkipDesktopHostRestore) {
    Write-Host "MSBuild restore is enabled; cached packages are reused unless they are missing." -ForegroundColor DarkGray
  }
  Write-Host "MSBuild output is captured and may appear only after the command exits." -ForegroundColor DarkGray
  Invoke-External $msbuild $msbArgs -Echo:$VerboseCommands -LogFile $logFile

  # Output dir is defined in the vcxproj as: out/desktop_host/<Platform>/<Config>/
  $winOut = $desktopOutDir
  if (-not (Test-Path $winOut)) {
    Write-Host "Desktop host output dir not found: $winOut" -ForegroundColor Yellow
  } else {
    Copy-VcpkgRuntimeDlls -RepoRoot $root -Triplet $Triplet -Config $Config -DestinationDir $winOut

    # Copy server + www next to the desktop host exe (best-effort)
    $serverBuilt = Join-Path $serverOutDir "lan_screenshare_server.exe"
    if (Test-Path $serverBuilt) { Copy-Item -Force $serverBuilt (Join-Path $winOut "lan_screenshare_server.exe") }

    $serverToolsBuilt = Join-Path $serverOutDir "tools"
    if (Test-Path $serverToolsBuilt) {
      $toolsOut = Join-Path $winOut "tools"
      if (Test-Path $toolsOut) { Remove-Item -Recurse -Force $toolsOut }
      Copy-Item -Recurse -Force $serverToolsBuilt $toolsOut
    }

    $wwwBuilt = Join-Path $serverOutDir "www"
    if (Test-Path $wwwBuilt) {
      $wwwOut = Join-Path $winOut "www"
      if (Test-Path $wwwOut) { Remove-Item -Recurse -Force $wwwOut }
      Copy-Item -Recurse -Force $wwwBuilt $wwwOut
    }

    $webUiBuilt = Join-Path $root "src\desktop_host\webui"
    if (Test-Path $webUiBuilt) {
      $webUiOut = Join-Path $winOut "webui"
      if (Test-Path $webUiOut) { Remove-Item -Recurse -Force $webUiOut }
      Copy-Item -Recurse -Force $webUiBuilt $webUiOut
    }
    if (-not $SkipDesktopValidation) {
      Write-Section "Desktop release validation"
      & (Join-Path $root "scripts\windows\validate_release.ps1") -Config $Config -Arch $Arch -DesktopDir $winOut
    }
  }
}

Write-Section "Done"
Write-Host "BuildDir: $buildDir"
Write-Host "OutDir:   $outDir"
Write-Host "LogFile:  $logFile"
if ($needServer) {
  $serverExeOut = Join-Path $serverOutDir "lan_screenshare_server.exe"
  Write-Host "ServerDir: $serverOutDir"
  Write-Host "ServerExe: $serverExeOut"
}
if ($needDesktopHost) {
  $desktopExeOut = Join-Path $desktopOutDir "LanScreenShareHostApp.exe"
  Write-Host "DesktopDir: $desktopOutDir"
  Write-Host "DesktopExe: $desktopExeOut"
}
