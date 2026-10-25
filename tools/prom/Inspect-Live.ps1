param([Parameter(Mandatory=$true)][string]$Prom,[string]$Clang='clang',[string]$VcVars='')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../Host-Environment.ps1')
Import-VcEnvironment $VcVars
$root=(Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$out=Join-Path $root '.local/prom-inspect.exe'
New-Item -ItemType Directory -Path (Split-Path $out) -Force|Out-Null
& $Clang -std=c11 -O1 -Werror -D_CRT_SECURE_NO_WARNINGS -DFW_FUSE_VERSION=2 "-I$root/src/compat" "-I$root/src/vendor" (Join-Path $PSScriptRoot 'inspect-live.c') "$root/src/vendor/vbios_610.c" -o $out
if($LASTEXITCODE -ne 0){throw 'Compile failed'}
& $out $Prom
if($LASTEXITCODE -ne 0){throw 'PROM inspection failed'}
