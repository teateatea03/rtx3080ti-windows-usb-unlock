# RTX 3080 Ti — manual USB unlock for Windows

[繁體中文](README.zh-TW.md) · [Reproduce](docs/REPRODUCE.md) · [Pitfalls](docs/PITFALLS.md) · [Results](docs/RESULTS.md) · [Credits](docs/REFERENCES.md)

A free, GPL-2.0-only research port for **one measured PNY RTX 3080 Ti / ASRock B550 Steel Legend / separate RTX 5080 display** configuration. Select a USB from the firmware's one-time boot menu, press **U once**, then manually select Windows Boot Manager in the **same boot**. Ordinary Windows startup does not run the tool.

**One successful hardware attempt**, with FP32-accumulating cuBLAS GEMM:

| Input → FP32 output | Before | After | Ratio |
| --- | ---: | ---: | ---: |
| FP16 | 59.717 TFLOPS | 114.491 TFLOPS | 1.917× |
| BF16 | 69.918 TFLOPS | 114.442 TFLOPS | 1.637× |

This is a constant-input 4096³ GEMM measurement, not an application benchmark or long-term stability result. All 16,777,216 outputs per type were exactly 4096. [Parameters, evidence and limits](docs/RESULTS.md).

## Scope

| Item | Measured / policy |
| --- | --- |
| Target | PNY 12 GB, `10de:2208`, subsystem `196e:1384`, `0000:04:00.0` |
| vBIOS / chip | `94.02.71.00.74` / GA102 `b72000a1` |
| Host | B550 Steel Legend BIOS P3.90, Ryzen 5700X (no iGPU), 64 GB |
| Display GPU | RTX 5080 `10de:2c02 / 19da:1762`, bus 8; target must have no driver/GOP |
| OS / driver | Windows 11 build 26300 / NVIDIA 617.42 |
| Secure Boot | Disabled for this unsigned experiment |
| Target PCI Command | Initially zero; temporary memory decode via EFI Attributes; restored |
| Selectors | SS0 `24321116` or `24321156`, latched once; SS1 `7` preserved |
| FWSEC | Register `0x8241e0`, raw `3`, version `2`, signature offset `768` |

**Not a general 3080 Ti unlocker.** Other boards, BAR assignments, display routes, drivers and single-GPU systems are unverified. RTX 40/50-series news claims are not implemented here. Do not bypass a rejection by changing just the PCI ID or deleting guards.

The experimental EFI performs privileged GPU writes, DMA and reset; a hang or failed cleanup can require physical power removal. It does not flash vBIOS, patch the Windows driver, register BCD/NVRAM entries or change the default boot order. USB selection reduces persistence and makes use optional; it does not make GPU firmware execution risk-free.

## Start here

1. Read [reproduction](docs/REPRODUCE.md) and [pitfalls](docs/PITFALLS.md).
2. Build/test the independent **diagnostic v4** first; keep its local report and PROM private.
3. Build the non-writing default port. Explicit `-Experimental` builds the manual unlock mode.
4. Obtain five hash-pinned firmware assets locally, run full fixture tests, prepare a local USB bundle.
5. Follow the [manual USB guide](docs/USB-GUIDE.md). Cleanup failure means power off before continuing.

Source, licenses, redacted measured evidence and reproduction scripts are public. **No NVIDIA firmware binaries, ROM/PROM dump, CUDA DLL or private device identifiers are distributed.** A release EFI alone is not a complete USB image. Local bundles contain separately licensed firmware and must not be uploaded.

## Validation and source

The public build reproduced the successful 66,048-byte EFI exactly:

`197602679e4574a5993e5ea6f963e128a7662ccaa1a382c2c2b29838e3a4bb08`

Local validation: 119 simulated app/core scenarios plus non-writing default case; 9 fixture-independent regression groups; 23 compile-policy cases; diagnostic v4 60 scenarios and 5 invalid-report cases; PE checks and offline firmware reconstruction. CI has no GPU or private PROM and explicitly skips fixture-dependent tests. Host tests cannot prove firmware authentication or hardware behavior.

This port builds on impacifist, rough2515-hub, Zagitta and WildFlash1st. See [pinned references](docs/REFERENCES.md), [licenses](THIRD_PARTY_NOTICES.md), [changes](docs/CHANGES-PNY.md). Original authors retain copyrights. There is no fee/paywall; GPL permits commercial redistribution under its conditions.

[Contributing](CONTRIBUTING.md) · [Security](SECURITY.md)
