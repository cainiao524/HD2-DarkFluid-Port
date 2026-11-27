# 研究紀錄（逐版測試結果與發現）

原文是工作筆記，英文為主；整理過的結論見 [HANDOFF.md](HANDOFF.md)。其中 `[[...]]` 是筆記之間的連結，可忽略。

Checked 2026-11-26 from analysis/image/game-25480438.bin:
- Mission row 93 DEPLOY DARK FLUID (id 3858764807): enabled, faction 2, difficulty 1–11, kind (+0x34) = 3, region [1] normal. The other kind-3 bug missions are 90, 91 (ACTIVATE TCS+ STATION, live), 92 and 71.
- Template quotas at +0x178 are 16-byte entries {kind, min, max, f32 weight}, with the count at +0x208. Selector 0x11e5100 first draws a kind from these quotas, then takes only the template-list missions of that kind.
- Only templates 6 (bug ACTIVATE TCS+, 1536944888: {3,1,1,1.0}), 17 and 22 (illuminate) have a kind-3 quota. Template 6's list has only one kind-3 mission, row 91.
- Picker 0x11e6340 takes the minimum per-mission usage count among candidates (least-used first). Adding 93 next to 91 therefore gives roughly alternating picks, not a guarantee.
- So darkfluid-port 0.3.1 OfferMission (append to bug templates with 5 or more entries) does nothing: those templates have no kind-3 quota. The offer must target template 6, ideally replacing 91 with 93.
- To get a planet on template 6, use local.host-ops-test (the F11 template override works since test6). Another path is its 自定義任務 slot = #93 (selector hook; untested).

On 2026-11-26 local.darkfluid-port, zz.darkfluid-resources and local.host-ops-test folders were all gone from HellForge\mods (ids remain in the disabled list), probably from the 0.4 repair.

Related: [[custom-mission-group-goal]] [[host-custom-operations-ref]]

2026-11-26 deleted templates: template rows hold name/briefing/summary string ids at +0x24/+0x28/+0x2c. The current strings still have three name+summary pairs that nothing in game.dll references (searched at any alignment). These are three deleted bug templates:
- ACTIVATE TERMINID CONTROL SYSTEM 2575685330 / "The TCS is ready…" 1880335159
- DEACTIVATE TERMINID CONTROL SYSTEM 270478576 / "The Terminids are mutating…" 691698893
- DEPLOY DARK FLUID 2401299203 / "Inject Dark Fluid beneath the Supercolony's crust…" 3795917234

Template 6 ACTIVATE TCS+ is a later, different operation ("newfound world" briefing).

The 2024 helldivers2.exe is WinLicense-packed and the 2024 data has no mission/template tables, so the original quotas and lists are not readable.

Shrieker waves: the DF stamps only hold bug_spawner markers and bug_tcs_spawner_marker. Objective ids 26/27 still sit in about 12 per-objective-type tables near game.dll 0x2135f00–0x215ab90, so the type code paths likely remain. The SDK has no enemy-spawn call, only difficulty budgets (encounter_points, spawner_cooldown) and HF_ON_SPAWN.

2026-11-26 full checklist: analysis/FULL-PORT-CHECKLIST.md.
- Present now: objective entities ba58a766… (backpack), 0f19b7b6… (payload), drill delivery b1275524…, backpack unit 07327065…, and the Deactivate TCS entities 4bac90c3… and 178183c8…. Source: HellForge live stats-values.tsv plus the Darctor 1.007.100 map.
- Meridia's supercolony terrain still exists: settingsHash 215016124, forest_primordial_base_bug. Meridia itself is a black hole in the war, so use the host-ops-test terrain swap.
- Unknown: whether the supercolony region counts as region type 1, which #93 requires.
- The 2024 objective definitions come from shalzuth 5a2e4b30a2 ObjectiveComponentData. Its location_groups dump is empty, so the original stamp registrations are not available.

