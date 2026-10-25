param([switch]$DynamicEntrySs0)
$ErrorActionPreference='Stop'
$repo=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$scratch=Join-Path $repo ('.local/copier-test-'+[Guid]::NewGuid().ToString('N'))
$source=Join-Path $scratch 'bundle'
$drive=Join-Path $scratch 'usb'
New-Item -ItemType Directory -Path (Join-Path $source 'EFI/BOOT'),(Join-Path $source 'PNY3080TI-UNLOCK'),(Join-Path $drive 'EFI/BOOT') -Force|Out-Null
$mode=if($DynamicEntrySs0){'experimental-dynamic-ss0-preserve-ss1'}else{'experimental-preserve-ss1'}
$efi=Join-Path $repo ('src/build/pny3080ti/'+$mode+'/PNY3080TI.EFI')
Copy-Item -LiteralPath $efi -Destination (Join-Path $source 'EFI/BOOT/BOOTX64.EFI')
Set-Content -LiteralPath (Join-Path $source 'PNY3080TI-UNLOCK/test.txt') -Value 'fixture'
Set-Content -LiteralPath (Join-Path $drive 'EFI/BOOT/BOOTX64.EFI') -Value 'prior boot'
$prior=(Get-FileHash (Join-Path $drive 'EFI/BOOT/BOOTX64.EFI')).Hash
$manifest=@(Get-ChildItem -LiteralPath $source -Recurse -File|ForEach-Object {
 '{0}  {1}' -f (Get-FileHash $_.FullName).Hash.ToLowerInvariant(),$_.FullName.Substring($source.Length+1).Replace('\','/')
})
$manifest|Set-Content -LiteralPath (Join-Path $source 'SHA256SUMS.txt') -Encoding ASCII
$letter=@('U','V','W','Y','Z')|Where-Object {!(Get-PSDrive $_ -ErrorAction SilentlyContinue)}|Select-Object -First 1
if(!$letter){throw 'No free mock drive name'}
# Utility modules may resolve drives in global scope on CI; remove it in finally.
New-PSDrive -Name $letter -PSProvider FileSystem -Root $drive -Scope Global|Out-Null
$env:GPU_UNLOCK_TEST_BUS='USB'
function Get-Partition {param($DriveLetter) [pscustomobject]@{DiskNumber=99;IsBoot=$false;IsSystem=$false}}
function Get-Disk {param($Number) [pscustomobject]@{Number=99;BusType=$env:GPU_UNLOCK_TEST_BUS;IsBoot=$false;IsSystem=$false;FriendlyName='MOCK FILESYSTEM ONLY'}}
function Get-Volume {param($DriveLetter) [pscustomobject]@{FileSystem='FAT32'}}
function Reject([scriptblock]$Action){
 $rejected=$false
 try{& $Action|Out-Null}catch{$rejected=$true}
 if(!$rejected){throw 'Expected rejection'}
}
try{
 & "$PSScriptRoot/Copy-Usb.ps1" -DriveLetter $letter -Bundle $source -DynamicEntrySs0:$DynamicEntrySs0|Out-Null
 if((Get-FileHash (Join-Path $drive 'EFI/BOOT/BOOTX64.EFI')).Hash -ne $prior -or (Test-Path (Join-Path $drive 'PNY3080TI-UNLOCK'))){throw 'Dry run mutated files'}
 Reject {& "$PSScriptRoot/Copy-Usb.ps1" -DriveLetter $letter -Bundle $source -DynamicEntrySs0:(!$DynamicEntrySs0) -Apply}
 $env:GPU_UNLOCK_TEST_BUS='NVMe'
 Reject {& "$PSScriptRoot/Copy-Usb.ps1" -DriveLetter $letter -Bundle $source -DynamicEntrySs0:$DynamicEntrySs0 -Apply}
 $env:GPU_UNLOCK_TEST_BUS='USB'
 ('0'*64+'  PNY3080TI-UNLOCK/../EFI/BOOT/BOOTX64.EFI')|Set-Content (Join-Path $source 'SHA256SUMS.txt')
 Reject {& "$PSScriptRoot/Copy-Usb.ps1" -DriveLetter $letter -Bundle $source -DynamicEntrySs0:$DynamicEntrySs0 -Apply}
 $manifest|Set-Content (Join-Path $source 'SHA256SUMS.txt') -Encoding ASCII
 & "$PSScriptRoot/Copy-Usb.ps1" -DriveLetter $letter -Bundle $source -DynamicEntrySs0:$DynamicEntrySs0 -Apply|Out-Null
 if((Get-FileHash (Join-Path $drive 'EFI/BOOT/BOOTX64.EFI')).Hash -ne (Get-FileHash $efi).Hash){throw 'Final copy mismatch'}
 $backups=@(Get-ChildItem -LiteralPath $drive -Filter 'GPU-UNLOCK-BACKUP-*' -Directory)
 if($backups.Count -ne 1 -or (Get-FileHash (Join-Path $backups[0].FullName 'BOOTX64.EFI')).Hash -ne $prior){throw 'Old boot backup missing'}
 Reject {& "$PSScriptRoot/Copy-Usb.ps1" -DriveLetter $letter -Bundle $source -DynamicEntrySs0:$DynamicEntrySs0 -Apply}
 'PASS mocked filesystem: dry-run unchanged, non-USB rejection, traversal rejection, apply/checksums/backup, existing logs preserved. No physical USB accessed.'
}finally{Remove-PSDrive -Name $letter}
