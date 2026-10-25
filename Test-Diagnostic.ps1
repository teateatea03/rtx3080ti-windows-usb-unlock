param([string]$VcVars='')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'tools/Host-Environment.ps1')
Import-VcEnvironment $VcVars
$msvc=$env:VCToolsInstallDir.TrimEnd('\')
$sdk=$env:WindowsSdkDir.TrimEnd('\')
$version=$env:WindowsSDKVersion.TrimEnd('\')
& (Join-Path $PSScriptRoot 'diagnostics/v4/Build.ps1') -Msvc $msvc
& (Join-Path $PSScriptRoot 'diagnostics/v4/Test.ps1') -Msvc $msvc -Sdk $sdk -SdkVersion $version
& (Join-Path $PSScriptRoot 'diagnostics/v4/Test-Report.ps1')
