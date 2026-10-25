param([string]$Clang='clang',[string]$LldLink='lld-link',[string]$Python='python',[switch]$Experimental)
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'src/Build-Pny.ps1') -Clang $Clang -LldLink $LldLink -Python $Python -Experimental:$Experimental
