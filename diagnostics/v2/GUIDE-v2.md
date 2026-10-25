# GPU-DIAG v2

第二版補收每張顯示卡完整 256-byte PCI 設定、標準能力鏈，以及文字主控台與 GPU 的公開連接關係。

1. 儲存工作，從 Windows 開始功能表選「重新啟動」。
2. 開機連按 F11，選 UEFI USB；確認畫面開頭是 GPU-DIAG v2。
3. 等 USB_LOG=SAVED，按任意鍵返回韌體。
4. 選原本的 Windows Boot Manager，回到 Windows 後保持 USB 插著。
5. 新紀錄存於 GPU-DIAG/RUNxxxx.TXT，既有紀錄不會覆寫。

本次不用拔電源或修改 BIOS。程式只讀 PCI configuration 與 UEFI metadata，不存取 GPU MMIO/PROM、不啟用 Memory Decode、不做 reset、driver detach、解鎖或 NVRAM/BCD 修改。唯一寫入是受 USB 路徑與 ALLOW-LOG 標記限制的診斷紀錄。

Build.ps1 / Test.ps1 驗證 x64 EFI、零 imports、重定位、無 WX section 與 25 個 host 情境；host 測試不等於真實 UEFI 驗證。Read-Run.ps1 將原始紀錄轉成 JSON，執行時需明確指定本機 OutputDirectory。

主控台 path 或 ownership edge 不公開時，結果保持未知。這版仍不收 live PROM、fuse、selectors 或 WPR，也不證明解鎖所需的冷啟動狀態。它不是解鎖程式。

舊版 EFI 與 USB 說明檔備份於 GPU-DIAG/BACKUP-v1；第一次 RUN0000.TXT 保留原位。MIT 與 EDK2 授權見 LICENSE.txt 和 EDK2-LICENSE.txt。
