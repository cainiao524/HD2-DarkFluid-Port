# 協作交接：部署暗流體／關閉 TCS 移植

目標：把 HELLDIVERS 2 在 2024 年的「部署暗流體」（任務列 #93）與「關閉 TCS」（#92）移植到 HellForge，遊戲構建 **25480438**，做到盡量和 2024-06-01 版一比一。

這份文件只寫**已經確認**的事（有實機日誌或檔案比對佐證），以及還沒解決的問題。推測一律標成「推測」。

最後更新：2026-11-27，mod 版本 0.8.2。

---

## 0. 規則與限制

- **不改遊戲本體檔案。** 所有改動只透過 HellForge：mod DLL，以及 `<bundle>.patch_<n>` 資源補丁。Mod 只能附帶這種補丁檔。
- **不改原版模型、原版印模。** 只新增，不接管現有的。
- 地圖類型只改 #93 這一個任務，見 §3。
- 安裝或建置 mod 前要先關遊戲。遊戲執行時，HellForge 會鎖住 mod 資料夾。

## 1. 環境

| 項目 | 內容 |
|---|---|
| HellForge | 0.5（SDK 20；本 mod 的 manifest 寫 `"sdk": 16`，可載入） |
| 主 mod | `darkfluid-port/`，id `local.darkfluid-port` |
| 資源包 | `zz.darkfluid-resources`（**不在本庫**，見 §6 如何重建） |
| 搭配 mod | `local.host-ops-test`：把蟲族星球的任務組設成 6、換地形。公開庫：https://github.com/cainiao524/HD2-Host-Operations |
| 2024 舊版 | depot 553853 manifest 5604538564913335971（2024-05-31）。下載腳本在 `old-build/` |

建置：

```powershell
HellForge.Cli.exe build-mod --mod <HellForge>\mods\local.darkfluid-port
```

- 日誌：`<HellForge>\logs\sessions\<id>\mod-local.darkfluid-port.log`。

## 2. 任務如何出現（已確認）

- #93 是類型 3 任務。只有任務組 6（蟲族 ACTIVATE TCS+，1536944888）有類型 3 的配額，原本的類型 3 任務是 #91。
- Mod 把任務組 6 清單第 7 格的 #91 換成 #93（`OfferMission`），名稱和簡介改回 2024 年的文字 id 2401299203、3795917234。
- 用 host-ops-test 把蟲族星球的任務組設成 6，重建行動後就會出現。
- 2024 年的 3 個專用任務組（啟動／關閉 TCS、部署暗流體）已整列刪除。它們在加殼的執行檔裡，配額讀不到。

## 3. 地圖產生（已確認）

- 任務列 +0x38 是地圖產生預設。類型 3 共用小地圖 `0x9510800888744760`，放不下暗流體印模。Mod **只把 #93** 改成蟲族標準地圖 `0x68C82F6B46E7F5DB`。
- 印模產生設定存在開機資源檔 `9ba626afa44a3aa3`，type `b8fd4d2cede20ed7`、name `8105986402e7f076`。
  - `analysis/port_lgs.py` 把 2024 年 11 個印模的設定合併進現行版本，輸出補丁 `9ba626afa44a3aa3.patch_0`。
  - 格式說明在該檔開頭。紀錄長度 0x34→0x44；`+0x2C` 與目標類型的換算表也在檔內。
- 目標類型編號（舊→新，加 2）：

  | 目標 | 舊編號 | 新編號 |
  |---|---|---|
  | 關閉電池筒 | 24 | 26 |
  | 突變蟲卵 | 25 | 27 |
  | 部署暗流體 | 26 | 28 |
  | 投放點 | 27 | 29 |

- `GenerateMissionStampList`（game.dll+0xF8E530）被 mod 掛鉤，只用來記錄選擇結果。印模的目標類型已經由設定補丁提供。

### 3.1 印模登記項目（location items）— 2024 原始資料已取得

- **2024 年只登記了 5 個印模**：DF1 `0b34988e28a9131b`、DF2 `b10c9886d3006fd4`、背包點 `2ddaa26e56f2cd4c`、TCS-A `465af3d39ab42964`、TCS-B `004f0e2fc6bcdb32`。
  - 5 個都在**洲際飛彈（`413c46c846ca6482`）的同一個登記組**，該組共 62 筆。
