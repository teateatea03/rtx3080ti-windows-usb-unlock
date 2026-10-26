# Manual USB, one attempt

Complete [reproduction](REPRODUCE.md), match the measured hardware, verify v4 restoration, EFI and five asset hashes first.

1. Save work. Full Windows shutdown (Shift + Shut down), PSU off/unplug ~10 seconds, reconnect.
2. On the measured ASRock B550 Steel Legend press **F11**, select **UEFI USB**. Other boards: consult manual. Expect PNY RTX3080Ti / B550 / RTX5080 display banner.
3. After checks and **Press U to attempt once**, press **U once**. Esc/30-second timeout cancels.
4. Keep USB and separate display connected; no reboot during execution.
5. Only after **UEFI transaction completed and cleaned up. Returning to firmware; Windows validation is pending.**, select existing **Windows Boot Manager** in the **same boot**. Do not restart or run USB again.
6. Preserve `PNY3080TI-UNLOCK/RUNxx/REPORT.TXT`. Verify FINAL, selectors, FLR, DMA cleanup, restored Command/Attributes, then Windows devices/events and matched benchmark. Banner alone is not acceleration proof.

Cancellation/rejection: inspect logs before retrying.

**STOP / POWER OFF / cleanup cannot be proven / restoration unverified**: physical shutdown and PSU off/unplug, do not continue to Windows. Also power off for no progress ~three minutes or display loss. Firmware protocol hangs may exceed software timeouts.

## Skip or restore

Full shutdown, PSU off/unplug ~10 seconds, boot Windows directly without USB selection. Warm reboot + skipping USB does not prove cleared state. A measured cold-restore round-trip is not yet completed. Keep prior USB files and logs backed up; changed hardware needs new diagnostics.

For dynamic mode expect **TRIAL DYNAMIC SS0**. Current `main` adds service-boundary hardening but retains this manual flow; its rebuilt EFI is a hardware-untested candidate, while RUN04 belongs to tagged v0.2. A rejection means preserve the report, not repeat blindly. [Policy](DYNAMIC-SS0.md).
