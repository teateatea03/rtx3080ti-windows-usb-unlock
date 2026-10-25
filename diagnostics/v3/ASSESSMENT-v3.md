# v3 bounded diagnostic assessment

Target: one PNY 10de:2208 / 196e:1384 at 0000:04:00.0, BAR0 ed000000..edffffff (16 MiB), with the observed PM/MSI/PCIe/vendor chain. Console must remain uniquely connected to the 19da:1762 RTX5080 at bus 8 through the public physical-GOP/SimpleTextOut/aggregate-console chain. Reject target BY_DRIVER/BY_CHILD ownership, unexpected Command bits, unverified USB, changed state after the USB callback, wrong resources or ambiguous identities.

The only configuration write call is a guarded 16-bit Command write at offset 4. Allowed values are 2 and 0. The initial state must be 0; Bus Master is never enabled. The neighbouring W1C Status word is never written. Successful enable must read back exactly 2 before any MMIO. Even an enable write reporting failure triggers cleanup, because it may have taken effect.

Only 12 register DWORDs are allowed: 0,1183a4,118128,118234,110100,823804,82381c,823820,1fa824,1fa828,824148,824160. The chip sample must identify GA102 before further reads. The other allowed access is exactly 256 x 4 KiB pages covering BAR0+300000..3fffff. No arbitrary MMIO, MMIO writes, firmware execution, DMA, reset, selector changes, driver detach, chainload or NVRAM/BCD mutation is implemented.

The transaction buffers all results in RAM. Between enable and restore it performs no filesystem or console I/O, pool allocation, event wait, polling loop or firmware discovery. PCI/MMIO protocol calls may still fail or hang in actual firmware. At most two Command=0 restore attempts, each followed by a readback, are made. An unverified restore sets a fail-stop flag and the actual EFI entry stays in the diagnostic instead of returning to firmware. A host fixture substitutes a finite return for that final permanent stop. Successful restore is followed by a complete 256-byte comparison; mismatches remain a failure.

PRE*.TXT must be written, flushed and closed before enabling. PROM is saved only after the whole 1 MiB read completes and Command restoration is verified. Final RUN*.TXT, PRE*.TXT and PRM*.BIN each preserve existing numbered files. Both the initial report and final artifacts are confined to the loaded USB filesystem and exact marker policy.

Host tests execute the same EFI entry using protocol mocks and the actual RUN0001 configuration fixture. They verify guards, widths/ranges, no writes to the 5080, preservation of W1C Status, USB failure containment, enable failures, register/PROM failures, partial capture rejection, restoration, bounded retries and fail-stop. Test.ps1 also checks x64 PE32+, imports=0, relocations and no WX sections, and audits all source callsites. These are preparation checks, not proof of real firmware access/restoration or unlock support.

The first v3 run requires a full shutdown and physical GPU power removal to collect a cold-power sample. The program cannot independently prove this physical procedure. It never performs the actual unlock. Existing v1/v2 sources, binaries and RUN0000/1 evidence are preserved.

Reference addresses only (no upstream unlock runtime compiled):
- https://github.com/impacifist/Windows-GPU-UNLOCK/blob/e3f03cd9389195adcabda4743d8fc2922b32498c/internal-boot/unlock/app.c
- https://github.com/impacifist/Windows-GPU-UNLOCK/blob/e3f03cd9389195adcabda4743d8fc2922b32498c/internal-boot/unlock/vendor/gfw_eval.h
- https://github.com/impacifist/Windows-GPU-UNLOCK/blob/e3f03cd9389195adcabda4743d8fc2922b32498c/internal-boot/unlock/vendor/prep_610.h
- https://github.com/impacifist/Windows-GPU-UNLOCK/blob/e3f03cd9389195adcabda4743d8fc2922b32498c/internal-boot/unlock/vendor/unlock_610.c