- 另外 6 個「只有紀錄」的印模（`adab38f5`、`d0a2ca99`、`ba1d0dda`、`74606ed3`、`80aca473`、`a04d0a19`）只存在於設定總表，**2024 年也沒登記**：
  - 遊戲不會擺放它們，2024 年的資料裡也沒有它們的地形檔。
  - 硬放的話，會出現大平地加上紫色的「?」。
- 登記項目在記憶體裡長 0x1C8，結構是「指向形狀紀錄的指標 + 現行檔案的登記內容」，所以記憶體位移 = 現行檔案位移 + 8。
- **2024 → 現行的欄位對照**：用兩版共有的 341 個印模逐欄比對得出。表內位移以印模鍵為 +0。

| 2024 檔案 | 現行檔案 | 意義／換算 |
|---|---|---|
| +0x54 | +0x58 | 難度字（第 n 位 = 第 n 級）。2024 年的 0x1FE 對應現行的 0x3FE。Mod 用 0xFFFE，才能進 10、11 級 |
| +0x8C | +0xE0 | 0x07FF000F → 0x10F |
| +0xB0 | +0x108 | 類別。暗流體與 TCS 是 4，洲際飛彈是 6 |
| +0xC4 | +0x130 | 暗流體與 TCS 是 18（意義未知） |
| +0xD4 | +0x140 | 間距：DF1、DF2、背包點 200 m，TCS 24 m |
| +0xE0 | +0x154 | 最高位元組是列舉值；5 以上被重新編號。DF1、DF2 是 0x060000FF，背包點與 TCS 是 0x050000FF |
| +0xE4 | +0x158 | 位元旗標：0x74 → 0x434，0x7C → 0x43C（背包點），0x70 → 0x430 |
| +0xA0 陣列 | +0xF8 陣列 | 2024 年只有背包點有內容，DF 與 TCS 為空 |

- 0.8.2 把上表的值寫進 mod 新增的 5 個登記項目，其餘欄位複製自洲際飛彈。
- **推測：** 最高位元組 6 在現行版本可能被重新編號，因為找不到兩版都有、值為 6 的印模可以對照。
- 每個登記項目的形狀紀錄由 mod 自己建：
  - 碰撞圓用 2024 年的值（`analysis/stamps-11-20240601.json` → 原始碼裡的 `kPorted`）。
  - 清空代理物件，也就是隱形牆。
  - **保留 +0x70 的旋轉方式。** 清掉的話地圖會產生不出來（0.6.2 的教訓）。
- 半徑目前用洲際飛彈的 49.7 m。之前試過 2024 年的外圈 143–187 m，`SpreadLocations` 會一直失敗、不斷重新產生地圖。

## 4. 目標與戰備（已確認）

- 目標實體都還在現行版本：
  - 部署 `ba58a7667aa11627`
  - 投放點 `0f19b7b633e9aa3c`（2024 年定義：12 m 內解鎖 TCS03 鑽機戰備）
  - 鑽機投放 `b127552416ce512e`
  - 暗流體背包 `073270650f859dd0`
  - 容器 `0e4e0a4947d5bfbe`
  - 關閉 TCS `4bac90c3b06a45d2`、蟲卵 `178183c8c74c6eba`
- 實機日誌顯示，每場都會生成投放點目標實體 ×4–5、部署 ×2、背包 ×8。
- 鑽機戰備 = 類型 64 `TCS03Thumper`，中文「地殼鑽頭」／TECTONIC DRILL，文字 id 3177142887。
- 任務戰備表在 game.dll+0x213F550：8 組 × 6 格，依任務列的類型（+0x34）選組，在 0x6AE2E7。類型 3 用第 1 組。
  - Mod 把 64 加進第 1 組。0.7.3 日誌確認它已在玩家任務用的戰備列表裡（`PlayerHistory` 紀錄 +0xA28，格子在 +0x188，數量在 +0x788）。
  - **還沒確認 HUD 有沒有顯示它。**
- 舊版 9 級的目標配置（使用者提供）：部署暗流體 ×1、投放點 ×3。

## 5. 2024 年設定檔（.dl_bin）解密方法

- `data/game/*.dl_bin` 是加密的，引擎（加殼的 helldivers2.exe）讀檔時才解密。
- 讀檔介面：`[[game.dll+0x3326308]+0x170]`。
  - `+8` 是 `read(name, &size)`，回傳緩衝區。
  - `+0x20` 是 `free(buffer)`。
