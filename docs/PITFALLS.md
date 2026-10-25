# Pitfalls and corrections

Historical v1–v3 preserve the development record; use v4 for new diagnostics. Original failed captures remain private. [繁體中文](PITFALLS.zh-TW.md)

| # | Mistake / failure | Correction and limit |
| --- | --- | --- |
| 1 | v3 called SEC2 slot 9 at `0x824160` GSP FWSEC and inferred version 1. | Formula `0x8241c0 + (ucode_id-1)*4` gives `0x8241e0` for ID 9. Actual raw 3 selects version 2, offset 768, absolute PROM offset 282852. Version-1 conclusion withdrawn. |
| 2 | Stock SS0 varied: `24321116` / `24321156`, bit `0x40`. | Accept only observed entries, latch once; reject any mid-invocation change, even between allowed values. Cause unknown. |
| 3 | Command=0 blocks MMIO; raw writes disagree with Attributes cache. | Diagnostic bounded 16-bit MSE enable, restore 0, compare all 256 bytes. Unlock uses Attributes MEMORY before BME; Supported=`0x13763`, Get=0 measured twice. Partial-effect API errors still require cleanup and remain errors. |
| 4 | Separate 5080 physical GOP/text → aggregate console, CPU has no iGPU. | Validate/latch ownership; target must have no driver/GOP. No generic/headless/detach shortcut. |
| 5 | 12GB differs from 24GB reference. | Port measured geometry, BARs, cold-state and display policy. PCI ID alone is insufficient. |
| 6 | GPU-Z export is NVGI/IFR v3, not 55AA at zero. | Directory points to chain at `0x9200`; live PROM starts at 55AA. Normalize chains, not whole blobs/first MiB. Keep ROM private. |
| 7 | PROM n−1 truncation passed. | It removed padding. Truncate inside FWSEC; test short header, corrupt signature and immutable input. |
| 8 | First 115 cases assumed fuse 1. | Corrected 119 use actual raw 3, both SS0 entries/cross-mutations. Simulation does not authenticate firmware or prove hardware. |
| 9 | Log callbacks can change state after validation. | Final config/cold/display checks after ARMED log; before BME; selector checks at reset/DMA boundaries. |
| 10 | HALTED alone treated as enough. | Require execution progress, scratch=0, WPR geometry, later reset and cleanup. |
| 11 | Pre-reset timeout confused with cleanup failure. | Specific upstream tolerance for HW bug 3419321 is not a general timeout/restore-error waiver. |
| 12 | Event StartTime query included historical ID14/153. | All 15 before 13:46 boot, latest 10:21. XML UTC, explicit local filtering and last-200 System query showed zero new events in bounded observation. Convert OrderedDictionary to PSCustomObject for sorting. No endurance claim. |
| 13 | Warm reboot + skip USB assumed stock reset. | Full shutdown, PSU off/unplug ~10 seconds; boot directly. A measured cold-restore A/B round-trip is still uncompleted. |
| 14 | Automatic installation or continuing after failed cleanup. | F11 USB → one U → same-boot manual Windows. No BCD/NVRAM. STOP/unproved cleanup requires power removal, not blind retries. Optional USB does not remove execution risk. |
| 15 | Universal 2×, games, FP8 or LLM claims. | Report FP16 +91.72%, BF16 +63.68% for all-ones 4096³ FP32-accumulating GEMM. No core/VRAM increase, application or long-term claim. |

Public tools remove paths, UUID and USB serial dependencies. Parameterized benchmark source differs from the original tested script; data retain its original hash. RUN00 is LF-normalized; its public hash is not the private original hash.

A clean checkout also caught line-ending drift: Git LF normalization invalidated diagnostic vendor-manifest hashes of original CRLF bytes. .gitattributes now explicitly checks out those vendor files as CRLF; verification is retained. Diagnostic tests are rerun from a clean checkout.

[References](REFERENCES.md) · [Reproduction](REPRODUCE.md)
