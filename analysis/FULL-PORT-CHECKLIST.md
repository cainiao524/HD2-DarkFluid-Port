# 完整重現 2024 年「部署暗流體」與「關閉 TCS」：逐層需求清單

比對對象：
- 舊版：2024-06-01（553853 manifest 5604538564913335971）的資料，以及同期社群解包 shalzuth/HelldiversData 提交 5a2e4b30a2（物件與實體定義）。
- 現行：HellForge 0.4，遊戲構建 25480438。依據是記憶體映像 `analysis/image/game-25480438.bin`、`index_25480438.json`、HellForge 即時實體紀錄 `stats-values.tsv`，以及 Darctor 1.007.100 實體表。

狀態欄：**在** = 現行已有，不用動；**補** = 要由 Mod 或資源包補回；**未知** = 要實機或進一步逆向確認。

## 1. 行動層（任務組）

| 項目 | 2024 | 現行 | 狀態 |
|---|---|---|---|
| 任務組「DEPLOY DARK FLUID」 | 有。簡介：注入暗流體到超級蟲巢地殼 | 整列刪除；名稱 2401299203、簡介 3795917234 還留在文字檔 | **補** |
| 任務組「DEACTIVATE TERMINID CONTROL SYSTEM」 | 有 | 整列刪除；名稱 270478576、簡介 691698893 | **補** |
| 任務組「ACTIVATE TERMINID CONTROL SYSTEM」（3 月，不在範圍） | 有 | 整列刪除；名稱 2575685330、簡介 1880335159 | 不做 |
| 配額與任務清單 | 在加殼的舊 exe 裡，讀不到 | — | **未知**，改用推論 |

做法：用現行任務組 6（ACTIVATE TCS+，配額「類型 1 抽 0–2、類型 2 抽 0–1、類型 3 固定 1」）當結構藍本：
- 把類型 3 那一格換成 #93 或 #92。
- 名稱、簡介改用上面遺留的文字 ID（任務組列 +0x24 名稱、+0x28 簡報、+0x2C 簡介）。
- 簡報（+0x28）是否也留在文字檔裡還沒查。

讓星球用這個任務組，靠 host-ops-test 的任務組覆寫（實測可用）。

## 2. 任務層

| 項目 | 現行內容 | 狀態 |
|---|---|---|
| 任務列 #93 DEPLOY DARK FLUID（3858764807） | 啟用；蟲族；難度 1–11；類型 3；地區只限 [1] 一般；名稱、短述、長述都在 | **在** |
| #93 的目標 | 目標列 26（629858657，背包，Objective_bug_tcs_03_backpack）、目標列 27（2314105256，投放點，Objective_bug_tcs_03_thumper，最多 3 個） | **在** |
| 任務列 #92 DEACTIVATE TCS（3005556867） | 啟用；類型 3；目標列 24 關閉電池筒、25 摧毀突變蟲卵 | **在** |
| 限時 40 分（暗流體）／12 分（關閉 TCS） | 位置沒找到；同類型 3 的 #91 能正常運作 | **未知**，實機看 |

## 3. 地形與地圖產生

| 項目 | 2024 | 現行 | 狀態 |
|---|---|---|---|
| 星球地形：Meridia 超級蟲巢 | 星球 67，`planet_forest_primordial_base_bug`，原始地區 region_primordial_bug | 地形設定還在（settingsHash 215016124，`forest_primordial_base_bug`）；Meridia 本身在戰爭裡是黑洞 | **在**；用 host-ops-test 把入口星球的地形換成 215016124 |
| #93 只限地區 1 | — | 超級蟲巢地區的地區類型是不是 1 | **未知**（不符合就抽不到任務） |
| 暗流體印模 3 個 | fb04…（DF1）、8ac0…（DF2）、efd6…（背包點） | 整包刪除 | **補**：資源已轉好（zz.darkfluid-resources） |
| 關閉 TCS 印模 2 個 | 271dd…、e3319… | 整包刪除 | **補**：資源要轉（每包約 34–55 個缺檔） |
| 印模設定（碰撞圓、入口、路徑樣條） | stamps-20240601.json | 沒有 | **補**：目前沿用探勘鑽機的碰撞紀錄（妥協），應改用原本的碰撞圓 |
| 印模登記（放進哪個登記組、難度、旗標） | 社群解包的 location_groups 是空的，讀不到 | 沒有 | **補**：目前掛在探勘鑽機那一組（推論做法） |
| 印模 → 資源包對應、印模 → 目標類型對應 | — | 沒有 | **補**：移植 Mod 寫入（0.3.1 尚未成功跑完） |

## 4. 各任務點的結構（舊版印模內容，stamp_contents.py）