2026-11-26 (late): the user moved testing to HellForge 0.5.0-beta at F:\Game\A\HD2\HellForgev5 (SDK 20, same build 25480438; SDK-16 mods load). Installed and enabled there: local.darkfluid-port 0.4.0 (built), pack.zz-darkfluid-resources, and local.host-ops-test test21 (already present). They are also installed in 0.4 at F:\Game\A\HD2\HellForge. 0.4.0 swaps the mapping (DF1/DF2 → payload objective 27, backpack stamp → objective 26), offers only after all 3 stamps, and rewrites template 6's kind-3 slot plus name/summary texts.

2026-11-26 0.4.0 run (HellForge v5): the memory search failed and nothing was written. The user forced missions 92/93 with host-ops-test; they deployed fine but spawned no objectives and did not crash.
Live findings:
- Stamp/objective entries sit at 8-mod-16 addresses; 0.4's `% 16 == 0` check skipped them.
- No pointer to their bucket base exists while on the ship.
- GenerateMissionStampList (0xF8E530): r8 = pass; container = [pass+0x38]; +0x18 buckets (0x40 each), +0x20 u32 count, +0x48 empty key; slot = key % count, linear probe (0xF8E700); key at bucket +0x20; type bytes at +0..+0x1F. rcx: [rcx] set array, [[rcx+8]] set count, [rcx+0x20] objective list.
- The stamp/package entries {key, pkg} (~1561 pairs in one block) fit no key % n or power-of-two scheme.
User rejected taking over vanilla stamps ("不想要改原有模型").
0.5.0 (built in v5, untested): appends location items to drill_01's group; hf->load_package loads the 3 stamp packages; hf->hook on 0xF8E530 inserts the stamp/objective entries into [pass+0x38] before each generation; offers template 6 once everything is ready.

2026-11-26 later findings (v5 runs):
- Live stamp/objective container: +0x18 buckets, +0x20 = 0x8000, +0x48 empty marker = ~0, but free buckets hold key 0. Entries sit at (2*key) % 32768 (probe ≤2). The game's lookup from key % N walks to the key, so an entry in any free bucket is found.
- A package id is a bundle name in this engine. RequestPackages on the 2024 stamp package ids crashed with "Incorrect version found when loading bundle 'fb04…'", Fatal 0x42554E444C45 ("BUNDLE").
- Mods may ship only <bundle>.patch_<n> files, and hf->load_package refuses uncatalogued ids.
- Fix (built): request the 27 current bundles covering the 672 shared resources the stamps list (greedy cover from index_25480438). Our converted resources sit in the boot-bundle patch.

2026-11-26 diagnostics (v5):
- With the hook, the DF generation's stamp list has objective 258ae161 with 1 candidate (backpack stamp) and 89ee71a8 with 2 (DF1/DF2), but nothing was placed and no stamp package loaded.
- Cause (likely): mission row +0x38 = map generation preset, written into the level seed. Kind-3 missions (14, 71, 90–93, 128, 144) share 0x9510800888744760, a small ~250 m map. Terminid standard missions use 0x68C82F6B46E7F5DB (bots 0x0B2BBB282AC9BFDA, Illuminate 0x003621037EC6FFEE).
- Pending fix (written, not built): OfferMission sets #93's +0x38 to the bug standard preset and keeps kind 3.

2026-11-26 0.5.1–0.5.3:
- drill_01-cloned items were never offered on the kind-3 map. With TCS+ station items as the template (stamp e5507fb92bd845a6, set f28cfc63, group bd7c08ab, radius 186.5), both objectives get placed: GenerateObjectiveLocations places 0→2. Then SpreadLocations runs twice and the level regenerates forever: two big stamps don't fit the small map.
- Payload "most 3" = 3 drill points inside one stamp; the mission places 1 backpack stamp + 1 payload stamp.
- The user first rejected map-type changes, then chose "Switch this mission only". 0.5.3 sets mission #93's +0x38 to bug standard and keeps the TCS template; untested.
- The stamp→package data (key, pkg pairs) is a serialized blob (header 3,1 … deadbee7), not an insertable table.