- 做法（`darkfluid_port.c` 的 `DecryptOldSettings`，目前已停用）：
  1. 掛鉤 KernelBase 的 `CreateFileW`。
  2. 請引擎讀現行的同名檔，開檔時換成 2024 年的檔案，引擎就會照常解密。
  3. 寫出 `<舊版>\dec\` 與 `dec\current\`。
- 49 個檔案中 46 個是標準 LDLD 格式。`stamp_settings`、`zone_data`、`resource_swap` 的開頭不同，但內容可讀。
- 要重新產生：把 `Tick` 裡的 `(void)DecryptOldSettings;` 改回 `DecryptOldSettings();`，並修改 `OLD_DIR` 與 `DEC_DIR`。

## 6. 本庫沒附、需自行重建的東西

| 東西 | 怎麼做 |
|---|---|
| 2024 舊版資料 | `old-build/download.ps1`，搭配 `stage1-headers.txt`、`stage2-darkfluid.txt`、`stage3-settings.txt` |
| 轉換後的印模資源 | `analysis/build_port_resources.py <out> --set deploy`。路徑寫死在檔案開頭，要改。輸出後把 `9ba626afa44a3aa3.patch_0*` 改名成 `d87885c7eaace597.patch_0*`，見 §7 |
| 印模設定補丁 | `analysis/port_lgs.py <2024 data> <current data> <out>`，輸出 `9ba626afa44a3aa3.patch_0`，放進同一個資源包 |
| 現行資源索引 | `analysis/index_slim.py` 會產生 `index_25480438.json` |
| game.dll 映像 | `analysis/disasm.py` 需要 `analysis/image/game-25480438.bin`（解殼後的 game.dll） |

## 7. 執行時載入

- 印模資源包得先載入。Mod 用遊戲自己的 `RequestPackages`，請求 27 個現行資源檔，涵蓋印模用到的 672 個共用資源（`kDepBundles`）。
- 請求 2024 年的資源包 id 會讓遊戲當掉（「Incorrect version found when loading bundle」），因為資源包 id 就是資源檔名。
- 轉換後的資源放在 `d87885c7eaace597` 的補丁裡，這是 27 個裡會被請求的其中一個。
- 實體字串防護：`0xFDA6DA` 被改成跳到一段 stub。原因是轉換後的部分單位沒有 entity 字串，遊戲讀到空指標會在 0xFDA6E7 當掉。

## 8. 還沒解決的問題

1. **紫色地面**
   - 已排除 Meridia 被刪的地表包：現行遊戲沒有任何資源引用那些已刪的材質。
   - 紫色集中在移植的印模附近。推測是印模地形的地面塗層，或材質轉換錯誤；材質轉換器只有約 1/4 完全正確。
   - 現在有 `generated_region_settings`，2024 版與現行版都已解密，可以比對地表材質層。
2. **高難度地圖生成**：7 級要重試約 50 次才成功，9 級曾卡住。0.8.2 改用 2024 年的登記欄位後還沒實測。
3. **地殼鑽頭**：已在戰備列表裡，還沒確認 HUD 顯示，也還沒確認在投放點能不能用（呼叫 → 插入容器 → 防守）。
4. **HUD**：同時顯示兩個「部署暗流體」，第二個沒有步驟文字。
5. **關閉 TCS（#92）**：TCS-A、TCS-B 的地形資源還沒轉換，任務也還沒放進任務組。
6. **資源轉換**：材質轉換器、2 個粒子效果（版本 0x6B → 0x73）、暗流體和 TCS 的音效庫。
7. **紀錄 +8 欄位**（0xCD121F4B）的意義未知。
8. **尖嘯蟲大波次**：來源未知，印模裡沒有專用物件。

## 9. 重要位址（game.dll，構建 25480438）

| 位址 | 用途 |
|---|---|
| 0xF8E530 | GenerateMissionStampList |
| 0xF8F1D0 | GenerateObjectiveLocations |
| 0xF7C310 | SpreadLocations（失敗就重新產生地圖） |
| 0xFDA650 / 0xFDA6DA | 實體佇列與防護點 |
| 0xFDC140 | SpawnEntity |
| 0x213F550 | 任務戰備表（8×6） |
| 0x6AE2E7 | 依任務類型選組 |
| 0x3326DA0 | ObjectiveManager（+0x24 數量、+0x50 實例） |
| 0x347CE50 | PlayerHistory |
| 0x3326308 | EngineApi（+0x170 讀檔） |

更細的逐版紀錄見 `docs/research-log.md`。注意：`analysis/FULL-MISSION-ANALYSIS.md` 第四節「補齊 11 個印模」的計畫已被 §3.1 推翻。
