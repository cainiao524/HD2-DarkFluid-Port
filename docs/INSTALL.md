# 部署暗流體 v1.0（HellForge mod）

這個 mod 把 2024 年默里迪亞的「部署暗流體」任務移植回 HELLDIVERS 2，遊戲構建 25480438，需要 HellForge 0.5。

## 內容

| 資料夾或檔案 | 誰要裝 | 說明 |
|---|---|---|
| `mods/local.darkfluid-port` | **所有玩家** | 任務本體，mod 版本 1.0.14。HellForge 會從 `src` 自動建置 |
| `mods/zz.darkfluid-resources` | **所有玩家** | 移植的印模、地形和模型資源包 |
| `房主專用/host-ops-test-0.3.0-test22.zip` | 只有房主 | 把蟲族星球的任務組設成 6，部署暗流體任務才會出現 |

> 所有玩家的 `local.darkfluid-port` 和 `zz.darkfluid-resources` 必須是同一版。版本不一致時，HellForge 會擋下加入（「gameplay mods don't match」）。

## 安裝

1. 關掉遊戲。
2. 把 `mods` 裡的兩個資料夾，複製到 HellForge 的 `mods` 資料夾。例如 `HellForgev5\mods\local.darkfluid-port`、`HellForgev5\mods\zz.darkfluid-resources`。
3. 打開 HellForge 啟動器，確認這兩個 mod 都是啟用狀態。
4. 房主另外匯入 `房主專用\host-ops-test-0.3.0-test22.zip` 並啟用。

## 開任務（房主）

1. 進遊戲後，在艦船上按 **F11**，打開 host-ops-test 選單。
2. 選一顆蟲族星球當入口，任務組選 **6（ACTIVATE TCS+）**，然後開放。
3. 到戰術桌進入那顆星球，任務卡上會出現「部署暗流體」。

## 任務流程

1. 降落後，在部署標記 150 m 內呼叫**暗流體容器**（任務戰備）。
2. 容器送到後，3 個**投放點**會出現。
3. 到投放點 12 m 內呼叫**地殼鑽頭**，把容器放進鑽頭，再守住鑽頭直到它跑完。
   - 同一時間只會有一個投放點在鑽，另外兩個會暫時關閉，這是原版的設計。
4. 3 個投放點都完成後，部署暗流體完成，前往撤離。

時限約 40 分鐘。地形會在兩種暗流體印模之間隨機。

## 已知問題

- 部署完成時的大爆炸特效，以及暗流體專用的音效還沒移植。
- 最後的尖嘯蟲大波次還沒做。
- 8～10 級剛修好降落崩潰，還在測試中。如果崩潰，請把 HellForge 的 `logs\sessions` 裡最新那個資料夾傳給作者。

## 原始碼與移植紀錄

https://github.com/cainiao524/HD2-DarkFluid-Port（v1.0）