2026-11-26 0.5.4/0.5.5:
- The ICBM template (radius 49.7) still looped. Keeping the template's 49.7 m radius (0.5.5 test) made the map generate with objectives visible, so the 143–187 m radii break SpreadLocations.
- Starting the mission then crashed at game.dll+0xFDA6E7 (fn 0xFDA650): a 4096-entry {id, unit handle} queue at mgr+0xF2AEF8. For each unit it reads unit script data key 0x21FE2A86 ("entity" path string) through [EngineApi+0xB8], hashes it (0xAAFF30) and calls SpawnEntity 0xFDC140. A null pointer for unit 0x400847 crashed it.
- Ported units 935c9d42/9d9f490f/f3f34d92/07327065 carry 2024 entity paths (bug_tcs_03_thumper/_bridge/_backpack, bug_darkfluid_backpack); all exist in the current game. Their converted bytes match old.
- Hypothesis: boot-bundle patch units never get script data registered. The pack was moved from 9ba626afa44a3aa3.patch_0 to d87885c7eaace597.patch_0, one of the 27 bundles requested at runtime; untested.
- Level converter: 424/801 shared levels exact; most size diffs are content edits. Unit converter: 267/400 exact.

2026-11-26 0.5.8 in-game result: the mission enters and the DF tower spawns, but the ground under it is purple and invisible walls don't match the models. The entity-string guard (patch at 0xFDA6DA → stub) skipped 17–87 null-entity units; they aren't spawned through World.spawn_unit.
- D10/11 cannot enter: the ICBM template level word +0x60 = 0x3FE (levels 1–9). 0.5.9 sets it to 0xFFFE.
- Om entry +0x28/+0x2C and +0x38/+0x3C are per-stamp record ranges sized with the stamp (drill 3/5, ICBM 122/17, TCS 991/252), likely ground cells/paint/blockers. The copied ICBM ranges probably cause the purple paint and invisible walls; 0.5.9 zeroes them (untested).
- Material conversion (version byte swap) is exact for only 154/600 shared materials; 419 changed size, so the 10 ported materials may be wrong too.

2026-11-26 settings port (user chose option A = patch the resource):
- Stamp generation settings = boot bundle 9ba626afa44a3aa3, type b8fd4d2cede20ed7, name 8105986402e7f076.
- Layout: S0 records (2024 0x34 → now 0x44); S1 stamp→types table (0x40, slot (2k)%n); S2 (0x38); S3 (0x10); S4 u64 list; S5 u64 names. Om +0x28/+0x2C index S0, +0x38/+0x3C index S5.
- In the 2024 data, DF1/DF2/PACK each list both old types [27,26]; old 26 = Deploy DF (entity ba58…), 27 = Payload (entity 0f19…).
- Record conversion: pad 16 zero bytes; +0x2C map 1→3, 2→4, 3→5, 6→11, 15→17, 12→14 (guess); flag-8 records' +4 map 26→28, 27→29.
- Tool: analysis/port_lgs.py writes darkfluid-settings/9ba626afa44a3aa3.patch_0, installed into mods/zz.darkfluid-resources in v5.
- hash_lookup 7056bc19c69f0f07 (stamp→pkg) also holds the DF entries, but its format changed and was left alone.
- Location items are not in any data resource (compiled), so the mod still adds them (ICBM template, radius 49.7, levels 0xFFFE).
- 0.6.0 no longer writes om entries (logs them from the patch). Untested.
- Full scan of 2024 non-art resources: nothing else references DF; no separate objective scripts or configs.

