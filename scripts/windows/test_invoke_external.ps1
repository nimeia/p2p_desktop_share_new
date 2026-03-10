param()
. (Join-Path $PSScriptRoot "common.ps1")
Write-Section "Test Invoke-External"
Invoke-External "cmake" @("--version") -Echo:$true | Out-Null
Write-Host "OK"
