# Host build helpers only; no device or boot configuration changes.
function Find-VcVars {
 $vswhere=Join-Path ([Environment]::GetFolderPath('ProgramFilesX86')) 'Microsoft Visual Studio/Installer/vswhere.exe'
 if(!(Test-Path -LiteralPath $vswhere)){throw 'Install Visual Studio C++ Build Tools or supply -VcVars'}
 $install=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
 if(!$install){throw 'Visual Studio C++ tools unavailable'}
 Join-Path $install 'VC/Auxiliary/Build/vcvars64.bat'
}
function Import-VcEnvironment([string]$VcVars) {
 if(!$VcVars){$VcVars=Find-VcVars}
 $lines=& cmd.exe /d /c ('call "'+$VcVars+'" >nul && set')
 if($LASTEXITCODE -ne 0){throw 'C++ SDK environment failed'}
 foreach($line in $lines){if($line -match '^([^=]+)=(.*)$'){[Environment]::SetEnvironmentVariable($matches[1],$matches[2],'Process')}}
}
