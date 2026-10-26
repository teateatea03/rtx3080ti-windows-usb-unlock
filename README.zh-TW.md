# RTX 3080 Ti：Windows 手動 USB 單次解鎖

[English](README.md) · [完整重現步驟](docs/REPRODUCE.zh-TW.md) · [踩過的坑](docs/PITFALLS.zh-TW.md) · [實測結果](docs/RESULTS.md) · [來源引用](docs/REFERENCES.md)

這是我們在 **PNY RTX 3080 Ti 12GB＋ASRock B550 Steel Legend＋RTX 5080 負責顯示** 上完成的實驗，免費公開、GPL-2.0-only 開源。想使用時從開機選單選 USB，檢查通過後按 **U 一次**，完成後在**同一次開機**選 Windows Boot Manager；平常直接開 Windows 不會執行。

**v0.2.0-experimental 新增動態 SS0 初值鎖定**：完整驗證後記住當次初值，同一次執行若改變就拒絕。需明確加上 `-DynamicEntrySs0`；預設非寫入模式與原本兩值模式都保留。

**目前 `main` 另加入尚未發版的 service-boundary 強化**：BAR 查詢配置／釋放 callback 前後重新核對 target handle、PciIo、BDF 與完整 PCI snapshot。動態候選已完成 RUN05 實機驗證；固定 SS0 與 diagnostic-only 強化版仍未實機執行。

| FP32 累加／輸出 | 原廠基準 | 原版 RUN00 | 動態版 RUN04 | 強化版 RUN05 |
| --- | ---: | ---: | ---: | ---: |
| FP16 輸入 | 59.717 TFLOPS | 114.491 TFLOPS | 113.321 TFLOPS（1.898 倍） | 113.336 TFLOPS（1.898 倍） |
| BF16 輸入 | 69.918 TFLOPS | 114.442 TFLOPS | 113.168 TFLOPS（1.619 倍） | 113.407 TFLOPS（1.622 倍） |

目前同機有三次成功：原版 RUN00、動態版 RUN04、強化動態版 RUN05。RUN01–03 在解鎖前被拒絕；兩個動態二進位各有一次實機成功，仍未證明每種初值／每次啟動都可用。測試為全 1 輸入的 4096³ cuBLAS GEMM。每種資料型態 16,777,216 個輸出皆精確等於 4096；這不代表所有模型推論、遊戲或長時間工作都獲得同樣提升。

## 適用範圍

- PNY 3080 Ti：`10de:2208 / 196e:1384`，BDF `0000:04:00.0`、BAR0 `ed000000`，vBIOS `94.02.71.00.74`。
- B550 Steel Legend BIOS P3.90；Ryzen 5700X 沒有內顯，64GB RAM。
- RTX 5080 `10de:2c02 / 19da:1762` 在 bus 8 負責顯示，目標卡在 UEFI 必須未被驅動或 GOP 接管。
- Windows 11 build 26300、NVIDIA 617.42；未簽章程式實驗需關閉 Secure Boot。
- FWSEC raw=3/version=2，SS1=7 保留不寫；原版 SS0 只接受兩值；動態版逐次驗證並鎖定初值，當次變動仍拒絕。

**不是所有 3080 Ti 都能直接套用的通用工具。** 其他廠牌、插槽、BAR、顯示路徑與單卡配置未驗證；新聞中的 40／50 系支援不屬於本專案。條件不合就停下，不要只改 PCI ID 或刪掉檢查。

不刷 vBIOS、不改 Windows 驅動、不註冊 BCD/NVRAM、不改預設開機順序。實驗版仍執行 GPU 韌體、DMA 與重置，可能卡住或需要實體斷電；手動 USB 的優點是可選擇、少了常駐開機變更，不能保證硬體安全。

## 整套內容

1. [重現與建置](docs/REPRODUCE.zh-TW.md)：環境、診斷 v4、固定韌體取得、編譯、測試與 USB 準備。
2. [實際開機操作](docs/USB-GUIDE.zh-TW.md)：F11、U 一次、同次開機進 Windows、停止與回復流程。
3. [踩坑紀錄](docs/PITFALLS.zh-TW.md)：錯讀 fuse、SS0 變動、EFI Attributes、ROM 格式、事件誤判等。
4. [實測與限制](docs/RESULTS.md)、[原作者與固定版本](docs/REFERENCES.md)、[授權邊界](THIRD_PARTY_NOTICES.md)。

目前 `main` 的兩個候選皆為 67,072 bytes：固定 SS0 `4fd4143b57acbdbd68e57a3b792bcebcf73f5b48612ea788b47347338c9b1167` 尚未實機執行；動態 SS0 `b04ce9ff5b484c4dd069778ed8102199fbced2db977bef745041793632990844` 已完成 RUN05，UEFI 清理、Windows 兩卡狀態與匹配矩陣效能皆通過。[RUN05 證據與限制](docs/RESULTS.md#current-main-hardening-run05)。

本機通過動態版 225／固定版 126 個 app/core 模擬情境、預設非寫入案例、9 組通用回歸、32 組編譯限制、診斷 v4 的 60 個情境及 5 個錯誤報告拒絕測試。模擬不等於實機韌體驗證；CI 沒有顯卡與私有 PROM，會明確略過。

**不散布 NVIDIA 韌體、ROM/PROM、CUDA DLL、GPU UUID 或私有識別。** 韌體由腳本在本機取得並核對 SHA-256；release EFI 不是完整映像。原作者著作權與第三方授權保留，沒有付費牆；免費公開不代表可把第三方韌體改授權。

[參與貢獻](CONTRIBUTING.md) · [安全回報](SECURITY.md)

## v0.2 動態版

[新初值、拒絕紀錄與 RUN04 詳細結果](docs/DYNAMIC-SS0.md)。
`v0.2.0-experimental` 標籤重編譯與 RUN04 實測版完全相同，66,560 bytes：
`0cf96d7ce5b81b87caf463b6ef4a2365b4785fb6d64273ee38a7d9f699e96845`。
`-Experimental -DynamicEntrySs0` 選動態版；只有 `-Experimental` 仍是原版。
