# v0.2 dynamic SS0 / 動態初值修正

The original two-value SS0 filter rejected later boots. Explicit `GU_PNY_DYNAMIC_ENTRY_SS0` uses the upstream-style immutable invocation latch while retaining the exact PNY identity/PROM/display/cold-state/fuse/preserve-SS1 requirements. Default non-writing and original experimental mode remain unchanged.

## Evidence

| Run | Initial SS0 | Outcome |
| --- | --- | --- |
| RUN00 | 24321116 | Original transaction succeeded |
| RUN01 | 14131116 | Original rejected, attempted=0, clean=1 |
| RUN02 | 24121117 | Original rejected, attempted=0, clean=1 |
| RUN03 | 24121116 | Original rejected, attempted=0, clean=1 |
| RUN04 | 24321117 | Dynamic transaction succeeded, attempted=1, clean=1 |

[RUN01](../evidence/rejected-run01.txt) · [RUN02](../evidence/rejected-run02.txt) · [RUN03](../evidence/rejected-run03.txt) · [RUN04](../evidence/unlock-run04.txt) · [benchmark samples](../evidence/benchmark-run04.json)

Initial PCI config DWORDs matched and PROM checks passed. The physical cause of between-boot SS0 changes is unknown. A full Windows boot does not establish physical GPU power removal.

The original refusal short-circuited before fuse/strap reads. Dynamic mode independently reads/logs FWSEC[8241e0]=3 and register[824148]=1 before U/latching, then resamples eleven cold registers without log callbacks. Invalid/unreadable values, badfxxxx, already-unlocked 88888888, wrong SS1 and later selector changes reject or stop with containment. Zero is accepted by the upstream-style filter and tested in simulation only, not on this hardware. Readable SS0 alone is never sufficient.

RUN04: FWSEC scratch=0; SS0=88888888, SS1=7 without writes, one FLR, quiesced=1/fault=0/retained=0/maps=0, original Attributes and Command=0 restored. Private original report SHA-256: `be4094096996c57cba5f7b3dca54cf4291093ab3ef59b949a7b76870a8f464e6`. Public log is LF-normalized.

Same original GPU/script/cuBLAS/parameters were privately matched. FP16=113.321285 and BF16=113.168405 TFLOPS: +89.76% / +61.86% against the original stock baseline. Each dtype's 16,777,216 constant-input outputs equalled 4096. Both PNP displays OK; no new Display/nvlddmkm/WHEA events from 21:24:41 to 21:27:42 Taipei (~3 minutes). Health is a maintainer observation, private raw snapshots excluded.

## Reproduce explicitly

First complete the [full guide](REPRODUCE.md), including local private PROM/v4 diagnostics and five hash-pinned assets.

~~~powershell
.\Test.ps1 -Prom .\.local\captures\PRM0001.BIN -DynamicEntrySs0
.\Build.ps1 -Experimental -DynamicEntrySs0
python tools/package_local.py --dynamic-entry-ss0 --assets .local/firmware/assets --diagnostic-summary .local/assessment/RUN0003.TXT.summary.json --output .local/usb-dynamic
.\tools\Copy-Usb.ps1 -DriveLetter X -Bundle .\.local\usb-dynamic -DynamicEntrySs0
~~~

X must be the actual USB. Copy defaults to dry run; explicit `-Apply` copies files. Original mode omits the dynamic flag from all four steps. The public copier refuses an existing unlock directory; preserve reports and use another USB. The hardware-tested USB update used a separate private staging script with full payload backups; do not delete logs to bypass the public copier.

225 dynamic / 126 fixed app/core simulations, default rejection, 32 compile selections and nine generic regression groups pass locally; CI has no private PROM/GPU. Current `main` produces the hardware-untested hardened dynamic candidate (67,072 bytes), SHA-256 `b04ce9ff5b484c4dd069778ed8102199fbced2db977bef745041793632990844`. The tagged v0.2 RUN04 EFI remains `0cf96d7ce5b81b87caf463b6ef4a2365b4785fb6d64273ee38a7d9f699e96845` and is the one with hardware evidence.

## 繁體中文摘要與限制

舊版兩值清單導致 RUN01–03 在解鎖前被拒絕。v0.2 需明確啟用動態模式：先驗卡片、PROM、冷狀態、fuse/相關寄存器與顯示路徑，重讀後才把當次初值記在 RAM；當次不得重新學習。這次 RUN04 初值又變為 24321117，但完整檢查後成功，SS1 保持7、清理完成，Windows GEMM約113 TFLOPS。

這是動態版**一次實機成功**，不是所有初值、每次冷啟動、休眠喚醒、長時間穩定、遊戲或LLM加速保證。初值為何變動仍未知。沒有永久開機安裝；回復USB不等於回復GPU電源狀態。

上游方法：[entry latch](https://github.com/impacifist/Windows-GPU-UNLOCK/blob/e3f03cd9389195adcabda4743d8fc2922b32498c/internal-boot/unlock/port.c#L15)、
[cold checks](https://github.com/impacifist/Windows-GPU-UNLOCK/blob/e3f03cd9389195adcabda4743d8fc2922b32498c/internal-boot/unlock/app.c#L262)。
保留 [引用](REFERENCES.md) 與 [授權](../THIRD_PARTY_NOTICES.md)。不包含NVIDIA韌體、ROM/PROM或私人GPU識別。
