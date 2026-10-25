param([string]$Msvc='C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207',
      [string]$Sdk='C:\Program Files (x86)\Windows Kits\10',[string]$SdkVersion='10.0.26100.0')
$ErrorActionPreference='Stop'
$headers=Join-Path $PSScriptRoot 'vendor/edk2/MdePkg/Include'
$output=Join-Path $PSScriptRoot 'build'
$compiler=Join-Path $Msvc 'bin/Hostx64/x64/cl.exe'
$linker=Join-Path $Msvc 'bin/Hostx64/x64/link.exe'
$object=Join-Path $output 'host.obj'
$exe=Join-Path $output 'host-tests.exe'
& $compiler /nologo /TC /std:c11 /W4 /WX /MT /c "/I$headers" "/I$headers/X64" "/I$Msvc/include" "/I$Sdk/Include/$SdkVersion/ucrt" "/Fo$object" (Join-Path $PSScriptRoot 'tests/host.c')
if ($LASTEXITCODE -ne 0) {throw 'Host fixture compile failed'}
& $linker /nologo "/out:$exe" $object "/libpath:$Msvc/lib/x64" "/libpath:$Sdk/Lib/$SdkVersion/ucrt/x64" "/libpath:$Sdk/Lib/$SdkVersion/um/x64"
if ($LASTEXITCODE -ne 0) {throw 'Host fixture link failed'}
$testOutput=& $exe
if ($LASTEXITCODE -ne 0) {throw 'Host scenario failed'}
$testOutput | Tee-Object -FilePath (Join-Path $output 'host-test-results.txt')
$efi=Join-Path $output 'GPU-DIAG.EFI'
$bytes=[IO.File]::ReadAllBytes($efi)
$pe=[BitConverter]::ToInt32($bytes,0x3c)
if ([BitConverter]::ToUInt32($bytes,$pe) -ne 0x4550) {throw 'Bad PE signature'}
if ([BitConverter]::ToUInt16($bytes,$pe+4) -ne 0x8664) {throw 'Not x64'}
$optional=$pe+24
if ([BitConverter]::ToUInt16($bytes,$optional) -ne 0x20b) {throw 'Not PE32+'}
if ([BitConverter]::ToUInt16($bytes,$optional+68) -ne 10) {throw 'Not EFI application'}
$entry=[BitConverter]::ToUInt32($bytes,$optional+16)
$sections=[BitConverter]::ToUInt16($bytes,$pe+6)
$sectionStart=$optional+[BitConverter]::ToUInt16($bytes,$pe+20)
$entryExecutable=$false
for ($i=0;$i -lt $sections;$i++) {
    $s=$sectionStart+40*$i
    $va=[BitConverter]::ToUInt32($bytes,$s+12)
    $size=[BitConverter]::ToUInt32($bytes,$s+8)
    $flags=[BitConverter]::ToUInt32($bytes,$s+36)
    if (($flags -band 0x20000000) -and ($flags -band 0x80000000)) {throw 'Writable executable section'}
    if ($entry -ge $va -and $entry -lt $va+$size -and ($flags -band 0x20000000)) {$entryExecutable=$true}
}
if (!$entryExecutable) {throw 'Entry point outside executable section'}
$directories=$optional+112
if ([BitConverter]::ToUInt32($bytes,$directories+8) -ne 0) {throw 'Unexpected runtime imports'}
if ([BitConverter]::ToUInt32($bytes,$directories+40) -eq 0) {throw 'Missing base relocation directory'}
$source=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'src/diagnostic.c') -Raw
if ($source -match '->(?:Pci|Mem|Io)\.Write\s*\(|->(?:SetBarAttributes|Attributes|Map|Unmap|CopyMem|ResetSystem|SetVariable|DisconnectController|ConnectController|StartImage|LoadImage|SetMode|Blt)\s*\(') {throw 'Disallowed hardware or boot call'}
if ($source -match '->Mem\.Read\s*\(|->RomImage|->RomSize') {throw 'Unexpected MMIO or ROM access'}
if ((Get-FileHash $efi).Hash -ne (Get-FileHash (Join-Path $PSScriptRoot 'bundle/EFI/BOOT/BOOTX64.EFI')).Hash) {throw 'Bundle differs from tested EFI'}
[ordered]@{Pe32Plus=$true;Machine='x64';Subsystem='EFI_APPLICATION';Imports=0;Relocations=$true;WriteExecuteSections=0;HostScenarios=13;HardwareTested=$false} |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output 'verification.json') -Encoding UTF8
Write-Output 'PE checks and hardware/boot call audit passed. Real UEFI execution remains unverified.'
