# 協作交接：部署暗流體／關閉 TCS 移植

## 目標

把 HELLDIVERS 2 在 2024 年的兩個任務移植到 HellForge，遊戲構建 **25480438**：

- 「部署暗流體」（任務列 #93）
- 「關閉 TCS」（#92）

移植來源是 2024 年最後一個默里迪亞版本 **1.001.104**（2024-10-15），2024-06-01 版只拿來對照，目標是盡量一比一。

這份文件只寫兩種內容：**已經確認**的事（有實機日誌或檔案比對佐證），以及還沒解決的問題。推測的部分一律標成「推測」。

最後更新：2026-11-28，mod 版本 1.0.3。

---

## 0. 規則與限制

- **不改遊戲本體檔案。** 所有改動都只透過 HellForge：mod DLL，以及 `<bundle>.patch_<n>` 資源補丁。
- **不接管原版模型和印模。** 只新增。
- 只改 #93 這一個任務列。
- 安裝或建置前要先關遊戲，因為遊戲執行時 HellForge 會鎖住 mod 資料夾。

## 1. 環境

| 項目 | 內容 |
|---|---|
| HellForge | 0.5 |
| 主 mod | `darkfluid-port/`，id `local.darkfluid-port` |
| 資源包 | `zz.darkfluid-resources`，**不在本庫**，重建方法見 §7 |
| 搭配 mod | `local.host-ops-test`：把蟲族星球的任務組設成 6。https://github.com/cainiao524/HD2-Host-Operations |
| 1.001.104 | depot 553853 manifest 5472097634894368342（553851 8986370718041180263、553854 321316744552908920）。下載清單：`old-build/stage-1001104-*.txt` |
| 2024-06-01 | depot 553853 manifest 5604538564913335971 |

建置方式：

```powershell
HellForge.Cli.exe build-mod --mod <HellForge>\mods\local.darkfluid-port
```

日誌在 `<HellForge>\logs\sessions\<id>\mod-local.darkfluid-port.log`。

## 2. 任務如何出現（已確認）

- #93 是類型 3 任務。只有任務組 6（蟲族 ACTIVATE TCS+）有類型 3 的配額。
- Mod 把任務組 6 清單第 7 格的 #91 換成 #93（`OfferMission`），名稱與簡介改回 2024 年的文字。
- 用 host-ops-test 把蟲族星球的任務組設成 6，重建行動後任務就會出現。時限約 40 分鐘，跟 2024 年相同。

## 3. 任務列與地圖（0.9.9 起，已確認 1～9 級都能進圖）

- **地圖：** 使用類型 3 原本的小地圖預設 `0x9510800888744760`。
  - 之前改成標準地圖時，標準地圖會自己加撤離點，跟暗流體印模自帶的降落點（kind 5 紀錄，實體 `7d8876ea`）衝突，地圖放不下。
- **目標槽：** 部署暗流體 ×1（類別 0，由印模擺放），投放點 ×3（`OfferMission` 改成類別 1、數量 3..3）。
  - 投放點不另外佔擺放位置，而是由已擺放印模的槽提供，跟發射 ICBM 的做法一樣。
- **印模：** DF1、DF2 二選一（`InsertObjectiveEntries` 把它們的目標類型寫成 `{28, 0}`，也就是只當部署點）。
  - 背包點（PACK）和 records-only 的印模不擺放。
- **登記項目（每筆 0x1C8）：** 照 1.001.104 的值，但為了能生成地圖改了 3 處：
  - 間距 0（TCS 24）
  - 子區域 0
  - 衛星清單清空

### 3.1 欄位意義

用 filediver 的 `dl_library.dl_typelib` 解出（`analysis/typelib.py`；成員名稱已被剝掉，型別還在）。

LocationStampInfo 在記憶體中的位移：

| 位移 | 意義 |
|---|---|
| +0x00 | StampInfo（難度字在 +0x60） |
| +0xC0 | MissionLocationTag[] |
| +0xD8 | 半徑 |
| +0xE8 | StampRaceAffiliation |
| +0xF0 | LevelGenerationRegionType[] |
| +0x100 | u32[] 衛星群組 id |
| +0x110 | 類別 |
| +0x134 / +0x138 | SubRegionType（18 = Bugs） |
| +0x148 | 間距 |
| +0x15F | GuardForceType |
| +0x160 | 旗標 |
| +0x198 | LocationFlag[] |

記憶體位移 = 現行檔案位移 + 8。2024 與 1.001.104 用的是舊的 0x118 版面，兩者的對照見 `analysis/location_groups.py` 的說明。

**衛星群組：** `LevelGenerationSatelliteGroup {id, satellites[]}` 是印模周圍附帶的副地形。
背包點的群組是 `0x0F513B12`，包含 central_core_hole 和 bug_rock_hole 兩個關卡，在資源包 `d87ceb3e760a3129` 和 `8a895f6de63c7fc0` 裡。目前還沒套用。

## 4. 印模產生設定（1.001.104，已確認）

詳見 `analysis/PORT-1001104.md`。

- 這份設定在開機資源檔 `9ba626afa44a3aa3` 裡（type `b8fd4d2cede20ed7`，name `8105986402e7f076`）。
- **S0 每筆紀錄的大小：** 1.001.104 是 0x38，2024-06 是 0x34，現行版是 0x44。
- **換到現行版的規則：**
  - `+0x2C` 一律 +2。
  - 目標類型 22～28 一律 +1：部署暗流體 27→28，投放點 28→29。
