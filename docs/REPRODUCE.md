# Reproduce, build and prepare locally

**v0.2:** the commands below preserve the original two-value mode. For the newly released dynamic mode use the matching explicit test/build/package/copy flags in [dynamic reproduction](DYNAMIC-SS0.md). Current original tests:122 cases, dynamic:221; compile-policy:32. The historical v0.1 count was119/23.

[繁體中文](REPRODUCE.zh-TW.md) · [Pitfalls](PITFALLS.md) · [Architecture](ARCHITECTURE.md)

The complete profile is in [README](../README.md), including specific bus/BAR/display topology. A same-model card is not enough. Keep Secure Boot recovery settings and any BitLocker recovery key available privately before changing firmware settings; the unsigned experiment requires Secure Boot off. Do not upload recovery keys.

## 1. Build tools

Windows x64; Python 3.12+, Clang/LLD **20.1.8**, Visual Studio C++ Build Tools and Windows SDK. Local reference versions are in [references](REFERENCES.md). PowerShell 5.1 or 7. Scripts do not install tools or modify system execution policy.

Obtain LLVM-20.1.8-win64.exe from the official release, verify SHA-256 `3197846a2b19063687dd56e93e34cd941e3548d907f23a6131571321bdf9fe7b` and extract locally with 7-Zip. Put its bin directory on the current process PATH or pass `-Clang` and `-LldLink` explicitly. `Test.ps1` discovers C++ Build Tools with vswhere; `-VcVars` overrides the vcvars64.bat path. Diagnostic wrappers derive MSVC/SDK versions from that environment.

From repository root:

~~~powershell
python --version
clang --version
lld-link --version
.\Test.ps1
.\Test-Diagnostic.ps1
.\Build.ps1
~~~

These are host builds/tests, not GPU execution. Test.ps1 runs 9 generic groups and 32 compile cases; without `-Prom` it explicitly skips 122 static / 221 dynamic fixture scenarios and the fixture-dependent default-mode test. Test-Diagnostic builds/checks v4, runs 60 scenarios and 5 invalid-report cases. Default Build creates `src/build/pny3080ti/diagnostic-only/PNY3080TI.EFI`; it cannot arm GPU writes/DMA/reset. It differs from the independent v4 inventory probe, and needs the same app data tree to run.

## 2. Independent diagnostic v4 and private evidence

Use an existing FAT32 USB that is not a system disk; back up its contents first. Copy the generated contents of `diagnostics/v4/bundle/` to it: `EFI/BOOT/BOOTX64.EFI`, `GPU-DIAG/ALLOW-LOG.TXT` and accompanying text/licenses. Preserve an existing BOOTX64.EFI before replacement. Do not format or change boot entries.

Cold-power boot, F11 on the measured board → UEFI USB. v4 collects PCI configuration, display ownership, bounded whitelisted MMIO and a 1MiB live PROM. **It temporarily enables memory decode and restores Command=0; it is not purely read-only.** It never enables bus mastering or executes GPU firmware. Stop/power off if restoration is unverified.

After returning to Windows, copy RUNxxxx.TXT, PRMxxxx.BIN and PRExxxx.TXT (if present) together into `.local/captures/`. Parse the real report, for example:

~~~powershell
.\diagnostics\v4\Read-Run.ps1 -Path .\.local\captures\RUN0003.TXT -OutputDirectory .\.local\assessment
.\tools\prom\Inspect-Live.ps1 -Prom .\.local\captures\PRM0001.BIN
~~~

Use your actual numbered files. Read-Run requires PROM in the report's directory and verifies its size/hash. Check ProbeCaptureComplete, RestoreVerified, ConfigUnchanged, attributes and all measured registers/topology; the summary does not physically prove a cold boot. FWSEC is `0x8241e0=3`, SS0 at `0x82381c` is `24321116` or `24321156`, SS1 at `0x823820` is 7. Do not use v3's withdrawn fuse interpretation.

Optional GPU-Z export analysis:
`.\tools\prom\Inspect-Rom.ps1 -RomPath .\.local\card.rom -OutputDirectory .\.local\rom-analysis`.
This only parses the measured IFR v3 format and normalizes its chain; it does not flash or verify firmware authentication. ROM/PROM remains local.

## 3. Obtain pinned firmware assets locally

