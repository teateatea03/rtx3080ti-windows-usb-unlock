# v2 assessment

This isolated diagnostic adds complete 256-byte PCI configuration snapshots for display devices, bounded conventional capability decoding, and SimpleTextOut paths/ownership evidence. The parent directory preserves v1 and its actual UEFI capture.

It uses Pci.Read and UEFI metadata queries. It never enables Memory Decode, reads GPU MMIO/PROM, submits firmware, resets a GPU, disconnects a driver, chainloads an OS or changes firmware variables. Filesystem writes remain confined to a marker-authorized loaded USB volume.

25 host scenarios execute the actual EFI entry with fake protocols. They cover the existing USB containment/failure cases and new configuration/capability/console cases. Test.ps1 also checks native PE structure and scans all diagnostic source files for forbidden calls. This is not hardware validation.

Runtime cold-state/fuse/PROM data and a target-specific unlock profile remain unresolved. The console graph records only publicly exposed evidence and makes no handoff safety inference. This image cannot unlock a GPU.

The user selects the USB with the ASRock F11 one-time boot menu and returns to the existing Windows Boot Manager. No default boot configuration is changed.
