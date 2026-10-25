# Guarded manual transaction

`src/app.c` and `pny-policy.h` own profile/display/cold-state validation; `app_io.c` owns hash-checked local files and per-run reporting; `port.c` mediates checked PCI/MMIO/DMA; `vendor/unlock_610.c` retains the upstream firmware sequence; `ownership.c` tracks resource release. The default build cannot arm writes.

~~~mermaid
flowchart TD
  A[One-time UEFI USB selection] --> B[Profile, USB log, display ownership and config guards]
  B --> C[Temporary Attributes MEMORY and cold-state/assets checks]
  C --> D{Manual U once?}
  D -->|Esc or timeout| E[Restore and return]
  D -->|Yes| F[Recheck after logging, then checked DMA and firmware sequence]
  F --> G[SS0 write, SS1 preserve, FLR and quiescence]
  G --> H{Cleanup and restore proven?}
  H -->|Yes| I[Return to firmware; manually choose Windows same boot]
  H -->|No| J[STOP: physical power removal]
~~~

Rejections must preserve error status even when an API partially changes hardware. Selector/display/identity checks are repeated at critical boundaries; there is no permission to relatch a changed stock state. Retained DMA resources or unverified cleanup stop the boot path. Public host tests exercise these branches but cannot establish hardware authentication or recovery from every protocol hang.

The independent `diagnostics/v4` does not call the unlock core. It uses a tightly bounded temporary MSE wrapper for whitelisted reads, then verifies the original full config. v1–v3 remain historical references.

Explicit dynamic mode logs fuse/related registers, resamples eleven cold registers without file callbacks, then invokes the existing immutable latch. The original mode keeps its two-value filter. [Details](DYNAMIC-SS0.md).
