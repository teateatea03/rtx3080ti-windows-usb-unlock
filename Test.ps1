param([string]$Clang='clang',[string]$Python='python',[string]$VcVars='',
 [string]$Prom='', [string]$Assets=(Join-Path $PSScriptRoot '.local/firmware/assets'))
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'src/Test-Pny-Regression.ps1') -Clang $Clang -Python $Python -VcVars $VcVars
& $Python (Join-Path $PSScriptRoot 'tools/validate_public.py')
if($LASTEXITCODE -ne 0){throw 'Public file verification failed'}
if($Prom){
 & $Python (Join-Path $PSScriptRoot 'tools/config_fixture.py')
 if($LASTEXITCODE -ne 0){throw 'Config fixture failed'}
 & (Join-Path $PSScriptRoot 'src/Test-Pny.ps1') -Clang $Clang -Python $Python -VcVars $VcVars -Prom $Prom -Config (Join-Path $PSScriptRoot '.local/config256.bin') -Assets $Assets
}else{'SKIP: 119 fixture scenarios and diagnostic-mode fixture test require the exact private PROM (-Prom).'}
