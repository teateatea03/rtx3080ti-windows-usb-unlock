param(
 [string]$Clang='clang',
 [string]$LldLink='lld-link',
 [string]$Python='python',
 [switch]$Experimental
)
$ErrorActionPreference='Stop'
if((& $Clang --version|Select-Object -First 1) -notmatch '20\.1\.8'){throw 'Require reviewed LLVM 20.1.8'}
if((& $LldLink --version) -notmatch '20\.1\.8'){throw 'Require reviewed LLD 20.1.8'}
$mode=if($Experimental){'experimental-preserve-ss1'}else{'diagnostic-only'}
$out=Join-Path $PSScriptRoot ('build/pny3080ti/'+$mode)
New-Item -ItemType Directory -Path $out -Force | Out-Null
$sources=@('app','app_io','ownership','format','runtime','sha256','checks','port','compat','vendor/unlock_610','vendor/prep_610','vendor/layout_610','vendor/vbios_610','vendor/gfw_eval')
$defines=@('-D__KERNEL__','-DGU_UEFI_PORT','-DGU_PROFILE_PNY_B550_3080TI','-DGU_TARGET_PCI_ID=0x220810deU')
if($Experimental){$defines+=@('-DGU_EXPERIMENTAL_3090','-DGU_ENTRY_LATCHED_SS1_PRESERVE')}
Push-Location $PSScriptRoot
try {
 $objects=@()
 foreach($source in $sources){
  $object=Join-Path $out (($source -replace '/','_')+'.obj')
  & $Clang -target x86_64-pc-windows-msvc -std=c11 -ffreestanding -fno-builtin -fno-stack-protector -mno-red-zone -fshort-wchar -Os -Wall -Wextra -Werror -Wno-unused-parameter @defines -Icompat -I../deps/edk2/MdePkg/Include -I../deps/edk2/MdePkg/Include/X64 -c "$source.c" -o $object
  if($LASTEXITCODE -ne 0){throw "Compile failed: $source"}
  $objects+=$object
 }
 $efi=Join-Path $out 'PNY3080TI.EFI';$map=Join-Path $out 'PNY3080TI.map'
 & $LldLink /nologo /nodefaultlib /subsystem:efi_application /entry:efi_main /machine:x64 /base:0 /dynamicbase /nxcompat /timestamp:0 /opt:ref /opt:icf "/map:$map" "/out:$efi" @objects
 if($LASTEXITCODE -ne 0){throw 'Link failed'}
 & $Python verify_pe.py $efi $map --output (Join-Path $out 'pe-verification.json')
 if($LASTEXITCODE -ne 0){throw 'PE validation failed'}
 [ordered]@{UpstreamRevision='e3f03cd9389195adcabda4743d8fc2922b32498c';Mode=$mode;HardwareUnlockTested=$false;
 Target='PNY 10de:2208 / 196e:1384, 0000:04:00.0';MemoryMiB=12288;Ss0EntryObserved=@('24321116','24321156');FwsecFuseAddress='8241e0';FwsecFuseRaw=3;FwsecFuseVersion=2;FwsecSignatureOffset=768;Ss1Entry=7;Ss0Target='88888888';Ss1Policy='preserve';
 InitialCommand=0;TemporaryMemoryDecode=[bool]$Experimental;SeparateDisplay='RTX5080 19da:1762 bus 8';DriverDetachAllowed=$false;AutomaticBoot=$false;
 Confirmation='U once; Esc/timeout cancels';EfiSha256=(Get-FileHash -LiteralPath $efi).Hash;
 Sources=@(Get-ChildItem -LiteralPath $PSScriptRoot -File -Recurse|Where-Object {$_.Extension -in @('.c','.h') -and $_.FullName -notlike '*\build\*'}|ForEach-Object {[ordered]@{Name=$_.FullName.Substring($PSScriptRoot.Length+1);Sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}});
 Defines=$defines
 }|ConvertTo-Json -Depth 6|Set-Content -LiteralPath (Join-Path $out 'build-manifest.json') -Encoding UTF8
 "Built $efi; compare its hash with docs/RESULTS.md. This invocation performed no hardware test."
}finally{Pop-Location}
