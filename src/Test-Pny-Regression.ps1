param(
 [string]$Clang='clang',
 [string]$Python='python',
 [string]$VcVars=''
)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../tools/Host-Environment.ps1')
if(!$VcVars){$VcVars=Find-VcVars}
Push-Location $PSScriptRoot
try {
 $out='build/tests/pny-regression';New-Item -ItemType Directory -Path $out -Force|Out-Null
 $vcCommand='call '+[char]34+$VcVars+[char]34+' >nul && set'
 $environment=& cmd.exe /d /c $vcCommand
 if($LASTEXITCODE -ne 0){throw 'Host SDK unavailable'}
 foreach($line in $environment){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($matches[1],$matches[2],'Process')}}
 $flags=@('-std=c11','-O1','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-unused-function','-D_CRT_SECURE_NO_WARNINGS','-Icompat','-I../deps/edk2/MdePkg/Include','-I../deps/edk2/MdePkg/Include/X64')
 $core=@('port.c','compat.c','vendor/unlock_610.c','vendor/prep_610.c','vendor/layout_610.c','vendor/vbios_610.c','vendor/gfw_eval.c')
 & $Clang @flags -fno-builtin -Dmemcpy=GuTestMemcpy -Dmemset=GuTestMemset -Dmemchr=GuTestMemchr -Dmemcmp=GuTestMemcmp -Dstrcmp=GuTestStrcmp -Dstrlen=GuTestStrlen -c runtime.c -o "$out/runtime-host.obj"
 if($LASTEXITCODE -ne 0){throw 'Runtime compilation failed'}
 $jobs=@(
  @{Name='config-path';Sources=@('tests/test_generic_checks.c','checks.c');Defines=@()},
  @{Name='port';Sources=@('tests/test_port.c','port.c');Defines=@('-DGU_HOST_WRITE_TEST')},
  @{Name='write-lock';Sources=@('tests/test_preflight_lock.c','port.c');Defines=@()},
  @{Name='master-attributes';Sources=@('tests/test_master_attributes.c')+$core;Defines=@('-D__KERNEL__','-DGU_UEFI_PORT','-DGU_TARGET_PCI_ID=0x220410deU','-DGU_HOST_WRITE_TEST')},
  @{Name='runtime';Sources=@('tests/test_runtime.c','format.c',"$out/runtime-host.obj");Defines=@('-fno-builtin')}
 )
 foreach($d in @('GU_STOCK_PROFILE_ORIGINAL_03100062','GU_STOCK_PROFILE_OBSERVED_03000062','GU_ENTRY_LATCHED_SS1_PRESERVE','GU_ENTRY_LATCHED_SS1_FULL')){
  $jobs+=@{Name="profile-$d";Sources=@('tests/test_profile_lock.c','port.c');Defines=@('-DGU_EXPERIMENTAL_3090',"-D$d")}
 }
 & $Python tests/test_pny_selection.py $Clang
 if($LASTEXITCODE -ne 0){throw 'Production selection test failed'}
 $results=@()
 foreach($job in $jobs){
  $exe="$out/$($job.Name).exe"
  & $Clang @flags @($job.Defines) @($job.Sources) -o $exe
  if($LASTEXITCODE -ne 0){throw "Compile failed: $($job.Name)"}
  & "./$exe" 2>&1|Tee-Object -FilePath "$out/$($job.Name).log"
  if($LASTEXITCODE -ne 0){throw "Test failed: $($job.Name)"}
  $results+=[ordered]@{Name=$job.Name;Passed=$true}
 }
 [ordered]@{ExecutedTestsPassed=$true;HardwareAccess=$false;OriginalHardwareFixturesAvailable=$false;
  Omitted='Original 3090 capture/PROM-specific tests; PNY live captures tested separately';Groups=$results}|
  ConvertTo-Json -Depth 5|Set-Content -LiteralPath "$out/results.json" -Encoding UTF8
}finally{Pop-Location}
