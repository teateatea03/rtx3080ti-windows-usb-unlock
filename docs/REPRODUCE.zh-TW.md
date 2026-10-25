# 從原始碼重現整套流程

[English 詳細參數](REPRODUCE.md) · [踩坑](PITFALLS.zh-TW.md) · [程式架構](ARCHITECTURE.md)

先核對 [README 硬體條件](../README.zh-TW.md)：同型號不等於符合 BDF、BAR、顯示拓樸與冷狀態。未簽章程式需關閉 Secure Boot；改韌體設定前請把原設定與可能用到的 BitLocker 復原金鑰留在自己手上，不能上傳。

## 1. 建置工具

Windows x64、Python3.12+、Clang/LLD **20.1.8**、Visual Studio C++ Build Tools 與 Windows SDK、PowerShell5.1/7。[固定版本與來源](REFERENCES.md)。

下載官方 LLVM-20.1.8-win64.exe，核對 SHA-256 `3197846a2b19063687dd56e93e34cd941e3548d907f23a6131571321bdf9fe7b`，可用 7-Zip 解壓而不安裝。bin 加到本次 PATH，或明確傳 `-Clang`／`-LldLink`。測試透過 vswhere 找 C++ 工具，可用 `-VcVars` 指定 vcvars64.bat。腳本不改系統 execution policy。

在 repo 根目錄執行：

~~~powershell
python --version
clang --version
lld-link --version
.\Test.ps1
.\Test-Diagnostic.ps1
.\Build.ps1
~~~

這些只在主機編譯／模擬，不會碰 GPU。第一個跑 9 組回歸與 23 組編譯限制，未提供 `-Prom` 會明確略過 119 情境與 fixture 預設模式測試。第二個建置獨立診斷 v4、跑 60 情境與 5 種錯誤報告。預設 Build 產物是 `src/build/pny3080ti/diagnostic-only/PNY3080TI.EFI`，無法 arm GPU 寫入／DMA／reset；它與獨立診斷 v4 不同，執行時仍需相同 app 資料目錄。

## 2. 診斷 v4 與私人擷取

使用非系統碟的既有 FAT32 USB，先備份內容及 BOOTX64.EFI，再複製 `diagnostics/v4/bundle/` 內的兩個目錄與說明：`EFI/BOOT/BOOTX64.EFI`、`GPU-DIAG/ALLOW-LOG.TXT`。不要格式化或改開機項。

冷斷電後 F11 選 UEFI USB。v4 收 PCI、顯示 ownership、白名單 MMIO 與 1MiB live PROM；**會暫開 memory decode 並回復 Command=0，並非完全唯讀**。不開 BME、不執行 GPU 韌體。回復無法驗證就關機斷電。

回 Windows 後把對應 RUNxxxx.TXT、PRMxxxx.BIN、PRExxxx.TXT（如有）放一起，留在 `.local/captures/`。以下編號是本案範例，請用自己實際編號：

~~~powershell
.\diagnostics\v4\Read-Run.ps1 -Path .\.local\captures\RUN0003.TXT -OutputDirectory .\.local\assessment
.\tools\prom\Inspect-Live.ps1 -Prom .\.local\captures\PRM0001.BIN
~~~

Read-Run 要同目錄 PROM 並驗大小／雜湊。核對 ProbeCaptureComplete、RestoreVerified、ConfigUnchanged、Attributes、所有 register 與顯示路徑；JSON 不證明真的有冷斷電。FWSEC `0x8241e0=3`，SS0 `0x82381c` 為 `24321116` 或 `24321156`，SS1 `0x823820=7`。不要用 v3 舊判讀。

選用 GPU-Z 匯出解析：
`.\tools\prom\Inspect-Rom.ps1 -RomPath .\.local\card.rom -OutputDirectory .\.local\rom-analysis`。
只解析本案 IFR v3 並正規化 chain，不刷寫、不證明簽章真偽。ROM/PROM 不能公開上傳。

## 3. 本機取得固定韌體

先讀 [授權邊界](../THIRD_PARTY_NOTICES.md) 與 [輸入清單](../tools/firmware/artifacts-610.43.03.json)。腳本僅下載、驗雜湊、讀選定 archive member，不跑驅動安裝或下載的程式碼。NVIDIA 韌體保留自己的授權。來源失效就停下，不換其他版本硬試。

