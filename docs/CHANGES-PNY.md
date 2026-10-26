# PNY port changes, 2026-10-25

Direct base: impacifist/Windows-GPU-UNLOCK `e3f03cd9389195adcabda4743d8fc2922b32498c`, `internal-boot/unlock/`. Original notices and copyrights retained; no claim to discovery.

- Add PNY 12GB/B550/5080 profile with measured config/BAR/PROM/cold-state guards.
- Validate/latch separate-display physical GOP/text and aggregate console ownership; no detach/headless fallback.
- Temporary EFI Attributes memory decode, restoration checks, partial API effect containment.
- Latch either observed SS0 once, preserve SS1=7, revalidate after logging and at reset/DMA boundaries.
- Correct FWSEC ID9 fuse address to 0x8241e0/version2.
- 119 simulated app/core scenarios, compile restrictions, retained generic regressions.
- Independent MIT diagnostic v1–v4; historical v3 fuse conclusion withdrawn.
- Portable build/test/extraction/package tools, fixed-input benchmark, bilingual method/results/pitfalls.
- Exclude firmware, PROM, private identifiers and build products.

Modified C/H carry dated notices. Same-line notice preserves runtime diagnostic line numbers; the experimental EFI remains byte-identical. Vendor code retains earlier modification notices. IMPORT-MANIFEST records local input, initial export and final public hashes; input hashes are not presented as pristine upstream hashes.

## v0.2.0-experimental

Add explicit GU_PNY_DYNAMIC_ENTRY_SS0: validated per-invocation SS0 latch, independent raw fuse/strap logs and callback-free cold resampling, retaining exact identity/PROM/display/SS1/cleanup checks. Original mode stays byte-identical. 221 dynamic/122 original app/core simulations and 32 compile selections. Preserve RUN01–03 refusals and RUN04 success. Matching build/test/package/copy options and CI hashes added; no firmware redistribution or persistent boot installation.

## Unreleased hardening

Borrow the service-boundary lesson from impacifist/Windows-GPU-UNLOCK main `849f99637a0e324161ad6fe12d77f7b668822512`: bind each BAR resource query to the same target handle, PciIo interface, BDF and complete 256-byte PCI snapshot before the query, after its allocation callback and after buffer release. Propagate a resource-buffer release failure instead of silently continuing. Four PNY host cases reject return-time config drift, free-time config drift, interface replacement and free failure before memory decode or MMIO.

The upstream headless USB wrapper and direct Windows handoff are not copied into this dual-GPU profile. They release a target-owned display stack and pin a Windows ESP for different exact machines. This profile deliberately keeps the measured one-time F11 USB → U → firmware menu → Windows path until a separate wrapper has its own build, recovery and hardware evidence.

Rebuilt 2026-10-26 candidates all pass PE validation. Dynamic SS0 `b04ce9ff5b484c4dd069778ed8102199fbced2db977bef745041793632990844` (67,072 bytes) subsequently completed RUN05 on the physical GPU: both BAR service-boundary rechecks passed, SS0 `24121116→88888888`, SS1 remained `7`, one FLR, clean containment and matched Windows matrix throughput. Fixed measured SS0 `4fd4143b57acbdbd68e57a3b792bcebcf73f5b48612ea788b47347338c9b1167` (67,072 bytes) and diagnostic-only/no-write `8ba3008b38db05f6b69d8cf4ffddba24aad24121ce3be0b8e0251057c880e3ab` (55,808 bytes) remain hardware-untested.