**DF1（fb04…，碰撞外圈 187.4）／DF2（8ac0…，165.2）**：完整的投放區。
- 降落點 landing_zone ×1。
- 敵人遭遇點 enemy_encounter_marker ×12、TCS 刷怪點 bug_tcs_spawner_marker ×10–12、蟲洞 bug_spawner ×6–10。
- 地殼裂口 central_core_hole ×5–6（暗流體注入點）。
- DF1 有鑽機模型 thumper ×3。
- 補給：彈藥箱 ×19–20、醫療包 ×3–9、手榴彈箱 ×2–3、武器生成點數個。
- 場景：孢子燈 ×8–55、已破蟲卵 embryo_01_cluster_x6_destroyed ×40–55。
- 主體物件 dark_fluid_tcs_01 / 02，以及 935c…、f3f3…、9d9f… 等。

**背包點（efd6…，142.9）**：
- 地殼裂口 ×8。
- 暗流體背包 073270650f859dd0 ×4。
- 73c77ef7… ×3。
- 沒有降落點。

**關閉 TCS（271dd…、e3319…）**：
- TCS 充能基座 bug_tcs_support_base_charger_01 ×3（3 座電池筒）。
- 突變蟲卵 embryo_01_cluster_x6 ×20。
- 敵人遭遇點 ×13、降落點 ×1。
- 大量場景模型（92d3… ×168 等）。

缺的 unit 在 2024 年就沒有任何實體定義，都是純場景物件，只需要資源檔。

## 5. 實體與目標邏輯

| 實體 | 2024 定義 | 現行 | 狀態 |
|---|---|---|---|
| 背包目標 ba58a7667aa11627 | 戰備 DarkFluidBackpack、單一、可控制次數；階段 WaitForBehavior「Transport Vessel to Payload Site」×2；可呼叫最多 12 次、150 m 內 | 在（Objective、Behavior、Ability、EffectReference） | **在** |
| 投放點目標 0f19b7b633e9aa3c | 戰備 TCS03Thumper（12 m 內 1 次）；階段「Insert Vessel into Drill」→「Defend Drill during deployment」 | 在（Objective、Behavior） | **在** |
| 鑽機投放實體 b127552416ce512e | ObjectiveCarryDelivery（需送入 0e4e… ×1）、HellpodPayload、MinigameWait | 在，還多了 Health、Interactable、InteractableMinigame | **在** |
| 暗流體背包 073270650f859dd0 | Backpack、Jumppack、ObjectiveCarry、Recharge… | 在 | **在** |
| 容器 0e4e0a4947d5bfbe | unit | 在（unit） | **在** |
| 關閉 TCS 目標 4bac90c3…、蟲卵目標 178183c8… | 「Deactivate corrupted battery silos」、「Destroy mutated Terminid eggs」 | 在 | **在** |
| 戰備：暗流體背包、TCS03 鑽機 | — | 在（HF_STRATAGEM_DARK_FLUID_BACKPACK、HF_STRATAGEM_TCS03_THUMPER） | **在** |
| 能力：背包推進、鑽機起降、暗流體爆炸等 | — | 2025-09 的列舉還在 | 大致**在**，實機確認 |

## 6. 敵人與節奏

| 項目 | 狀態 |
|---|---|
| 鑽機啟動時的蟲洞突破、印模裡的遭遇點與 TCS 刷怪點 | 會跟著印模資源一起回來 |
| 最後一台鑽機後的尖嘯蟲大波次、撤離點尖嘯蟲 | **未知**。印模裡沒有尖嘯蟲專用物件，可能來自目標行為、Meridia 敵人配置或已被熱修正調整的參數 |
| 2024-06-01 熱修正（鑽機附近不再冒蟲） | 在舊 exe 裡，帶不過來 |

## 7. 其他資源

| 項目 | 狀態 |
|---|---|
| 文字（任務、目標、提示、背包） | **在** |
| 音效（TCS 啟動／關閉、背包、鑽機） | 在，但已改名重製；舊音效庫 84deb5ec… 沒移 |
| 粒子效果 | 2 個沒轉（版本 0x6B → 0x73） |
| 關閉 TCS 的音效庫 | 沒移 |

## 8. 要補的工作（依順序）

1. 移植 Mod：只改任務組 6 的類型 3 那一格，並換上遺留的名稱與簡介。
2. 重新安裝三個 Mod：local.darkfluid-port、zz.darkfluid-resources、local.host-ops-test。
3. 實機跑 0.3.x：確認三張對應表與登記項目寫入成功。
4. 用 host-ops-test 設定一顆蟲族星球：任務組 6、地形 215016124。確認戰爭桌出現部署暗流體。
5. 出擊：確認印模出現、背包點與 3 個投放點、鑽機插入、撤離。記錄尖嘯蟲與限時。
6. 修正：碰撞圓改用 stamps-20240601.json 原值；補 2 個粒子效果。
7. 關閉 TCS：轉換 271dd…、e3319… 兩包，再重做 1–5。