~~~powershell
python -m venv .local/venv
.\.local\venv\Scripts\python.exe -m pip install -r requirements-firmware.txt
python tools/firmware/fetch_artifacts.py .local/inputs
.\.local\venv\Scripts\python.exe tools/firmware/prepare_windows_assets.py --inputs .local/inputs --work .local/firmware
~~~

`--work` 必須為新目錄；產物都留在忽略上傳的 .local。已有快取可用：
`python tools/firmware/fetch_artifacts.py .local/inputs --offline`。
核對 `src/assets.json` 中五資產與 NVIDIA-LICENSE。公開 repo／release 不含韌體。

## 4. 完整測試與實驗版

PROM 雜湊綁定本案實測卡，不符就調查，不要改雜湊強迫過關。

~~~powershell
.\Test.ps1 -Prom .\.local\captures\PRM0001.BIN
.\Build.ps1 -Experimental
~~~

119 個 app/core 情境仍是模擬，另驗預設拒絕寫入。實驗 EFI 應 66,048 bytes、SHA-256 `197602679e4574a5993e5ea6f963e128a7662ccaa1a382c2c2b29838e3a4bb08`。PE 檢查排除 imports、可寫又可執行區段；新編譯不代表新實機驗證。

## 5. 本機 USB 資料樹

封裝核對 v4 完整／回復、PROM、selector/fuse、119 測試紀錄、source/EFI/asset 雜湊。

~~~powershell
python tools/package_local.py --assets .local/firmware/assets --diagnostic-summary .local/assessment/RUN0003.TXT.summary.json --output .local/usb
.\tools\Copy-Usb.ps1 -DriveLetter X -Bundle .\.local\usb
~~~

X 換實際 USB 代號；第二行預設 **dry-run**，先確認磁碟與檔案。加 `-Apply` 才複製、備份既有 boot EFI、驗證並最後替換 BOOTX64.EFI。不接受非 USB、系統碟、非 FAT32、已有 PNY3080TI-UNLOCK 的 USB；保留舊 RUN，改用另一支。新版複製工具公開準備期間只做 dry-run／模擬檔案系統測試，沒實際重寫成功那支 USB；當初成功是用機器專用 staging script，方法在此整理。

無格式化、分割、BCD/NVRAM／開機順序變更；沒有執行解鎖。斷電或複製失敗仍可能留下不完整目錄，要保留備份。`.local/usb` 含第三方韌體，不能整包上傳。

若要執行預設不寫入 port，可用相同 app 資料樹，於另一支已備份 USB 手動放入獨立驗過的 diagnostic-only EFI。實驗版專用 Copy-Usb 會拒絕該不同雜湊。

## 6. 開機與 Windows 驗證

依 [USB 操作](USB-GUIDE.zh-TW.md)：完整關機／PSU 斷電約 10 秒 → F11 USB → U 一次 → 清理完成 → 同次開機手選 Windows，不能重啟。不自動 chainload、不安裝 BCD/NVRAM。

保留 REPORT，核對 FINAL、SS0=88888888、SS1=7、不寫 SS1、一次 FLR、retained/maps/fault=0、quiesced、Command/Attributes 還原。清理不確定就實體斷電。

Windows 驗兩卡 PNP OK，事件以本次開機 XML UTC 做程式端再篩選，另查最近 System 交叉驗證；不能只信 StartTime。這不是長期穩定證明。

匹配 benchmark 前先停止 GPU 工作，使用可信本機 CUDA12 cuBLAS DLL，明確提供私有 UUID 防選錯卡；輸出排除 UUID 與完整 DLL 路徑：

~~~powershell
python tools/benchmark.py --cublas "C:\CUDA\bin\cublas64_12.dll" --bus 0000:04:00.0 --expect-uuid YOUR_LOCAL_GPU_UUID --output .local/before.json
~~~

DLL 路徑是範例；前後同腳本／DLL／參數，用 before.json、after.json 不同檔名。build/test/package 不會啟動 benchmark。[結果與限制](RESULTS.md)：尚未完成冷斷電回復的完整量測 round-trip。