2026-11-27 full analysis: analysis/FULL-MISSION-ANALYSIS.md.
- The 2024 settings blob has 11 stamps for missions 92/93 (old types 24 deactivate, 25 eggs, 26 deploy, 27 payload).
- Five have levels: DF1, DF2, PACK, TCS-A 465af3d3, TCS-B 004f0e2f.
- Six are record-only (no level, no package): adab38f5, d0a2ca99, ba1d0dda, 74606ed3 (payload-only), 80aca473 (deploy-only), a04d0a19 (TCS).
- With only the 3 dual-use stamps ported, the payload objective landed on the PACK stamp: 4 backpacks, no payload entity, so no drill.
- Set-1 grant of stratagem 64 did not show in the loadout; objective stratagems seem to be the only path.
- Meridia setup forest_primordial_base_bug is Meridia-only; its utility_primordial_day, minimap_primordial packages and 4 rocks plus 15 speedtrees were deleted → purple ground.
- Next step (user wants a 1:1 port): add all 11 stamps to port_lgs.py, give items their 2024 circles, port the TCS stamps.

2026-11-27 0.7.0 (built, untested):
- port_lgs.py now merges all 11 mission stamps; type map 24→26, 25→27, 26→28, 27→29; +0x2C map adds 10→12, 13→15, 20→22.
- The mod places items with their own 2024 circles for all 11, from the kPorted table generated from analysis/stamps-11-20240601.json.
- GrantDrill is turned off.
- The record-only stamps have no level resource, so a crash on load is possible; watch for that.

2026-11-27 0.7.1–0.7.3 results:
- Set-1 grant does work: the 0.7.3 log shows stratagem 64 in player 0's loadout and its mission copy (PlayerHistory record +0x38 / +0xA28, slots +0x188, count +0x788). 0x6AE35D has no HellForge jump in this session. Whether the HUD showed it was not yet confirmed.
- The payload objective entity 0f19 and the deploy entity ba58 do spawn (4–5 and 2 per mission). The stamps' payload slots work.
- The user says 2024 difficulty 9 had 1 Deploy + 3 Payload sites. The drill stratagem is "地殼鑽頭 / TECTONIC DRILL" (string 3177142887), used at the payload sites (crust fissures). Likely cause of the D7+ generation loop: only DF1/DF2/PACK can take payload objectives. The 4 record-only payload stamps (adab, d0a2, ba1d, 7460) are probably the real extra payload sites. Their "?" in 0.7.0 is a missing unit to find.
- Purple ground is NOT from Meridia's deleted utility_primordial_day: nothing in the current game references its 8 missing materials or 12 missing textures. Key ffd46e6b… in the 2024 stamp settings is a utility stamp (S1 key), not ours. The purple sits at the ported stamps, so suspect the stamp levels' ground paint and the converted materials.

2026-11-27 0.8.0–0.8.2:
- data/game/*.dl_bin are encrypted. The engine reader decrypts them: [[game+0x3326308]+0x170], +8 read(name,&size), +0x20 free. 0.8.0 hooked KernelBase CreateFileW to swap in the 2024 file. The decrypted 2024 files are in F:\Game\A\HD2\A\0\hd2_old\20240601\dec\, the current ones in dec\current\.
- The 2024 location_groups registered only DF1, DF2, PACK, TCS-A and TCS-B, all in the ICBM's group (62 entries). The record-only stamps were never placed in 2024.
- Field map, 2024 file → current file, checked on 341 common stamps (memory = current file + 8):
  - 0x54 → 0x58 level (0x1FE → 0x3FE)
  - 0x8C → 0xE0 (0x07ff000f → 0x10f)
  - 0xB0 → 0x108 category
  - 0xC4 → 0x130
  - 0xD4 → 0x140 spacing
  - 0xE0 → 0x154 (top byte renumbered above 5)
  - 0xE4 → 0x158 (0x70 → 0x430, 0x74 → 0x434, 0x7c → 0x43c)
- 2024 values for DF1/DF2: category 4, C4 18, spacing 200 (TCS 24), E0 0x060000ff (PACK/TCS 0x050000ff), E4 0x74 (PACK 0x7c), second list (+0xA0 → mem +0x100) null except PACK.
- 0.8.2 writes these values into the items.
