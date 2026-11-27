# HD2 Dark Fluid & TCS Port（備份）

把 HELLDIVERS 2 在 2024 年的「部署暗流體」（任務 #93）與「關閉 TCS」（任務 #92）移植到 HellForge（遊戲構建 25480438）的工作備份。

**要一起修的人請先讀 [`docs/HANDOFF.md`](docs/HANDOFF.md)**：已確認的移植項目、2024→現行的欄位對照、如何重建本庫沒附的檔案、還沒解決的問題。

## 內容

| 資料夾 | 說明 |
|---|---|
| `darkfluid-port/` | 主 mod（HellForge SDK 16）：印模登記項目、目標類型、任務組替換、鑽機戰備、診斷與 2024 設定檔解密 |
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