- `port_lgs.py` 會依紀錄大小自動判斷來源版本。
- **S0 +0x10 是印模關卡檔的物件索引**，所以設定補丁和資源包必須出自同一個版本。
- 現行版還留著 DF1、DF2 的孤兒設定（key `0x494D4CF25E7E4384`、`0x23D76A72526FCF1C`），但沒有對應的關卡檔，也沒有登記，所以不能直接用。
- 形狀紀錄（`generated_stamp_settings`）和碰撞圓，在 2024 和 1.001.104 之間完全相同。唯一例外是 TCS-B，它在 1.001.104 多了 3 個子關卡。

## 5. 目標的運作方式（2024 元件與現行版相同）

| 實體 | 內容 |
|---|---|
| 部署 `ba58…` | 行為 objective_bug_tcs_03_backpack。容器戰備在 150 m 內，最多 12 次。步驟文字「Transport Vessel to Payload Site」 |
| 投放點 `0f19…` | 行為 _thumper。start_inactive 1、hide_from_ui_if_inactive 1。12 m 內解鎖 TCS03 鑽頭戰備，步驟「Insert Vessel into Drill」→「Defend Drill」 |
| 鑽頭投放 `b127…` | ObjectiveCarryDelivery，需要容器 `0e4e…`，再接 MinigameWait |
| 背包 `0732…` | ObjectiveCarry |

- ObjectiveComponent 每筆 0xAD0：+0xA84 sequential、+0xA85 start_inactive、+0xA86 hide_from_ui_if_inactive。
- **1.0.3 的做法：** 把投放點元件的 start_inactive 和 hide 都設成 0，強制啟用，**還沒實測**。
  - 推測真正的啟用條件在行為腳本裡（部署完成後才啟用）。
- **地殼鑽頭**（類型 64 `TCS03Thumper`）：
  - 只在暗流體任務裡加進任務戰備表（game.dll+0x213F550 第 1 組），由 GenStampListDetour 判斷。
  - 已確認它會出現在戰備列表。

## 6. 9 級閃退（未解決）

- 當掉的路徑：
  `SpawnEntity → 0x5D1549 → 0x5D2BB0（遞迴走關卡樹）→ 0x5D2CE0 → 0x5D22B0 → exe+0x32F6A0`
- 在 exe+0x32F6A0，`exe+0x9D8C0(id)` 會回傳 NULL，接著沒有檢查就被解參照。
- 1.0.2 在 0x5D2CE0 先查這個 id，查不到就跳過那個單位，**還沒實測**。
- 推測根本原因是某個轉換壞掉的單位。

## 7. 本庫沒附、需要自行重建的東西

| 東西 | 做法 |
|---|---|
| 1.001.104 舊版資料 | 用 `old-build/download.ps1` 搭配 `stage-1001104-headers.txt`、`stage-1001104-settings.txt` 下載 |
| 轉換後的印模資源（62 個） | `analysis/build_port_resources.py <out> --set deploy --old <1.001.104 data>`，輸出後把 `9ba626afa44a3aa3.patch_0*` 改名成 `d87885c7eaace597.patch_0*` |
| 印模設定補丁 | `analysis/port_lgs.py <1.001.104 data> <current data> <out>`，跟上一項放進同一個資源包 |
| 解密後的 .dl_bin | 把 `darkfluid_port.c` 的 `(void)DecryptOldSettings;` 改回呼叫，再改 `OLD_DIR`、`DEC_DIR` |

## 8. 轉換器現況

- **單位：** 對 1.001.104 測了 196 個 GPU 資料相同的單位，89 個完全一致。
  - 大小不同的主因是現行版新增的 CustomizationInfo 屬性 `0x328f2050`，這是內容差異，不是格式問題。
- **關卡：** `+0x2C` 有兩個換算值（12→14、13→15）還是推測。
- **材質：** 除了地形材質以外，都還沒驗證。
- **粒子效果 2 個（版本 0x6B→0x73）、音效庫：** 還沒轉換。

## 9. 還沒解決的問題

1. 1 級完整流程還沒打通：呼叫容器 → 投放點 → 鑽頭 → 防守 ×3 → 撤離。
2. 9 級閃退（§6）。
3. 紫色地面。
4. 背包點和它的衛星副地形。
5. 尖嘯蟲大波次。
6. 關閉 TCS（#92）。
7. 資源包對照（hash_lookup）：目前是請求 27 個現行資源包來代替。
8. 連線：客機也要裝資源包和 mod。

## 10. 重要位址（game.dll，構建 25480438）

| 位址 | 用途 |
|---|---|
| 0xC14950 | 關卡產生的步驟分派（0x15 GenStampList、0x16 GenObjectives、0x17 AddImportantLocations、0x8B SpreadLocations） |
| 0xF8E530 | GenerateMissionStampList |
| 0xF7C310 | SpreadLocations（失敗時設 gen+0x19B0EA，整張地圖重新產生） |
| 0xF2BA70 | 間距檢查 |
| 0x3773420 | 任務列表（每列 0x380，目標槽從 +0x40 開始，每槽 0x18） |
| 0x32EF870 | 目標表（每列 0xA0，類型 = 列號 + 2） |
| 0x213F550 | 任務戰備表（8 組 × 6 格） |
| 0x5D2CE0 | 目標設定時的單位處理（9 級閃退的掛鉤點） |
| 0x3326308 | EngineApi（+0x170 讀檔） |

更細的逐版紀錄見 `docs/research-log.md`。
