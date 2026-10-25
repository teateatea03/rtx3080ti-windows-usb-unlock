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
