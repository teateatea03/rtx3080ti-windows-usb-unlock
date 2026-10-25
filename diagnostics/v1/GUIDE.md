# GPU UEFI read-only inventory

獨立的 x64 UEFI 診斷工具，用於 ASRock B550 Steel Legend / P3.90、Ryzen 7 5700X、RTX 3080 Ti 12GB + RTX 5080 的移植前盤點。

只列舉 UEFI 已公開的 PCI、BAR 資源描述、GOP、控制台與 protocol ownership。沒有解鎖程式、PCI/MMIO 寫入、GPU ROM/MMIO 讀取、驅動釋放、GPU reset、NVRAM/BCD 變更或 Windows chainload。BAR 只讀既有配置與資源描述，不進行 BAR sizing。

畫面輸出使用韌體既有文字控制台，不切換解析度。唯一檔案寫入是啟動 USB 上 `GPU-DIAG/RUNxxxx.TXT`：必須同時確認啟動裝置路徑含 USB 節點，且同一磁碟已有 `GPU-DIAG/ALLOW-LOG.TXT`，內容恰為 `GPU-DIAG-USB-LOG-v1`。沒有這些條件時只顯示畫面，不寫入任何磁碟。既有紀錄不覆寫。

## 建置

執行 `Fetch-Headers.ps1` 下載固定版本的 EDK2 宣告檔並核對 Git blob 雜湊，再執行 `Build.ps1`。工具只編譯本資料夾 `src/diagnostic.c`，不下載或編譯任何解鎖核心或 NVIDIA 韌體。使用本機 MSVC x64，沒有新增系統套件。

`Test.ps1` 執行 host fixtures 與 PE 結構檢查。這些檢查不能證明實機 UEFI 可開機或硬體相容性。

## 實機執行（需要人員操作）

1. 先存檔。準備一支可供測試的 FAT32 USB；目前尚未偵測到 USB 磁碟。不要格式化系統磁碟，也不要修改 Windows EFI 分割區。
2. 將 `bundle` 內的資料夾複製到 USB 根目錄。若 USB 原本已有 `EFI/BOOT/BOOTX64.EFI`，請改用另一支 USB，保留原檔。
3. 使用主板的一次性 UEFI 開機選單選擇該 USB；不修改預設開機順序。工具只做盤點。
4. 完成後按任意鍵返回韌體選單，再選擇原有 Windows Boot Manager。它不自動重啟或啟動其他程式。
5. 回到 Windows 後保留 USB 的 `GPU-DIAG/RUNxxxx.TXT`，供下一步分析。若只看到 `USB_LOG=DISABLED`，表示未能證明啟動 USB／標記條件；不代表硬體不支援。

這次盤點仍不會取得 PROM digest、冷啟動 selector、WPR 配置或驅動程式碼指紋。因此結果只能支持下一階段的診斷設計，不能授權或證明 write-enabled 解鎖可執行。

## 參考來源

- [上游移植設計](https://github.com/impacifist/Windows-GPU-UNLOCK/blob/9352abd27480203ea146f766e7c96957912d148a/docs/PORTABILITY.en.md)
- [上游診斷原始碼，僅供閱讀比較，未連結進本工具](https://github.com/jingx8885/Windows-GPU-UNLOCK/blob/80a6203e0d1ef093027fb3cc77a817b21f9dbd11/internal-boot/boot/probe.c)

EDK2 headers retain their BSD-2-Clause-Patent license in `vendor/edk2/License.txt`. The new diagnostic source is MIT licensed, see LICENSE.txt.