Review [license boundaries](../THIRD_PARTY_NOTICES.md) and [input manifest](../tools/firmware/artifacts-610.43.03.json). Scripts download/check archives and parse selected members; no driver installer or downloaded source code is executed. NVIDIA firmware licenses remain separate. Use the official/manifest HTTPS URLs; if an archive disappears, do not substitute another version.

~~~powershell
python -m venv .local/venv
.\.local\venv\Scripts\python.exe -m pip install -r requirements-firmware.txt
python tools/firmware/fetch_artifacts.py .local/inputs
.\.local\venv\Scripts\python.exe tools/firmware/prepare_windows_assets.py --inputs .local/inputs --work .local/firmware
~~~

`--work` must be new; all outputs stay ignored beneath `.local`. To verify an existing input cache without network:
`python tools/firmware/fetch_artifacts.py .local/inputs --offline`.
The five assets in `src/assets.json` plus NVIDIA-LICENSE are verified. Public build/release contains no firmware.

## 4. Full fixture tests and experimental build

The PROM hash is deliberately pinned to the measured card. If yours differs, stop and investigate; do not replace the hash to force acceptance.

~~~powershell
.\Test.ps1 -Prom .\.local\captures\PRM0001.BIN
.\Build.ps1 -Experimental
~~~

Expected: 122 original-mode app/core scenarios (simulated) plus default-mode rejection case. The experimental EFI must be 66,048 bytes and hash `197602679e4574a5993e5ea6f963e128a7662ccaa1a382c2c2b29838e3a4bb08`. PE verification rejects imports and writable/executable sections. Build manifests identify source hashes; a new build alone is not a new hardware validation.

## 5. Local USB file bundle

Use the local summary generated from your measured v4 report. The package script checks diagnostic completeness/restoration, measured PROM/selectors/fuse, full-test log, source/EFI and asset hashes.

~~~powershell
python tools/package_local.py --assets .local/firmware/assets --diagnostic-summary .local/assessment/RUN0003.TXT.summary.json --output .local/usb
.\tools\Copy-Usb.ps1 -DriveLetter X -Bundle .\.local\usb
~~~

Replace **X** with the actual USB drive letter. The second command is a **dry run**. Review the selected disk/volume first. `-Apply` copies files, backs up an existing boot EFI, verifies copies and replaces BOOTX64.EFI last. It rejects non-USB/system/non-FAT32 volumes and an existing unlock directory, preserving old RUN logs; use another USB rather than deleting evidence. The new copier's physical write path has not been exercised during publication; only dry-run and mocked filesystem tests are used. Existing successful USB was staged with the original machine-specific script; its method is documented here.

The script does not format, partition, register boot entries or unlock. Power loss/copy failure can still leave a partial file tree; retain backups. Do not upload `.local/usb`: it contains firmware under its own license.

For the default non-writing port, prepare the same app data tree, but manually use the separately verified diagnostic-only EFI as BOOTX64.EFI on a separate backed-up USB. The experimental-only copier intentionally rejects that different hash.

## 6. Manual hardware attempt and validation

Follow [USB guide](USB-GUIDE.md): full shutdown/PSU off ~10s → F11 USB → U once → cleaned-up return → manual Windows selection **without reboot**. No automatic chainload or BCD/NVRAM installation.

Preserve RUNxx/REPORT.TXT. Require successful FINAL, SS0=88888888, unchanged SS1=7, single FLR, zero retained/maps/fault, quiesced, restored Command/Attributes. A failure to prove cleanup means physical power off.

In Windows, check both display devices are PNP OK and explicitly filter events by the current boot's XML UTC timestamp; inspect recent System events independently. Do not trust StartTime alone (pitfall12). This is bounded observation, not endurance proof.

For a matched benchmark, stop GPU workloads first and use a trusted locally installed CUDA12 cuBLAS DLL. Supply your GPU UUID privately to guard identity; public tool output excludes UUID and full DLL path:

~~~powershell
python tools/benchmark.py --cublas "C:\CUDA\bin\cublas64_12.dll" --bus 0000:04:00.0 --expect-uuid YOUR_LOCAL_GPU_UUID --output .local/before.json
~~~

Use the same command/script/DLL/parameters before and after, with a new output path (after.json). The sample DLL path is illustrative, not an installed path guarantee. No benchmark is launched by build/test/package. See [results and limits](RESULTS.md). Full cold-power restore and remeasurement are needed to prove reversibility; that round-trip remains uncompleted in this case.
