param(
  [ValidateSet("Debug","Release")] [string]$Config = "Debug",
  [ValidateSet("all","server","desktop_host")] [string]$Target = "all",
  [switch]$Clean,
  [string]$VcpkgRoot = "auto",
  [ValidateSet("auto","ninja","vs")] [string]$Generator = "auto",
  [ValidateSet("x64-windows","x64-windows-static")] [string]$Triplet = "x64-windows",
  [int]$Port = 9443,
  [int]$MaxViewers = 10,
  [switch]$SkipVcpkgInstall,
  [switch]$VerboseCommands,
  [string]$BuildRoot = "auto"
)

& (Join-Path $PSScriptRoot "windows\build.ps1") -Config $Config -Target $Target -Clean:$Clean -VcpkgRoot $VcpkgRoot -Generator $Generator -Triplet $Triplet -Port $Port -MaxViewers $MaxViewers -SkipVcpkgInstall:$SkipVcpkgInstall -VerboseCommands:$VerboseCommands -BuildRoot:$BuildRoot
