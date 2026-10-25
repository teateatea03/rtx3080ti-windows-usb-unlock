param([Parameter(Mandatory=$true)][string]$RomPath,[Parameter(Mandatory=$true)][string]$OutputDirectory)
$ErrorActionPreference='Stop'
if(Test-Path -LiteralPath $OutputDirectory){throw 'Preserve existing output'}
New-Item -ItemType Directory -Path $OutputDirectory|Out-Null
$r=[IO.File]::ReadAllBytes($romPath)
function Bounds([long]$o,[long]$n) { if ($o -lt 0 -or $n -lt 0 -or $o -gt $r.Length -or $n -gt $r.Length-$o) {throw "ROM range out of bounds: $o + $n"} }
function U16([long]$o) { Bounds $o 2; [BitConverter]::ToUInt16($r,[int]$o) }
function U32([long]$o) { Bounds $o 4; [BitConverter]::ToUInt32($r,[int]$o) }
if ((U32 0) -ne 0x4947564e) {throw 'Expected NVGI'}
$fixed=U32 4; $version=($fixed -shr 8) -band 255; $fixedSize=($fixed -shr 16) -band 0x7fff
$total=(U32 8) -band 0xfffff
if ($version -ne 3) {throw 'This analysis expects IFR v3'}
$status=U32 $total; $directory=[long]$status+4096
if ((U32 $directory) -ne 0x44524652) {throw 'Missing RFRD'}
$start=U32 ($directory+8)
if ($start % 4) {throw 'Unaligned ROM offset'}
$images=@(); $pos=[long]$start
for($i=0;$i -lt 8;$i++){
 Bounds $pos 0x1a; $sig=U16 $pos
 if ($sig -notin @(0xaa55,0x4e56,0xbb77)) {throw "Bad image signature at $pos"}
 $pcir=$pos+(U16 ($pos+0x18)); $dataSig=U32 $pcir
 if ($dataSig -notin @(0x52494350,0x5344504e,0x53494752)) {throw 'Bad PCI data signature'}
 Bounds $pcir 0x16
 $bytes=[long](U16 ($pcir+0x10))*512; $last=($r[$pcir+0x15] -band 0x80) -ne 0
 $npde=($pcir+(U16 ($pcir+0xa))+15) -band -16
 $npdeInfo=$null
 if($npde -le $r.Length-10 -and (U32 $npde) -eq 0x4544504e){
  $rev=U16 ($npde+4); $len=U16 ($npde+6)
  if($rev -in @(0x100,0x101)){
   if($len -lt 10) {throw 'Bad NPDE length'}; Bounds $npde $len
   $sub=[long](U16 ($npde+8))*512
   if($len -gt 10){$last=($r[$npde+10] -band 0x80) -ne 0}
   elseif($sub -lt $bytes){$last=$false}
   $bytes=$sub; $npdeInfo=[ordered]@{Offset=$npde;Revision=$rev;Length=$len}
  }
 }
 if(!$bytes){throw 'Zero image length'}; Bounds $pos $bytes
 $sum=0; for($j=$pos;$j -lt $pos+$bytes;$j++){$sum=($sum+$r[$j]) -band 255}
 $images += [ordered]@{Offset=$pos;RelativeOffset=$pos-$start;DataOffset=$pcir;Signature=('{0:X4}' -f $sig);DataSignature=[Text.Encoding]::ASCII.GetString($r,$pcir,4);Vendor=(U16 ($pcir+4));Device=(U16 ($pcir+6));CodeType=[int]$r[$pcir+0x14];Bytes=$bytes;Last=$last;ByteSumMod256=$sum;Npde=$npdeInfo}
 $pos+=$bytes
 if($last){break}
}
if(!$last){throw 'No bounded final ROM image'}
$chainLength=$pos-$start
if($chainLength -gt 1MB){throw 'Exceeds upstream parser cap'}
$view=New-Object byte[] $chainLength
[Array]::Copy($r,$start,$view,0,$chainLength)
$viewPath=Join-Path $OutputDirectory 'pci-rom-analysis-only.bin'
[IO.File]::WriteAllBytes($viewPath,$view)
$versionString='94.02.71.00.74'
if(![Text.Encoding]::ASCII.GetString($r).Contains($versionString)){throw 'Expected BIOS version absent'}
$result=[ordered]@{Source=$romPath;SourceBytes=$r.Length;SourceSha256=(Get-FileHash -LiteralPath $romPath -Algorithm SHA256).Hash;IFRVersion=$version;FixedSize=$fixedSize;TotalDataSize=$total;FlashStatusOffset=$status;DirectoryOffset=$directory;PciRomOffset=$start;ChainBytes=$chainLength;Images=$images;IFRSubsystemVendor=(U16 12);IFRSubsystemDevice=(U16 14);ExpectedVersionStringPresent=$true;NormalizedView=$viewPath;NormalizedSha256=(Get-FileHash -LiteralPath $viewPath -Algorithm SHA256).Hash;FirmwareSignatureVerified=$false;HardwareAccess=$false}
$result | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'rom-structure.json') -Encoding UTF8
$result | ConvertTo-Json -Depth 10
