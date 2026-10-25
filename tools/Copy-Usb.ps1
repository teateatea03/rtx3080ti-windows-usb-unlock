param([Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z]$')][string]$DriveLetter,
 [Parameter(Mandatory=$true)][string]$Bundle,[switch]$Apply,[switch]$DynamicEntrySs0)
$ErrorActionPreference='Stop'
# Copies files only. No formatting, partitioning, BCD, NVRAM or boot-order calls.
$partition=Get-Partition -DriveLetter $DriveLetter
$disk=Get-Disk -Number $partition.DiskNumber
$volume=Get-Volume -DriveLetter $DriveLetter
if($disk.BusType -ne 'USB' -or $disk.IsBoot -or $disk.IsSystem -or
 $partition.IsBoot -or $partition.IsSystem -or $volume.FileSystem -ne 'FAT32'){
 throw 'Require an existing non-system FAT32 USB volume'
}
$root=$DriveLetter.ToUpperInvariant()+':\'
$Bundle=(Resolve-Path -LiteralPath $Bundle).Path
function Hash([string]$Path){(Get-FileHash -LiteralPath $Path).Hash.ToLowerInvariant()}
function SafePath([string]$Base,[string]$Relative){
 $full=[IO.Path]::GetFullPath((Join-Path $Base $Relative))
 if(!$full.StartsWith($Base.TrimEnd('\')+'\',[StringComparison]::OrdinalIgnoreCase)){throw 'Unsafe manifest path'}
 $full
}
foreach($base in @($Bundle,$root)){
 $all=@(Get-Item -LiteralPath $base)+@(Get-ChildItem -LiteralPath $base -Force -Recurse)
 if(@($all|Where-Object {$_.Attributes -band [IO.FileAttributes]::ReparsePoint}).Count){throw 'Reparse points are unsupported'}
}
$parent=Join-Path $root 'PNY3080TI-UNLOCK'
if(Test-Path -LiteralPath $parent){throw 'Preserve the existing unlock directory and logs; use another USB'}
$entries=@();$seen=@{}
foreach($line in Get-Content -LiteralPath (Join-Path $Bundle 'SHA256SUMS.txt')){
 if($line -notmatch '^([a-f0-9]{64})  (.+)$'){throw 'Invalid manifest'}
 $hash=$Matches[1];$relative=$Matches[2]
 if($relative -match '\.\.|[:\\]' -or $relative -eq 'PNY3080TI-UNLOCK/PAYLOAD-SHA256SUMS.txt' -or $seen.ContainsKey($relative) -or
  ($relative -ne 'EFI/BOOT/BOOTX64.EFI' -and $relative -notmatch '^PNY3080TI-UNLOCK/[A-Za-z0-9._/-]+$')){throw 'Unexpected payload path'}
 $seen[$relative]=$true
 $src=SafePath $Bundle $relative
 if((Hash $src) -ne $hash){throw 'Changed bundle'}
 $entries+=,[pscustomobject]@{Relative=$relative;Source=$src;Hash=$hash}
}
$boot=@($entries|Where-Object {$_.Relative -eq 'EFI/BOOT/BOOTX64.EFI'})
$expectedEfi=if($DynamicEntrySs0){'0cf96d7ce5b81b87caf463b6ef4a2365b4785fb6d64273ee38a7d9f699e96845'}else{'197602679e4574a5993e5ea6f963e128a7662ccaa1a382c2c2b29838e3a4bb08'}
if($boot.Count -ne 1 -or $boot[0].Hash -ne $expectedEfi){throw 'Wrong experimental EFI'}
[pscustomobject]@{Drive=$root;DiskNumber=$disk.Number;Model=$disk.FriendlyName;FileSystem=$volume.FileSystem;Apply=[bool]$Apply;Files=$entries.Count}
if(!$Apply){'Dry run only. Review the drive and bundle; -Apply copies files.';return}
$bootPath=Join-Path $root 'EFI/BOOT/BOOTX64.EFI'
$pending=Join-Path $root 'EFI/BOOT/PNY3080TI.NEW'
if(Test-Path -LiteralPath $pending){throw 'Preserve existing pending EFI'}
New-Item -ItemType Directory -Path (Split-Path $bootPath) -Force|Out-Null
if(Test-Path -LiteralPath $bootPath){
 $backup=Join-Path $root ('GPU-UNLOCK-BACKUP-'+(Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
 New-Item -ItemType Directory -Path $backup|Out-Null
 Copy-Item -LiteralPath $bootPath -Destination (Join-Path $backup 'BOOTX64.EFI')
 if((Hash $bootPath) -ne (Hash (Join-Path $backup 'BOOTX64.EFI'))){throw 'Backup failed'}
}
foreach($entry in $entries|Where-Object {$_.Relative -ne 'EFI/BOOT/BOOTX64.EFI'}){
 $dest=SafePath $root $entry.Relative
 New-Item -ItemType Directory -Path (Split-Path $dest) -Force|Out-Null
 Copy-Item -LiteralPath $entry.Source -Destination $dest
 if((Hash $dest) -ne $entry.Hash){throw 'Copy verification failed; boot EFI was not replaced'}
}
Copy-Item -LiteralPath (Join-Path $Bundle 'SHA256SUMS.txt') -Destination (Join-Path $parent 'PAYLOAD-SHA256SUMS.txt')
Copy-Item -LiteralPath $boot[0].Source -Destination $pending
if((Hash $pending) -ne $boot[0].Hash){throw 'Pending EFI mismatch'}
Move-Item -LiteralPath $pending -Destination $bootPath -Force
if((Hash $bootPath) -ne $boot[0].Hash){throw 'Final EFI mismatch'}
'File copy complete; boot EFI replaced last. No unlock attempted.'
