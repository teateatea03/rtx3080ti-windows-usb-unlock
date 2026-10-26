param([string]$Clang='clang',[string]$Python='python',[string]$VcVars='',
 [switch]$DynamicEntrySs0, [string]$Prom='', [string]$Assets=(Join-Path $PSScriptRoot '.local/firmware/assets'))
$ErrorActionPreference='Stop'
& (Join-Path $PSScriptRoot 'src/Test-Pny-Regression.ps1') -Clang $Clang -Python $Python -VcVars $VcVars
& $Python (Join-Path $PSScriptRoot 'tools/validate_public.py')
if($LASTEXITCODE -ne 0){throw 'Public file verification failed'}
if($Prom){
 & $Python (Join-Path $PSScriptRoot 'tools/config_fixture.py')
 if($LASTEXITCODE -ne 0){throw 'Config fixture failed'}
 & (Join-Path $PSScriptRoot 'src/Test-Pny.ps1') -Clang $Clang -Python $Python -VcVars $VcVars -Prom $Prom -Config (Join-Path $PSScriptRoot '.local/config256.bin') -Assets $Assets -DynamicEntrySs0:$DynamicEntrySs0
}else{'SKIP: 126 static / 225 dynamic fixture scenarios and diagnostic-mode fixture test require the exact private PROM (-Prom).'}
