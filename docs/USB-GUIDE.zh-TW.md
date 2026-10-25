# 手動 USB：想解鎖才執行

前提：[重現指南](REPRODUCE.zh-TW.md)完成，硬體完全匹配、v4 回復驗證通過、EFI 與五個資產雜湊正確。這不是單卡通用教學。

1. 儲存工作，按住 Shift 選 Windows「關機」，關機後關 PSU／拔電約 10 秒，再接回。
2. 本案 ASRock B550 Steel Legend 開機連按 **F11**，選 **UEFI USB**；其他主機板依手冊。應顯示 **PNY RTX3080Ti / B550 / RTX5080 display**。
3. 檢查通過且看到 **Press U to attempt once** 後，按 **U 一次**。Esc 或 30 秒不按會取消。
4. 保留 USB 與 5080 顯示連接，有進度就等候，不要執行中重啟。
5. 只有看到 **UEFI transaction completed and cleaned up. Returning to firmware; Windows validation is pending.**，才從韌體選原本 **Windows Boot Manager**。是同次開機，不要重啟或再跑 USB。
6. 保留 `PNY3080TI-UNLOCK/RUNxx/REPORT.TXT`，核對 FINAL、SS0/SS1、FLR、DMA 清理、Command/Attributes 回復，再驗 Windows 裝置、事件與匹配 benchmark。完成畫面本身不證明加速。

**Attempt cancelled or rejected**：不要連續重試，先讀紀錄。

**STOP / POWER OFF / cleanup cannot be proven / restoration unverified**：實體關機、關 PSU／拔電，先不要 Windows。無進度逾約三分鐘或失去畫面也如此處理；UEFI protocol 卡住可能無法被 timeout 中斷。

## 不需要解鎖／回復

完整關機、PSU 關閉／拔電約 10 秒，直接 Windows、跳過 USB。可拔除 USB，無須永久改開機順序。**暖重啟跳過 USB 不保證清除。** 證明恢復原廠狀態仍需量測；本案尚未完成完整回復 A/B round-trip。

每次建立新 RUNxx，保留舊紀錄；硬體變動先診斷。備份既有 USB 內容，我們的 v1–v4 與歷次證據皆保留。
