# Measured results and evidence limits

See [hardware](../README.md). Exactly one hardware attempt RUN00 completed; publication rebuilt the same EFI and performed no new GPU attempt.

## UEFI

- FWSEC raw 3/version 2/signature offset 768; running → halted; scratch 0.
- SS0 became `88888888`, SS1 remained `7` without SS1 writes.
- One FLR; quiesced=1, fault=0, retained=0, maps=0.
- Original Attributes and Command=0 restored/read back; FINAL status=0, attempted=1, clean=1; no driver detach.
- [Public log](../evidence/unlock-run00.txt) is LF-normalized, with a different hash from the private original.
- Original report SHA-256: `98fc36c6b79a6161c18bbf3fdbf118681151ec17265587a539b7ceba46bdbb3d`.
- Successful/reproduced EFI, 66,048 bytes: `197602679e4574a5993e5ea6f963e128a7662ccaa1a382c2c2b29838e3a4bb08`.
- Default non-writing EFI, 54,784 bytes: `10c8e4a8dfdd3a90aba33cacda6eff635bb036e573d026e7378cfe01a2ee5f8d`.
- Private live PROM: `991cbe4878a4c6f0456b6412ffff3dab5e7f60ec317ba4539254eb52b692f140`.
- Reconstructable [PCI config](../evidence/pny-pci-config.json): `ec681e8f2b2f4cef6e6ec0e451b1d2814c31b6b2c32defd5bd65cc8747a5c1b2`.

Both Windows displays were PNP OK. Corrected XML/local-time filtering and an independent query found zero new Display/nvlddmkm/WHEA events in the bounded observation. Raw health captures remain private; this is a maintainer-reported observation, not independently replayable public health proof. See [pitfall 12](PITFALLS.md).

## Matched benchmark

[Before](../evidence/benchmark-before.json) · [After](../evidence/benchmark-after.json)

Same privately matched GPU identity/script/DLL/parameters: cuBLAS 12.8.4 (120804), DLL SHA-256 `9513540e4ec4c51ee9e7304138c2cc255c29a8c181f9e80c38efa25738becd99`; 4096³, 3 warmups, 3 samples ×20 iterations, CUDA events, computeType=68, mathMode=16 (disallow reduced-precision reduction), algorithm=99. FP16/BF16 inputs, FP32 output/accumulation. No power/clock changes.

| Input | Before TFLOPS | After TFLOPS | Ratio | Increase |
| --- | ---: | ---: | ---: | ---: |
| FP16 | 59.717350 | 114.490943 | 1.917214× | 91.72% |
| BF16 | 69.917815 | 114.442129 | 1.636809× | 63.68% |

All 16,777,216 outputs per type exactly 4096 for all-ones inputs. Not random-input numerical testing, LLM/LM Studio, gaming, training, FP8, core count or VRAM evidence.

Public data remove UUID, DLL path, device snapshots containing UUIDs, timestamp. Original script hash `0fcd0a81d59f69d91e444780228a7042a1a71996df70eddce1f92aeea562241d` remains. Public parameterized tool has different source bytes; its computation preserves parameters but was not rerun on GPU for publication.

No endurance test, cold-power restore A/B round-trip, other hardware/driver, general single-GPU or application validation has been completed. Simulations do not prove recovery from every real failure; firmware protocol hangs may exceed deadlines.
