param([string]$Msvc='C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\MSVC\14.44.35207')
$ErrorActionPreference='Stop'
$compiler=Join-Path $Msvc 'bin/Hostx64/x64/cl.exe'
$linker=Join-Path $Msvc 'bin/Hostx64/x64/link.exe'
$headers=Join-Path $PSScriptRoot 'vendor/edk2/MdePkg/Include'
$output=Join-Path $PSScriptRoot 'build'
$bundle=Join-Path $PSScriptRoot 'bundle'
$manifest=Get-Content -LiteralPath (Join-Path $PSScriptRoot 'vendor-manifest.json') -Raw -Encoding UTF8 | ConvertFrom-Json
foreach ($file in $manifest.Files) {
    $path=Join-Path (Join-Path $PSScriptRoot 'vendor/edk2') $file.Path
    if ((Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash -ne $file.Sha256) {throw "Changed dependency: $($file.Path)"}
}
New-Item -ItemType Directory -Path $output,(Join-Path $bundle 'EFI/BOOT'),(Join-Path $bundle 'GPU-DIAG') -Force | Out-Null
$source=Join-Path $PSScriptRoot 'src/diagnostic.c'
$object=Join-Path $output 'diagnostic.obj'
$efi=Join-Path $output 'GPU-DIAG.EFI'
& $compiler /nologo /TC /std:c11 /W4 /WX /GS- /Zl /Oi /Os /c "/I$headers" "/I$headers/X64" "/Fo$object" $source
if ($LASTEXITCODE -ne 0) {throw 'Diagnostic compile failed'}
& $linker /nologo /nodefaultlib /subsystem:efi_application /entry:efi_main /machine:x64 /base:0x100000000 /dynamicbase:no /fixed:no /Brepro /opt:ref "/map:$output/diagnostic.map" "/out:$efi" $object
if ($LASTEXITCODE -ne 0) {throw 'Diagnostic link failed'}
Copy-Item -LiteralPath $efi -Destination (Join-Path $bundle 'EFI/BOOT/BOOTX64.EFI')
[IO.File]::WriteAllText((Join-Path $bundle 'GPU-DIAG/ALLOW-LOG.TXT'),'GPU-DIAG-USB-LOG-v1',[Text.Encoding]::ASCII)
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'GUIDE-v2.md') -Destination (Join-Path $bundle 'README.md')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'LICENSE.txt') -Destination (Join-Path $bundle 'LICENSE.txt')
[ordered]@{
    Mode='read-only-UEFI-config256-console-v2';CanUnlock=$false;HardwareTested=$false
    Compiler=$compiler;CompilerVersion=(Get-Item -LiteralPath $compiler).VersionInfo.FileVersion
    HeaderRevision=$manifest.Revision;EfiSha256=(Get-FileHash $efi -Algorithm SHA256).Hash
    SourceSha256=(Get-FileHash $source -Algorithm SHA256).Hash
    SourceFiles=@(Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'src') -File | Sort-Object Name | ForEach-Object {[ordered]@{Name=$_.Name;Sha256=(Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash}})
} | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $output 'build-manifest.json') -Encoding UTF8
Write-Output 'Built independent diagnostic and USB file bundle; no USB, firmware, or boot settings changed.'
