param([Parameter(Mandatory=$true)][string]$Path,
      [Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
$lines=[IO.File]::ReadAllLines((Resolve-Path -LiteralPath $Path).Path,[Text.Encoding]::ASCII)
if($lines[0] -cne 'GPU-DIAG v3' -or $lines[-1] -cne 'END-OF-INVENTORY'){throw 'Incomplete or wrong-version report'}
function Number([string]$line,[string]$key) {
 $pattern='(?:^| )'+[regex]::Escape($key)+'=([0-9a-f]{16})(?: |$)'
 $m=[regex]::Match($line,$pattern)
 if(!$m.Success){throw "Missing numeric field: $key"}
 [Convert]::ToUInt64($m.Groups[1].Value,16)
}
$footer=@($lines | Where-Object {$_ -like 'PROBLEMS=*'})
if($footer.Count -ne 1 -or (Number $footer[0] 'TRUNCATED') -ne 0){throw 'Missing footer or truncated report'}
$gpus=@();$current=$null;$textHandles=@();$textCurrent=$null;$ownerContext=$null;$edges=@()
foreach($line in $lines){
 if($line -like 'PCI handle=*'){
  $current=$null
  if(((Number $line 'class-revision') -shr 24) -eq 3) {
   $current=[ordered]@{Handle=(Number $line 'handle');Bus=(Number $line 'bus');Device=(Number $line 'device');Function=(Number $line 'function');Segment=(Number $line 'segment');Id=(Number $line 'id');Subsystem=(Number $line 'subsystem');SnapshotStatus=$null;Words=@{};CapabilityChainComplete=$false;Capabilities=@();SnapshotValid=$false}
   $gpus+=,$current
  }
 } elseif($line -like 'CONFIG256 handle=*'){
  if(!$current -or (Number $line 'handle') -ne $current.Handle){throw 'Unmatched configuration snapshot'}
  if($null -ne $current.SnapshotStatus){throw 'Duplicate configuration snapshot'}
  $current.SnapshotStatus=Number $line 'status'
 } elseif($line -like ' CFG_DWORD offset=*'){
  if(!$current -or $current.SnapshotStatus -ne 0){throw 'Data outside successful snapshot'}
  $offset=Number $line 'offset';$value=Number $line 'value'
  if($offset -gt 252 -or $offset%4 -or $value -gt [uint32]::MaxValue -or $current.Words.ContainsKey([int]$offset)){throw 'Invalid or duplicate config DWORD'}
  $current.Words[[int]$offset]=[uint32]$value
 } elseif($line -like ' CAP offset=*') {
  if(!$current){throw 'Capability without device'}
  $current.Capabilities+=,[ordered]@{Offset=(Number $line 'offset');Id=(Number $line 'id');Next=(Number $line 'next')}
 } elseif($line -like ' CAP_CHAIN_COMPLETE count=*') {
  if(!$current -or (Number $line 'count') -ne $current.Capabilities.Count){throw 'Invalid capability completion'}
  $current.CapabilityChainComplete=$true
 } elseif($line -like 'TEXT_OUT handle=*'){
  $textCurrent=[ordered]@{Handle=(Number $line 'handle');IsConsole=((Number $line 'system-console') -eq 1);IsStderr=((Number $line 'system-stderr') -eq 1);DirectGop=((Number $line 'direct-gop') -eq 1);PciCandidates=$null;PciOwner=$null}
  $textHandles+=,$textCurrent
 } elseif($line -like ' TEXT_PCI_CANDIDATES=*') {
  if(!$textCurrent){throw 'Console mapping without text handle'}
  $textCurrent.PciCandidates=Number $line 'TEXT_PCI_CANDIDATES'
  $textCurrent.PciOwner=Number $line 'owner'
 } elseif($line -like 'OWNERS handle=*'){
  $ownerContext=[ordered]@{Handle=(Number $line 'handle');ProtocolData1=(Number $line 'protocol-data1')}
 } elseif($line -like ' AGENT=*'){
  if(!$ownerContext){throw 'Ownership edge without context'}
  $edges+=,[ordered]@{Handle=$ownerContext.Handle;ProtocolData1=$ownerContext.ProtocolData1;Agent=(Number $line 'AGENT');Controller=(Number $line 'controller');Attributes=(Number $line 'attributes');Count=(Number $line 'count')}
 }
}
foreach($gpu in $gpus){
 $gpu.SnapshotValid=$gpu.SnapshotStatus -eq 0 -and $gpu.Words.Count -eq 64
 if($gpu.SnapshotValid){
  $gpu.SnapshotValid=$gpu.Words[0] -eq $gpu.Id -and $gpu.Words[44] -eq $gpu.Subsystem
 }
 $gpu.Words=@($gpu.Words.GetEnumerator() | Sort-Object Key | ForEach-Object {[ordered]@{Offset=$_.Key;Value=$_.Value}})
}
$probeLines=@($lines|Where-Object {$_ -like 'PROBE_STATUS=*'})
if($probeLines.Count -ne 1){throw 'Missing or duplicate probe result'}
$probeStatus=Number $probeLines[0] 'PROBE_STATUS'
$restoreLines=@($lines|Where-Object {$_ -like 'PROBE_RESTORE_VERIFIED=*'})
$configLines=@($lines|Where-Object {$_ -like 'PROBE_CONFIG256_UNCHANGED=*'})
$promLines=@($lines|Where-Object {$_ -like 'PROBE_PROM_SAVE_STATUS=*'})
$regs=@();$seen=@{}
$allowed=@(0,0x1183a4,0x118128,0x118234,0x110100,0x823804,0x82381c,0x823820,0x1fa824,0x1fa828,0x824148,0x824160)
foreach($line in $lines|Where-Object {$_ -like 'PROBE_REG offset=*'}){
 $offset=Number $line 'offset'
 if($offset -notin $allowed -or $seen.ContainsKey($offset)){throw 'Invalid/duplicate probe register'}
 $seen[$offset]=$true;$regs+=,[ordered]@{Offset=$offset;Value=(Number $line 'value')}
}
$restored=$restoreLines.Count -eq 1 -and (Number $restoreLines[0] 'PROBE_RESTORE_VERIFIED') -eq 1 -and (Number $restoreLines[0] 'command') -eq 0
$unchanged=$configLines.Count -eq 1 -and (Number $configLines[0] 'PROBE_CONFIG256_UNCHANGED') -eq 1
$promSource=$null;$promHash=$null;$promBytes=0
if($promLines.Count -gt 1){throw 'Duplicate PROM result'}
if($promLines.Count -eq 1 -and (Number $promLines[0] 'PROBE_PROM_SAVE_STATUS') -eq 0){
 $index=Number $promLines[0] 'prom-index';$promBytes=Number $promLines[0] 'bytes'
 if($index -gt 9999 -or $promBytes -ne 1048576){throw 'Invalid PROM metadata'}
 $promSource=Join-Path (Split-Path (Resolve-Path -LiteralPath $Path).Path) ('PRM{0:D4}.BIN' -f $index)
 if(!(Test-Path -LiteralPath $promSource) -or (Get-Item -LiteralPath $promSource).Length -ne $promBytes){throw 'Missing/incomplete PROM artifact'}
 $promHash=(Get-FileHash -LiteralPath $promSource).Hash
}
$complete=$probeStatus -eq 0 -and $restored -and $unchanged -and $regs.Count -eq 12 -and $null -ne $promHash
if($probeStatus -eq 0 -and !$complete){throw 'Successful probe claim has incomplete supporting evidence'}
$hash=(Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash
$result=[ordered]@{Version=3;SourceSha256=$hash;Problems=(Number $footer[0] 'PROBLEMS');Truncated=$false;DisplayDevices=$gpus;TextOutputHandles=$textHandles;OwnershipEdges=$edges;HardwareUnlockProven=$false;ProbeStatus=$probeStatus;RestoreVerified=$restored;ConfigUnchanged=$unchanged;Registers=$regs;PromSource=$promSource;PromSha256=$promHash;PromBytes=$promBytes;ProbeCaptureComplete=$complete;ColdStateCollected=$complete;ColdBootPhysicallyVerified=$false}
if(!(Test-Path -LiteralPath $OutputDirectory)){New-Item -ItemType Directory -Path $OutputDirectory | Out-Null}
$name=[IO.Path]::GetFileName($Path)
$raw=Join-Path $OutputDirectory $name
if(Test-Path -LiteralPath $raw){
 if((Get-FileHash -LiteralPath $raw -Algorithm SHA256).Hash -ne $hash){throw 'Preserve existing evidence with different contents'}
} else {Copy-Item -LiteralPath $Path -Destination $raw}
if($promSource){
 $dest=Join-Path $OutputDirectory ([IO.Path]::GetFileName($promSource))
 if(Test-Path -LiteralPath $dest){if((Get-FileHash -LiteralPath $dest).Hash -ne $promHash){throw 'Preserve existing PROM evidence'}}
 else {Copy-Item -LiteralPath $promSource -Destination $dest}
}
$result | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $OutputDirectory ($name+'.summary.json')) -Encoding UTF8
[pscustomobject]@{Version=3;Problems=$result.Problems;DisplayDevices=$gpus.Count;CompleteSnapshots=@($gpus|Where-Object {$_.SnapshotValid}).Count;TextOutputs=$textHandles.Count;OwnershipEdges=$edges.Count;EvidenceDirectory=$OutputDirectory}
