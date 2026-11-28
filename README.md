# HD2 Dark Fluid Port

把 HELLDIVERS 2 在 2024 年默里迪亞的「部署暗流體」任務（#93）移植到 HellForge（遊戲構建 25480438）。移植來源是 2024 年最後一個默里迪亞版本 1.001.104。「關閉 TCS」（#92）還沒做。

## v1.0（mod 1.0.14）

- 1、3 級可以完整打完：部署 ×1、投放點 ×3、地殼鑽頭、撤離。4 級以上的降落崩潰已修正，8～10 級還在測試。
- 印模周圍的紫色地表已修正，原因是地表層的編號被重新排過。
- 還沒做：部署完成的爆炸特效、暗流體音效、尖嘯蟲大波次。
- 安裝與玩法：[`docs/INSTALL.md`](docs/INSTALL.md)。mod 原始碼在 `darkfluid-port/`。資源包是從遊戲資料轉出來的，不在本庫，重建方法見交接文件。

**要一起修的人請先讀 [`docs/HANDOFF.md`](docs/HANDOFF.md)**：已確認的移植項目、執行邏輯、版本間的欄位對照、如何重建本庫沒附的檔案，以及還沒解決的問題。

## 內容

| 資料夾 | 說明 |
|---|---|
| `darkfluid-port/` | 主 mod（HellForge SDK 16）：印模登記、任務組替換、鑽頭戰備、目標開放與部署推進、降落崩潰修正、診斷 |
| `tools/` | `mdstack.py`：讀 HellForge 崩潰傾印的呼叫鏈 |
| `darkfluid-probe/` | 早期的探測 mod |
| `mission-groups/` | 任務組相關的小 mod |
| `analysis/` | 分析與轉換腳本（Python）、分析文件（`FULL-MISSION-ANALYSIS.md`、`FULL-PORT-CHECKLIST.md`、`FINDINGS-20240601.md` 等） |
| `old-build/` | 用 DepotDownloader 下載 2024-06-01 舊版的腳本和檔案清單 |
| `compat-test/` | 資源轉換相容性測試報告 |
| `docs/research-log.md` | 研究紀錄（逐版測試結果與發現） |

## 沒有放進來的東西

這些是遊戲本身的檔案或由遊戲檔案轉出來的資料，體積大，也不適合上傳；都能用 `analysis/` 裡的腳本從遊戲和舊版重新產生：

- 遊戲 `game.dll` 記憶體映像（`analysis/image/`）、資源索引（`index_25480438.json`）、遊戲文字（`strings-25480438/`）
- 轉換後的資源補丁（`zz.darkfluid-resources`：`d87885c7eaace597.patch_0` 等；`port_lgs.py` 產生的 `9ba626afa44a3aa3.patch_0`）
- 解密後的 2024 與現行 `.dl_bin` 設定檔
- 編譯出的 DLL
