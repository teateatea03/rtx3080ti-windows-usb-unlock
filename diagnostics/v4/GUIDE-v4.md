# GPU-DIAG v4：PNY RTX 3080 Ti 限定診斷

v3 誤將 0x824160 當成 FWSEC 版本欄位；v4 改讀上游實際使用的 GSP fuse 0x8241e0。先前的 FWSEC version=1 結論撤回，須以本次實測為準。此外只查詢 PCI Attributes 的 Supported/Get，確認韌體快取狀態與 Command=0 相符。

這版會辨識唯一 10de:2208 / 196e:1384、BDF 0000:04:00.0 與 BAR0 0xed000000，確認 RTX 5080 的主控台連接關係、目標無 driver/child ownership、Command=0，並先寫入/flush USB 診斷前紀錄。

接著只將目標卡 16-bit PCI Command 的 Memory Space bit 暫時從 0 改為 1 (Command 0→2)。Bus Master 始終不啟用。讀取白名單的 12 個 DWORD 和 BAR0+0x300000 起的 1 MiB PROM 後，還原 Command=0 並讀回確認；所有可恢復錯誤也會執行還原。沒有 payload、GPU MMIO write、刷寫、reset、driver detach、BCD 或 NVRAM 修改。

## 操作

1. 儲存工作，按住 Shift 再選 Windows「關機」（避開快速啟動）。完全關機後關閉電源供應器/拔除電源，等待約 10 秒，再接回開機。這次要觀察冷啟動狀態。
2. 開機連按 F11，選 UEFI USB，確認 GPU-DIAG v4。
3. 等待完成。正常會顯示 USB_LOG=SAVED；紀錄內還須有 PROBE_RESTORE_VERIFIED=1 與成功的 PROBE_STATUS，回到 Windows 後會自動解析確認。
4. 按任意鍵返回韌體，選原本 Windows Boot Manager。USB 保持插著。
5. 若顯示 RESTORE=FAILED / STOP，請直接關機，勿選 Windows；回報這個訊息。程式會停留，不自動返回開機流程。
6. 若畫面卡住超過約一分鐘，關機並回報；不連續重試。UEFI protocol 卡住不能由 host 測試保證可中斷，正常錯誤回傳才會走還原路徑。

## 檔案與結果

- PRE0000.TXT：操作前紀錄（不應當作完整 RUN 報告）。
- PRM0000.BIN：完整讀取且確認還原後才保存的 1 MiB PROM。
- RUNxxxx.TXT：最終診斷報告。各命名空間都不覆寫既有檔案。
- 目標、顯示或 USB 條件不符時，PROBE_REFUSED / 非零 PROBE_STATUS 表示未完成資料取得，不代表硬體支援或解鎖失敗。
- 這是資料取得程式，不執行解鎖。Host / PE 驗證不是本機硬體還原保證；第一次 v4 實測仍待完成。

v1/v2/v3 與前三次紀錄全部保留；v3 USB payload 備份於 GPU-DIAG/BACKUP-v3。授權見 LICENSE.txt 與 EDK2-LICENSE.txt。
