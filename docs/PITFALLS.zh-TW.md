# 踩過的坑：錯在哪、怎麼修、還不能下什麼結論

保留錯誤與修正，避免把後來才知道的資訊寫成一開始就成功。v1–v3 是歷史資料，新診斷使用 v4。原始失敗擷取與 ROM 留在私人工作目錄；公開可分享的原始碼、雜湊與摘要。

| # | 踩坑／症狀 | 修正與驗證 | 界線 |
| --- | --- | --- | --- |
| 1 | v3 把 `0x824160` 的 SEC2 slot 9 當作 GSP FWSEC，曾錯判 raw=1/version=1。 | 上游公式 `0x8241c0 + (ucode_id-1)*4`，ID=9 應讀 `0x8241e0`。v4 raw=3 → version=2 → 簽章偏移 768，PROM 絕對位置 282852。 | 撤回 version=1 結論；舊版僅供追查。 |
| 2 | 把兩個早期SS0值視為固定清單，RUN01–03後來出現新值而拒絕。 | v0.2明確啟用動態初值驗證／當次鎖定，RUN04以24321117成功，執行中仍不重鎖。 | [詳情](DYNAMIC-SS0.md)：原因未知，動態版僅一次實機成功。 |
| 3 | Command=0 無法直接 MMIO；直接改 Command 可能與 EFI Attributes 快取不一致。 | 診斷 v3/v4 只以 16-bit Command 暫開 MSE，回復 0 並比較 256 bytes。解鎖版用 Attributes MEMORY 再處理 BME；Supported=`0x13763`、Get=0 各驗兩次。 | API 部分生效仍須清理，不能用 readback 把錯誤改判成功。 |
| 4 | CPU 無內顯，5080 的實體 GOP/text 與 aggregate ConsoleOut/StdErr 是不同 handle。 | 驗證並鎖定 ownership／顯示拓樸，要求 3080 Ti 未被 driver/GOP 使用。 | 不套 generic/headless、detach 或單卡捷徑。 |
| 5 | 12GB 配置不同於 3090 24GB 參考。 | 依實測 BAR、VRAM、FWSEC、顯示路徑建立專用 policy。 | 只改 PCI ID 不足以移植。 |
| 6 | GPU-Z 匯出開頭 NVGI/IFR v3，不是 offset 0 的 `55AA`。 | 解析目錄與 PCI ROM chain；匯出 chain 從 `0x9200` 開始，live PROM 從 `55AA` 開始，比較正規化 chain。 | 不直接比較整份匯出／前 1MiB；不散布 ROM。 |
| 7 | Parser 的 `n-1` 截短未失敗。 | 原先只砍尾端 padding；改在 FWSEC 真正內容內截斷，同測短 header、損壞簽頭、輸入不變。 | 必須截到實際所需內容。 |
| 8 | 最初 115 情境假設 fuse=1，容易誤認已驗證硬體。 | 改實測 raw=3 的 119 情境，含兩種 SS0 和跨階段變更，明標 SIMULATED。 | 不證明硬體接受簽章或穩定。 |
| 9 | 外部 log callback 可改變狀態，先前檢查會過期。 | ARMED 記錄後重驗 config/cold/display；BME 前與 reset/DMA 邊界重驗 selector。 | 「只是記錄」也不能假設無副作用。 |
| 10 | 只看 Falcon HALTED 易誤判成功。 | 要有 running→halted、scratch=0、WPR 幾何、後續重置與清理。 | 單一 HALTED 不夠。 |
| 11 | Falcon pre-reset timeout 與 cleanup failure 混淆。 | 上游針對 HW bug 3419321 有特定容忍路徑，分清該情況與真正清理失敗。 | 不能通用忽略 timeout／回復失敗。 |
| 12 | Get-WinEvent 的 StartTime 查到舊事件，曾先誤報本次 NVIDIA 錯誤。 | 15 筆 ID14/153 都早於 13:46 開機，最新 10:21。核對 XML UTC、程式端再篩選、另查 System 最新 200 筆交叉驗證，觀察期 0 新事件。 | 非長期保證。OrderedDictionary 排序也曾不如預期，轉 PSCustomObject 再排序。 |
| 13 | 暖重啟跳過 USB 被當成已恢復。 | 完整關機、PSU 關閉／拔電約 10 秒，再直接 Windows；證明回復還需重讀狀態與匹配測試。 | 尚未做完冷斷電回復的完整 A/B round-trip。 |
| 14 | 自動開機安裝增加管理面；清理失敗還進 Windows 會擴大問題。 | F11 手選 USB → U 一次 → 同次手選 Windows，不改 BCD/NVRAM；STOP／清理無法證明就實體斷電。 | USB 可選不等於零風險，不盲目重試。 |
| 15 | GEMM 被寫成全部 2×，套到 FP8、遊戲、LM Studio。 | 分別報 FP16 +91.72%、BF16 +63.68%，全 1 GEMM、FP32 累加、逐元素核對。 | 未增加 CUDA core／VRAM，未測遊戲／FP8／LLM／長期穩定。 |

公開前移除專屬路徑、GPU UUID 與 USB 序號依賴。公開 benchmark 使用明確參數，檔案雜湊與原測試腳本不同；數據仍保留原腳本雜湊。RUN00 公開文字已 LF 正規化，不能冒充私人原檔的雜湊。

公開前的乾淨 checkout 也抓到換行問題：Git 正規化成 LF 後，診斷 vendor-manifest 原先對 CRLF 原始 bytes 的雜湊會不符。已用 .gitattributes 明確保留診斷 vendor 檔的 CRLF checkout；不是略過驗證。乾淨 checkout 會再跑診斷測試。

[English](PITFALLS.md) · [來源](REFERENCES.md) · [重現](REPRODUCE.zh-TW.md)

舊版SS0拒絕會短路跳過fuse／相關寄存器讀取，不能把失敗報告當作所有後續條件已通過。動態版記錄兩個raw值並在鎖定前重讀。
