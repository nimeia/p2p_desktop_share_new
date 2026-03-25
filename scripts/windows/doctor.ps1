param(
  [ValidateSet("Debug","Release")] [string]$Config = "Debug",
  [ValidateSet("auto","ninja","vs")] [string]$Generator = "auto",
  [ValidateSet("x64-windows","x64-windows-static")] [string]$Triplet = "x64-windows",
  [string]$VcpkgRoot = "auto"
)

. (Join-Path $PSScriptRoot "common.ps1")

$root = Get-RepoRoot

Write-Section "Environment"
Write-Host "PowerShell: $($PSVersionTable.PSVersion)"
Write-Host "OS: $([System.Environment]::OSVersion.VersionString)"
Write-Host "CWD: $(Get-Location)"

Write-Section "Tooling"
Ensure-Command cmake "Install CMake >= 3.24 and ensure it's in PATH."
Write-Host "cmake: " (Get-Command cmake).Source
if (Get-Command ninja -ErrorAction SilentlyContinue) { Write-Host "ninja: " (Get-Command ninja).Source } else { Write-Host "ninja: (not found)" }
$msbuild = Find-MSBuild
if ($msbuild) { Write-Host "MSBuild: $msbuild" } else { Write-Host "MSBuild: (not found)" }

Write-Section "vcpkg"
$vcpkgRootResolved = Find-Vcpkg $VcpkgRoot
if (-not $vcpkgRootResolved) { Fail "vcpkg not found. Set -VcpkgRoot or VCPKG_ROOT." }
Write-Host "VCPKG_ROOT: $vcpkgRootResolved"
Write-Host "vcpkg.exe:  " (Join-Path $vcpkgRootResolved "vcpkg.exe")
Write-Host "toolchain:  " (Join-Path $vcpkgRootResolved "scripts\buildsystems\vcpkg.cmake")

Write-Section "Build directory"
$Generator = Resolve-CmakeGenerator $Generator
$buildDir = Get-DefaultWindowsBuildDir $root $Generator $Triplet $Config
Write-Host "Expected build dir: $buildDir"
Write-Host "Exists: " (Test-Path $buildDir)
if (Test-Path $buildDir) {
  Write-Host "CMakeCache exists: " (Test-Path (Join-Path $buildDir "CMakeCache.txt"))
  Write-Host "Top 50 files:"
  Get-ChildItem -Path $buildDir -Recurse -File -ErrorAction SilentlyContinue |
    Sort-Object -Property LastWriteTime -Descending |
    Select-Object -First 50 |
    ForEach-Object { Write-Host ("  " + $_.FullName) }
}

Write-Section "Hints"
Write-Host "If you keep seeing CMake Usage output, run:"
Write-Host "  .\scripts\build.ps1 -Target server -VerboseCommands -Clean"
Write-Host "And paste the printed 'cmake ...' lines + any cmake output."
