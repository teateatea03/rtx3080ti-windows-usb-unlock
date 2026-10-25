param(
 [string]$Clang='clang',
 [string]$VcVars='',
 [string]$Python='python',
 [string]$Prom='',
 [string]$Config='../.local/config256.bin',
 [string]$Assets='../.local/firmware/assets'
)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../tools/Host-Environment.ps1')
if(!$VcVars){$VcVars=Find-VcVars}
Push-Location $PSScriptRoot
try {
 $out='build/tests/pny3080ti'
 New-Item -ItemType Directory -Path $out -Force|Out-Null
 $vcCommand='call '+[char]34+$VcVars+[char]34+' >nul && set'
 $environment=& cmd.exe /d /c $vcCommand
 if($LASTEXITCODE -ne 0){throw 'Host SDK environment failed'}
 foreach($line in $environment){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($matches[1],$matches[2],'Process')}}
 if((Get-FileHash -LiteralPath $Prom).Hash -ne '991CBE4878A4C6F0456B6412FFFF3DAB5E7F60EC317BA4539254EB52B692F140'){throw 'Wrong PROM'}
 if((Get-FileHash -LiteralPath $Config).Hash -ne 'EC681E8F2B2F4CEF6E6EC0E451B1D2814C31B6B2C32DEFD5BD65CC8747A5C1B2'){throw 'Wrong PCI capture'}
 foreach($asset in (Get-Content -Raw -LiteralPath assets.json|ConvertFrom-Json)){
  $path=Join-Path $Assets $asset.Name
  if((Get-Item -LiteralPath $path).Length -ne $asset.Bytes -or (Get-FileHash -LiteralPath $path).Hash -ne $asset.Sha256){throw "Wrong asset $($asset.Name)"}
 }
 $flags=@('-std=c11','-O1','-Wall','-Wextra','-Werror','-Wno-unused-parameter','-Wno-unused-function','-D_CRT_SECURE_NO_WARNINGS','-Icompat','-I../deps/edk2/MdePkg/Include','-I../deps/edk2/MdePkg/Include/X64')
 $defs=@('-D__KERNEL__','-DGU_UEFI_PORT','-DGU_PROFILE_PNY_B550_3080TI','-DGU_TARGET_PCI_ID=0x220810deU','-DGU_EXPERIMENTAL_3090','-DGU_ENTRY_LATCHED_SS1_PRESERVE')
 $sources=@('tests/test_pny.c','app_io.c','ownership.c','format.c','sha256.c','checks.c','port.c','compat.c','vendor/unlock_610.c','vendor/prep_610.c','vendor/layout_610.c','vendor/vbios_610.c','vendor/gfw_eval.c')
 $runStamp=Get-Date -Format 'yyyyMMdd-HHmmss-fff'
 $log="$out/pny-host-$runStamp.log"
 $exe="$out/pny-tests.exe"
 & $Clang @flags @defs @sources -o $exe
 if($LASTEXITCODE -ne 0){throw 'PNY host compilation failed'}
 $priorPreference=$ErrorActionPreference;$ErrorActionPreference='Continue'
 & "./$exe" $Prom $Assets $Config 2>&1|Tee-Object -FilePath $log
 $hostCode=$LASTEXITCODE;$ErrorActionPreference=$priorPreference
 if($hostCode -ne 0){throw 'PNY host tests failed'}
 $diagnosticDefs=@($defs|Where-Object {$_ -notin @('-DGU_EXPERIMENTAL_3090','-DGU_ENTRY_LATCHED_SS1_PRESERVE')})
 & $Clang @flags @diagnosticDefs @sources -o "$out/pny-diagnostic-tests.exe"
 if($LASTEXITCODE -ne 0){throw 'PNY diagnostic host compilation failed'}
 & "./$out/pny-diagnostic-tests.exe" $Prom $Assets $Config
 if($LASTEXITCODE -ne 0){throw 'PNY diagnostic host test failed'}
 [ordered]@{AllPassed=$true;HardwareAccess=$false;FirmwareExecuted=$false;EfiMocked=$true;
  PromSha256=(Get-FileHash -LiteralPath $Prom).Hash;ConfigSha256=(Get-FileHash -LiteralPath $Config).Hash;
  TestExeSha256=(Get-FileHash -LiteralPath $exe).Hash;LogSha256=(Get-FileHash -LiteralPath $log).Hash;
  Compiler=(& $Clang --version|Select-Object -First 1);Sources=$sources;
  CompletedAt=(Get-Date).ToString('o')}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath "$out/pny-host-results.json" -Encoding UTF8
} finally {Pop-Location}
