# Contributing

Issues/PRs welcome. Preserve notices and keep changes small. Do not generalize a profile by changing PCI IDs alone.

Include commit/EFI hash, card/subsystem, board BIOS, display topology, driver, failed stage and minimal redacted logs. Remove UUIDs, USB serials, hostnames, paths. Do not attach ROM/PROM, firmware or CUDA DLLs; retain originals privately.

Run Test.ps1, Test-Diagnostic.ps1, Build.ps1 per [reproduction](docs/REPRODUCE.md). Explicitly mark fixture-dependent skips. CI/simulation is not hardware validation. New hardware needs measured cold-state/ownership, negative cases and verified cleanup.

Never weaken fail-stop to reach Windows. Update both languages and pitfalls for corrected assumptions. No auto-merge or paid access.
