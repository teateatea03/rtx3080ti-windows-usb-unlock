$ErrorActionPreference='Stop'
$base=Join-Path $PSScriptRoot 'build'
$sample=Join-Path $base 'host-sample-v4.TXT'
$original=[IO.File]::ReadAllText($sample,[Text.Encoding]::ASCII)
$restoreMatch=[regex]::Match($original,'PROBE_RESTORE_VERIFIED=([0-9a-f]{16})')
if(!$restoreMatch.Success){throw 'Missing fixture restore field'}
$cases=@(
 [ordered]@{Name='wrong-fuse-address';Text=($original.Replace('008241e0','00824160'));CopyProm=$true;Expected='Invalid/duplicate probe register'},
 [ordered]@{Name='missing-attributes';Text=([regex]::Replace($original,'(?m)^PROBE_ATTRIBUTE_[^\r\n]+\r?\n',''));CopyProm=$true;Expected='Successful probe claim'},
 [ordered]@{Name='restore-claim-mismatch';Text=($original.Replace($restoreMatch.Value,'PROBE_RESTORE_VERIFIED='+('0'*16)));CopyProm=$true;Expected='Successful probe claim'},
 [ordered]@{Name='missing-prom';Text=$original;CopyProm=$false;Expected='Missing/incomplete PROM'},
 [ordered]@{Name='incomplete-log';Text=($original.Replace('END-OF-INVENTORY',''));CopyProm=$false;Expected='Incomplete or wrong-version report'}
)
foreach($case in $cases){
 $dir=Join-Path $base ('report-test-'+$case.Name)
 if(!(Test-Path -LiteralPath $dir)){New-Item -ItemType Directory -Path $dir | Out-Null}
 $path=Join-Path $dir 'RUN0000.TXT'
 [IO.File]::WriteAllText($path,$case.Text,[Text.Encoding]::ASCII)
 if($case.CopyProm){Copy-Item -LiteralPath (Join-Path $base 'PRM0000.BIN') -Destination (Join-Path $dir 'PRM0000.BIN') -Force}
 $rejected=$false
 try {& (Join-Path $PSScriptRoot 'Read-Run.ps1') -Path $path -OutputDirectory (Join-Path $dir 'parsed') | Out-Null}
 catch {if($_.Exception.Message -notlike ('*'+$case.Expected+'*')){throw};$rejected=$true}
 if(!$rejected){throw "Invalid report accepted: $($case.Name)"}
 Write-Output ('PASS '+$case.Name)
}
'Five invalid evidence cases rejected; valid host sample already parsed with restore verified and 1 MiB PROM.'
