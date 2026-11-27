/* Dark Fluid & TCS Port (local.darkfluid-port), stage 1: read-only discovery.
 *
 * Finds, in the running game, everything the two 2024 missions hang on, and writes it to the mod's log
 * (logs/sessions/<id>/game/mod-local.darkfluid-port.log). Nothing is written to game memory.
 *
 *   - mission rows (game.dll + 0x3773420, 162 x 0x380) that list the missions' objectives, wherever they are
 *     (MDBE 0.4.7 names rows 92 Deactivate TCS and 93 Deploy Dark Fluid), with every field the port will change;
 *   - the objectives' definitions (game.dll + 0x32EF870, 153 x 0xA0): level range and uses per mission;
 *   - the operations (game.dll + 0x32FEF10, 25 x 0x490) that list those rows, and every Terminid operation;
 *   - the level generator's location groups once a level exists ([[world + 0xAE288] + 0x2D0]): which groups carry the
 *     removed stamps (their stamp key is the stamp level's resource hash) and which carry reference stamps the port can
 *     copy items from (the prospecting drill and the Launch ICBM silo, objectives that still work).
 *
 * Addresses: HellForge 0.2.0-beta native/mod-loader/difficulty.c and MDBE 0.4.7 game.h, both for game build 25480438.
 * On any other build the mod does nothing. */
#include "hellforge_mod.h"
#include "hellforge_functions.h"
#include "hellforge_globals.h"

#include <windows.h>
#include <stdio.h>
#include <string.h>

#define BUILD "25480438"
#define RVA_MISSIONS 0x3773420u
#define MISSION_ROWS 162u
#define MISSION_ROW 0x380u
#define RVA_OBJECTIVES 0x32EF870u
#define OBJECTIVE_ROWS 153u
#define OBJECTIVE_ROW 0xA0u
#define RVA_OPERATIONS 0x32FEF10u
#define OPERATION_ROWS 25u
#define OPERATION_ROW 0x490u
#define RVA_WORLD 0x3326340u
#define LOCATION_ITEM 0x1C8u

static const HfApi* hf;
static HfMod* g_mod;
static uintptr_t g_game;

static const struct { uint32_t id; const char* name; } kObjectives[] = {
    {0x258AE161u, "Deploy Dark Fluid"},
    {0x89EE71A8u, "Payload Site"},
    {0x0A7BF185u, "Deactivate Termicide Tower"},
    {0x13E63075u, "Activate Termicide Tower (reference)"},
};
#define OBJECTIVE_COUNT (sizeof kObjectives / sizeof kObjectives[0])

/* Stamp levels (= stamp keys) from the 2024-06-01 build; references that exist now. */
static const struct { uint64_t key; const char* name; } kStamps[] = {
    {0x0B34988E28A9131Bull, "removed: Deploy Dark Fluid site 1 (fb04c613...)"},
    {0xB10C9886D3006FD4ull, "removed: Deploy Dark Fluid site 2 (8ac0fe97...)"},
    {0x2DDAA26E56F2CD4Cull, "removed: Dark Fluid backpack site (efd618be...)"},
    {0x465AF3D39AB42964ull, "removed: Deactivate TCS site A (271dd102...)"},
    {0x004F0E2FC6BCDB32ull, "removed: Deactivate TCS site B (e3319b35...)"},
    {0xAE345C87AC24630Full, "reference: gen_prime_prospecting_drill_01"},
    {0xB0B4C38EBCE49FDAull, "reference: gen_prime_prospecting_drill_02"},
    {0x413C46C846CA6482ull, "reference: gen_prime_launch_icbm_01"},
};
#define STAMP_COUNT (sizeof kStamps / sizeof kStamps[0])

static int Rd(uintptr_t address, void* out, size_t size) { return address != 0 && hf->read(address, out, size) == 0; }

static const char* ObjectiveName(uint32_t id)
{
    for (size_t i = 0; i < OBJECTIVE_COUNT; i++) if (kObjectives[i].id == id) return kObjectives[i].name;
    return NULL;
}

static uint8_t g_rowHit[MISSION_ROWS];
static uint32_t g_rowId[MISSION_ROWS];
static uint8_t g_bugFaction = 0xFF;

static void LogMissions(void)
{
    uint32_t hits = 0;
    for (uint32_t r = 0; r < MISSION_ROWS; r++)
    {
        uint8_t row[MISSION_ROW];
        if (!Rd(g_game + RVA_MISSIONS + r * MISSION_ROW, row, sizeof row)) continue;
        memcpy(&g_rowId[r], row, 4);
        int hit = 0;
        for (int k = 0; k < 32; k++)
        {
            uint32_t id;
            memcpy(&id, row + 0x40 + k * 0x18, 4);
            if (id && ObjectiveName(id) && id != 0x13E63075u) hit = 1;
        }
        if (!hit && r != 90 && r != 91 && r != 92 && r != 93) continue;
        g_rowHit[r] = (uint8_t)hit;
        hits += hit;
        uint32_t lo, hi, opt, major, name, layouts;
        uint64_t delta, layoutList;
        memcpy(&lo, row + 0xC, 4); memcpy(&hi, row + 0x10, 4); memcpy(&opt, row + 0x28, 4); memcpy(&major, row + 0x2C, 4);
        memcpy(&name, row + 0x340, 4); memcpy(&delta, row + 0x360, 8); memcpy(&layoutList, row + 0x368, 8); memcpy(&layouts, row + 0x370, 4);
        if (hit && row[8] != 0) g_bugFaction = row[8];
        hf->log(g_mod, "mission row %u id %08x enabled %u faction %u levels %u..%u optional %u major %u kind %u name-loc %u "
                       "delta %016llx layouts %u (list %#llx)%s",
                r, g_rowId[r], row[4], row[8], lo, hi, opt, major, row[0x34], name, (unsigned long long)delta, layouts,
                (unsigned long long)layoutList, hit ? "" : " [MDBE row number check]");
        if (layoutList && layouts && layouts <= 16)
        {
            uint8_t l[16];
            if (Rd((uintptr_t)layoutList, l, layouts))
            {
                char text[80] = "";
                for (uint32_t i = 0; i < layouts; i++) _snprintf_s(text + strlen(text), sizeof text - strlen(text), _TRUNCATE, " %u", l[i]);
                hf->log(g_mod, "    layouts:%s", text);
            }
        }
        for (int k = 0; k < 32; k++)
        {
            uint32_t e[6];
            memcpy(e, row + 0x40 + k * 0x18, sizeof e);
            if (e[0] == 0) continue;
            float w;
            memcpy(&w, &e[1], 4);
            const char* n = ObjectiveName(e[0]);
            hf->log(g_mod, "    slot %2d objective %08x weight %g ? %u class %u min %u max %u%s%s", k, e[0], (double)w, e[2], e[3], e[4], e[5],
                    n ? "  " : "", n ? n : "");
        }
    }
    hf->log(g_mod, "missions: %u rows list the Dark Fluid / Deactivate TCS objectives; Terminid faction value %u", hits, g_bugFaction);
}

static void LogObjectives(void)
{
    for (size_t i = 0; i < OBJECTIVE_COUNT; i++)
    {
        int found = 0;
        for (uint32_t d = 0; d < OBJECTIVE_ROWS && !found; d++)
        {
            uint8_t row[OBJECTIVE_ROW];
            uint32_t id;
            if (!Rd(g_game + RVA_OBJECTIVES + d * OBJECTIVE_ROW, row, sizeof row)) continue;
            memcpy(&id, row, 4);
            if (id != kObjectives[i].id) continue;
            found = 1;
            uint32_t most, lo, hi, nameLoc, descLoc, badge;
            memcpy(&most, row + 0x20, 4); memcpy(&lo, row + 0x24, 4); memcpy(&hi, row + 0x28, 4);
            memcpy(&nameLoc, row + 0x34, 4); memcpy(&descLoc, row + 0x3C, 4); memcpy(&badge, row + 0x54, 4);
            hf->log(g_mod, "objective %08x %s: row %u, most %u per mission, levels %u..%u, name-loc %u, description-loc %u, badge %u",
                    kObjectives[i].id, kObjectives[i].name, d, most, lo, hi, nameLoc, descLoc, badge);
            char hex[OBJECTIVE_ROW * 2 + 1];
            for (uint32_t b = 0; b < OBJECTIVE_ROW; b++) _snprintf_s(hex + 2 * b, 3, _TRUNCATE, "%02x", row[b]);
            hf->log(g_mod, "    raw %s", hex);
        }
        if (!found) hf->log(g_mod, "objective %08x %s: NOT in the objective table", kObjectives[i].id, kObjectives[i].name);
    }
}

/* A mission's objective record has a counted list before its objective entity.
 * Log the target records and two shipped objectives so the resource associations
 * can be compared with the packaged levels. */
static void LogObjectiveReferences(void)
{
    for (uint32_t i = 0; i < OBJECTIVE_ROWS; ++i)
    {
        uint8_t row[OBJECTIVE_ROW];
        uint32_t id, count;
        uint64_t list, entity;
        if (!Rd(g_game + RVA_OBJECTIVES + i * OBJECTIVE_ROW, row, sizeof row)) continue;
        memcpy(&id, row, sizeof id);
        if (!ObjectiveName(id) && id != 3026836252u && id != 246435402u) continue;
        memcpy(&list, row + 8, sizeof list);
        memcpy(&count, row + 16, sizeof count);
        memcpy(&entity, row + 24, sizeof entity);
        hf->log(g_mod, "objective-ref %08x entity %016llx count %u", id, (unsigned long long)entity, count);
        if (!list || !count || count > 4) continue;
        /* Consecutive two-entry reference lists in this build occupy 32 bytes.
         * Keep the raw words until their field meanings can be checked against live levels. */
        for (uint32_t j = 0; j < count; ++j)
        {
            uint32_t fields[4];
            if (!Rd((uintptr_t)list + j * sizeof fields, fields, sizeof fields)) break;
            hf->log(g_mod, "  ref %u words %08x %08x %08x %08x", j, fields[0], fields[1], fields[2], fields[3]);
        }
    }
}

static void LogOperations(void)
{
    for (uint32_t o = 0; o < OPERATION_ROWS; o++)
    {
        uint8_t row[OPERATION_ROW];
        if (!Rd(g_game + RVA_OPERATIONS + o * OPERATION_ROW, row, sizeof row)) continue;
        uint32_t id;
        memcpy(&id, row, 4);
        int lists = 0;
        char text[512] = "";
        for (int k = 0; k < 32; k++)
        {
            uint32_t mission;
            float w;
            memcpy(&mission, row + 0x38 + 8 * k, 4); memcpy(&w, row + 0x38 + 8 * k + 4, 4);
            if (mission == 0) break;
            int r = -1;
            for (uint32_t i = 0; i < MISSION_ROWS; i++) if (g_rowId[i] == mission) { r = (int)i; break; }
            if (r >= 0 && g_rowHit[r]) lists = 1;
            _snprintf_s(text + strlen(text), sizeof text - strlen(text), _TRUNCATE, " %d:%g", r, (double)w);
        }
        if (lists || row[8] == g_bugFaction)
            hf->log(g_mod, "operation %u id %08x faction %u%s missions (row:weight):%s", o, id, row[8], lists ? " LISTS A PORT MISSION;" : "", text);
    }
}

/* game.dll is loaded after the mod's init; find it once the game has its data. */
static int FindGame(void)
{
    size_t size = 0;
    if (g_game) return 1;
    g_game = hf->module_base("game.dll", &size);
    if (g_game == 0 || size < RVA_MISSIONS + MISSION_ROWS * MISSION_ROW)
    {
        hf->log(g_mod, "game.dll is not loaded as expected (base %#llx, size %#llx)", (unsigned long long)g_game, (unsigned long long)size);
        g_game = 0;
        return 0;
    }
    hf->log(g_mod, "game.dll at %#llx", (unsigned long long)g_game);
    return 1;
}

static DWORD WINAPI ScanKeys(LPVOID unused);

/* 0.2.1: game.dll's unpacked image (the file on disk is packed), written once per session to the logs folder as
 * darkfluid-gamedll.bin, so the stamp selection (GenerateMissionStampList, 0xF8E530) can be disassembled offline.
 * Code is read as it was before HellForge's hooks (read_code); pages that cannot be read are zero. */
static DWORD WINAPI DumpGame(LPVOID unused)
{
    (void)unused;
    size_t size = 0;
    uintptr_t base = hf->module_base("game.dll", &size);
    const char* folder = hf->logs_folder(g_mod);
    char path[1024];
    wchar_t wide[1024];
    if (!base || !size || !folder || !folder[0]) return 0;
    _snprintf_s(path, sizeof path, _TRUNCATE, "%s\\darkfluid-gamedll.bin", folder);
    if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wide, 1024)) return 0;
    HANDLE f = CreateFileW(wide, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (f == INVALID_HANDLE_VALUE) { hf->log(g_mod, "dump: cannot create %s", path); return 0; }
    static uint8_t page[0x1000];
    uint64_t missed = 0;
    for (size_t at = 0; at < size; at += sizeof page)
    {
        size_t n = size - at < sizeof page ? size - at : sizeof page;
        int ok = hf->size >= offsetof(HfApi, read_code) + sizeof(void*) ? hf->read_code(base + at, page, n) == 0 : 0;
        if (!ok && hf->read(base + at, page, n) != 0) { memset(page, 0, n); missed += n; }
        DWORD done = 0;
        WriteFile(f, page, (DWORD)n, &done, NULL);
    }
    CloseHandle(f);
    hf->log(g_mod, "dump: game.dll image (%#llx bytes from %#llx) written to %s, %llu bytes unreadable", (unsigned long long)size,
            (unsigned long long)base, path, (unsigned long long)missed);
    return 0;
}

static void Ready(HfMod* mod, HfEvent event, void* user)
{
    (void)mod; (void)event; (void)user;
    if (!FindGame()) return;
    LogMissions();
    LogObjectives();
    LogObjectiveReferences();
    LogOperations();
    (void)ScanKeys; (void)DumpGame; /* diagnostics of 0.1.2 / 0.2.1, kept for later builds */
}

static uint64_t g_holder;
static uint32_t g_dumps;
static float g_wait;

/* Hex dump of size bytes at address, 0x40 per line, with any u64 that looks like a heap pointer marked. */
static void DumpAt(const char* label, uintptr_t address, uint32_t size)
{
    uint8_t buf[0x200];
    if (size > sizeof buf) size = sizeof buf;
    if (!Rd(address, buf, size)) { hf->log(g_mod, "%s %#llx unreadable", label, (unsigned long long)address); return; }
    hf->log(g_mod, "%s %#llx:", label, (unsigned long long)address);
    for (uint32_t o = 0; o < size; o += 0x40)
    {
        char hex[0x40 * 2 + 1] = "";
        for (uint32_t b = 0; b < 0x40 && o + b < size; b++) _snprintf_s(hex + 2 * b, 3, _TRUNCATE, "%02x", buf[o + b]);
        hf->log(g_mod, "      +%03x %s", o, hex);
    }
}

/* 0.1.2: one pass over the process's committed, readable memory for the stamp keys: is a removed stamp still known to
 * the game anywhere (the stamp settings table), and where do the reference stamps' keys live besides their items. */
#define MAX_HITS 16
static uintptr_t g_hitAt[STAMP_COUNT][MAX_HITS];
static volatile LONG g_scanDone;

static DWORD WINAPI ScanKeys(LPVOID unused)
{
    (void)unused;
    static uint8_t chunk[1 << 20];
    uint32_t hits[STAMP_COUNT] = {0};
    uint64_t scanned = 0;
    HANDLE self = GetCurrentProcess();
    uintptr_t at = 0x10000;
    MEMORY_BASIC_INFORMATION mi;
    while (at < 0x7FFFFFFF0000ull && VirtualQuery((LPCVOID)at, &mi, sizeof mi) == sizeof mi)
    {
        uintptr_t base = (uintptr_t)mi.BaseAddress, end = base + mi.RegionSize;
        DWORD prot = mi.Protect & 0xFF;
        int readable = mi.State == MEM_COMMIT && !(mi.Protect & PAGE_GUARD) &&
                       (prot == PAGE_READONLY || prot == PAGE_READWRITE || prot == PAGE_WRITECOPY || prot == PAGE_EXECUTE_READ ||
                        prot == PAGE_EXECUTE_READWRITE);
        for (uintptr_t p = base; readable && p < end; p += sizeof chunk)
        {
            SIZE_T want = end - p < sizeof chunk ? (SIZE_T)(end - p) : sizeof chunk, got = 0;
            if (p >= (uintptr_t)chunk && p < (uintptr_t)chunk + sizeof chunk) continue; /* our own buffer */
            if (!ReadProcessMemory(self, (LPCVOID)p, chunk, want, &got) || got < 8) continue;
            scanned += got;
            for (SIZE_T o = 0; o + 8 <= got; o += 8)
            {
                uint64_t v;
                memcpy(&v, chunk + o, 8);
                for (size_t k = 0; k < STAMP_COUNT; k++)
                {
                    if (v != kStamps[k].key || hits[k] >= MAX_HITS) continue;
                    uintptr_t where = p + o;
                    g_hitAt[k][hits[k]++] = where;
                    hf->log(g_mod, "scan: %s at %#llx (region %#llx, type %#lx)", kStamps[k].name, (unsigned long long)where,
                            (unsigned long long)base, (unsigned long)mi.Type);
                    if (hits[k] <= 4) DumpAt("      around", where >= 0x40 ? where - 0x40 : where, 0xC0);
                }
            }
        }
        at = end;
    }
    hf->log(g_mod, "scan done: %llu MB read", (unsigned long long)(scanned >> 20));
    for (size_t k = 0; k < STAMP_COUNT; k++) hf->log(g_mod, "    %s: %u hit(s)%s", kStamps[k].name, hits[k], hits[k] >= MAX_HITS ? " (capped)" : "");
    InterlockedExchange(&g_scanDone, 1);
    return 0;
}

static void LogLocations(uint64_t holder)
{
    uint64_t sets = 0;
    uint32_t count = 0, items = 0, seen[STAMP_COUNT] = {0};
    if (!Rd((uintptr_t)holder + 0x38, &sets, 8) || !Rd((uintptr_t)holder + 0xBC, &count, 4) || sets == 0 || count == 0 || count > 256) return;
    for (uint32_t s = 0; s < count; s++)
    {
        uint64_t set = 0, groups = 0;
        uint32_t setId = 0, groupCount = 0;
        if (!Rd((uintptr_t)sets + 8 * (uintptr_t)s, &set, 8) || !Rd((uintptr_t)set, &setId, 4) || !Rd((uintptr_t)set + 0x28, &groups, 8) ||
            !Rd((uintptr_t)set + 0x30, &groupCount, 4) || groups == 0 || groupCount > 256) continue;
        for (uint32_t g = 0; g < groupCount; g++)
        {
            uintptr_t group = (uintptr_t)groups + 0x18 * (uintptr_t)g;
            uint64_t list = 0;
            uint32_t groupId = 0, n = 0;
            if (!Rd(group, &groupId, 4) || !Rd(group + 8, &list, 8) || !Rd(group + 0x10, &n, 4) || list == 0 || n > 4096) continue;
            items += n;
            if (g_dumps == 1)
                hf->log(g_mod, "set %08x group %08x items %u", setId, groupId, n);
            for (uint32_t i = 0; i < n; i++)
            {
                uint8_t item[LOCATION_ITEM];
                uint64_t key;
                if (!Rd((uintptr_t)list + (uintptr_t)i * LOCATION_ITEM, item, sizeof item)) continue;
                memcpy(&key, item + 8, 8);
                for (size_t k = 0; k < STAMP_COUNT; k++)
                {
                    if (key != kStamps[k].key) continue;
                    seen[k]++;
                    uint16_t word;
                    float radius;
                    uint32_t classes = 0;
                    memcpy(&word, item + 0x60, 2); memcpy(&radius, item + 0xD8, 4); memcpy(&classes, item + 0x1A0, 4);
                    hf->log(g_mod, "location: set %08x group %08x item %u of %u: %s; levels %u..%u, radius %.1f, size %u, %u classes",
                            setId, groupId, i, n, kStamps[k].name, (word >> 2) & 0xF, (word >> 6) & 0xF, (double)radius, item[0x15E], classes);
                    if (seen[k] == 1)
                    {
                        const uint32_t offsets[] = {0xC0u, 0x100u, 0x198u};
                        const uint32_t widths[] = {1u, 4u, 1u};
                        for (unsigned a = 0; a < sizeof offsets / sizeof offsets[0]; ++a)
                        {
                            uint64_t values;
                            uint32_t length, first = 0;
                            memcpy(&values, item + offsets[a], sizeof values);
                            memcpy(&length, item + offsets[a] + 8, sizeof length);
                            if (values && length && Rd((uintptr_t)values, &first, widths[a]))
                                hf->log(g_mod, "  reference %016llx field %03x count %u first %08x",
                                        (unsigned long long)key, offsets[a], length, first);
                        }
                        char hex[LOCATION_ITEM * 2 + 1];
                        for (uint32_t b = 0; b < LOCATION_ITEM; b++) _snprintf_s(hex + 2 * b, 3, _TRUNCATE, "%02x", item[b]);
                        hf->log(g_mod, "    raw %s", hex);
                        /* 0.1.2: what the item's pointers lead to (+0x00 differs per stamp: likely its stamp record). */
                        const uint32_t ptrAt[] = {0x00u, 0xC0u, 0x100u, 0x198u};
                        for (unsigned a = 0; a < sizeof ptrAt / sizeof ptrAt[0]; ++a)
                        {
                            uint64_t p;
                            memcpy(&p, item + ptrAt[a], 8);
                            DumpAt(ptrAt[a] == 0 ? "    item+0x00 ->" : "    item list ->", (uintptr_t)p, ptrAt[a] == 0 ? 0x180u : 0x20u);
                        }
                    }
                }
            }
        }
    }
    hf->log(g_mod, "location groups: %u sets, %u items (holder %#llx)", count, items, (unsigned long long)holder);
    for (size_t k = 0; k < STAMP_COUNT; k++)
        hf->log(g_mod, "    %s: %u item(s)", kStamps[k].name, seen[k]);
}

/* ---- stage 2 (0.3.0): restore the Deploy Dark Fluid stamps and offer the mission ---------------------------------------
 *
 * How the generator picks an objective's stamp (game.dll 0xF8E530, GenerateMissionStampList, disassembled from the
 * unpacked image): for each objective id it takes the id's type index (its place in the 158-entry table at
 * game.dll+0x2150030: Deploy Dark Fluid 28, Payload Site 29, Launch ICBM 49), then walks every item of every location
 * group and keeps the items whose stamp key has an entry in the stamp/objective hash map ([lg+0x38]: +0x18 buckets of
 * 0x40 bytes, +0x20 their count, +0x48 the empty key; an entry is 32 type-index bytes, the key at +0x20, a record index
 * and count at +0x28) listing that type index. Which package a stamp loads comes from a second hash map, 16-byte
 * entries {stamp key, package}; its header is found at run time from the drill and ICBM entries.
 *
 * So a removed stamp needs: a stamp/objective entry, a stamp/package entry, and a location item. Items are copies of
 * gen_prime_prospecting_drill_01's (an open-map objective stamp) with the stamp's key and radius (the outer radius of its
 * 2024 collision circles); they keep the drill's collision record (+0x00), so the ground is flattened as for the drill.
 * The stamps' levels and packages come from the zz.darkfluid-resources pack (2024 data converted to this build).
 *
 * Every table is checked on the drill and ICBM entries before anything is written; a failed check writes nothing. */
#define OBJ_ICBM 0xB469DB1Cu
#define OBJ_DEPLOY 0x258AE161u
#define OBJ_PAYLOAD 0x89EE71A8u
#define RVA_OBJ_TYPES 0x2150030u
#define OBJ_TYPES 158u
/* The operation template that carries the mission. Mission picking (game.dll 0x11e5100) first draws a mission kind
 * from the template's quotas (+0x178: 16-byte {kind, min, max, f32 weight}, count +0x208), then takes the template's
 * list missions of that kind (mission row +0x34). Deploy Dark Fluid is kind 3, and the only Terminid template with a
 * kind-3 quota is row 6, ACTIVATE TCS+ (one kind-3 mission per operation; its only one is ACTIVATE TCS+ STATION).
 * The three 2024 TCS / Dark Fluid templates themselves were deleted; only their texts are left. */
#define TEMPLATE_ID 1536944888u        /* row 6 ACTIVATE TCS+ */
#define MISSION_KIND_SPECIAL 3u
#define MAP_PRESET_SPECIAL_SMALL 0x9510800888744760ull /* mission row +0x38 of every kind-3 mission (TCS+ station...) */
#define MAP_PRESET_BUG_STANDARD 0x68C82F6B46E7F5DBull  /* mission row +0x38 of the Terminid standard missions */
#define TEXT_DF_OPERATION 2401299203u   /* "DEPLOY DARK FLUID": the deleted template's name, unreferenced now */
#define TEXT_DF_SUMMARY 3795917234u     /* "Inject Dark Fluid beneath the Supercolony's crust to destroy it once and for all." */
#define PKG_DRILL1 0x11B62F0E71EA927Full /* packages/generated/stamps/gen_prime_prospecting_drill_01 */
#define PKG_ICBM1 0x566E02CCEBC5D3C0ull  /* packages/generated/stamps/gen_prime_launch_icbm_01 */
enum { K_DF1, K_DF2, K_DFPACK, K_TCSA, K_TCSB, K_DRILL1, K_DRILL2, K_ICBM1 };
static const struct { int stamp; uint32_t objective; float radius; uint64_t package; } kPort[] = {
    /* 0.4.0: DF1/DF2 are the payload sites (objective 27, up to 3 per mission, two alternating stamps); the small
     * backpack stamp is the Deploy Dark Fluid objective (26, one per mission), whose vessel stratagem drops within
     * 150 m of it, matching that stamp's 142.9 m outer radius. 0.3.x had these the other way round. */
    {K_DF1, OBJ_PAYLOAD, 187.4f, 0xFB04C61383402693ull},
    {K_DF2, OBJ_PAYLOAD, 165.2f, 0x8AC0FE970E89087Full},
    {K_DFPACK, OBJ_DEPLOY, 142.9f, 0xEFD618BE21CCEEB3ull},
};
#define PORT_COUNT (sizeof kPort / sizeof kPort[0])
/* 0.7.0: every stamp the 2024 settings give the Deploy Dark Fluid / Deactivate TCS objectives (analysis/FULL-MISSION-ANALYSIS.md),
 * with its 2024 collision circles (analysis/stamps-11-20240601.json, from the 2024 game's StampHashMap). Their records come
 * from the settings patch (port_lgs.py); each gets a location item and its own shape record here. */
typedef struct { float x, y, r; } Circle;
static const Circle kC0[] = {{-0.5450f, -28.1114f, 124.1599f}, {82.8730f, 82.0638f, 70.7429f}, {-77.3784f, 80.9033f, 66.5170f}, {-14.9904f, 71.4285f, 70.7429f}};
static const Circle kC1[] = {{-4.2876f, -25.4666f, 80.4674f}, {32.7716f, 25.0146f, 100.3511f}, {-37.8435f, 58.1099f, 74.1093f}, {-63.3409f, 37.7261f, 91.4957f}};
static const Circle kC2[] = {{23.2823f, -112.4768f, 28.0299f}, {3.6897f, -101.8886f, 28.0299f}, {44.6668f, -102.1306f, 28.0299f}, {14.3988f, 47.7421f, 54.4393f}, {-49.8149f, -19.6643f, 54.4393f}, {36.9712f, -27.9860f, 54.4393f}};
static const Circle kC3[] = {{1.4274f, -8.5633f, 23.8770f}, {10.5004f, 16.7973f, 15.6822f}, {-14.2742f, 12.0090f, 20.1133f}, {-12.6358f, -6.8444f, 23.8770f}, {-21.0976f, 26.2811f, 6.6598f}, {3.7092f, 2.3588f, 23.8770f}};
static const Circle kC4[] = {{-11.3425f, 3.7428f, 28.2861f}, {-1.6000f, -12.7000f, 25.4803f}};
static const Circle kC5[] = {{10.3716f, -10.3252f, 18.2476f}, {-5.3386f, -14.5070f, 11.7590f}, {-2.6954f, 9.5561f, 14.7755f}, {6.3000f, 11.9000f, 16.3045f}};
static const Circle kC6[] = {{3.1247f, -0.3449f, 50.3918f}, {1.9995f, 21.8905f, 64.9054f}};
static const Circle kC7[] = {{-3.2000f, 9.4000f, 54.0000f}};
static const Circle kC8[] = {{-113.9332f, 1.6879f, 62.2630f}, {27.5262f, -113.5004f, 52.4549f}, {9.7274f, 7.6619f, 74.4026f}, {0.7000f, 105.1000f, 54.4481f}, {-59.7384f, 98.7591f, 57.5169f}, {26.8485f, -46.6481f, 49.8012f}, {-62.0314f, 32.3382f, 26.4729f}, {-56.4968f, -47.6235f, 37.4953f}, {-0.3002f, -105.8282f, 68.6005f}, {60.1000f, 93.1000f, 60.0821f}};
static const Circle kC9[] = {{-76.5000f, -141.3000f, 42.2855f}, {20.2000f, -49.7000f, 85.9336f}, {78.8000f, 73.8000f, 73.4286f}, {-144.2000f, -87.9000f, 76.5637f}, {0.2000f, -150.2000f, 39.4567f}, {-34.8000f, 69.4000f, 73.4286f}, {-58.8000f, -32.0000f, 104.0849f}, {24.3000f, 94.8000f, 49.9920f}};
static const Circle kC10[] = {{-57.6185f, 86.3509f, 63.7596f}, {65.7721f, 88.6916f, 51.2959f}, {-1.4556f, 53.1444f, 77.1017f}, {22.8237f, -76.7208f, 65.7766f}, {-25.3763f, -80.9484f, 61.8066f}, {-162.8501f, 37.8839f, 32.4843f}};
static const struct { uint64_t key; const char* name; const Circle* circles; uint32_t count; } kPorted[] = {
    {0x0B34988E28A9131Bull, "Deploy Dark Fluid site 1 (fb04)", kC0, 4},
    {0xB10C9886D3006FD4ull, "Deploy Dark Fluid site 2 (8ac0)", kC1, 4},
    {0x2DDAA26E56F2CD4Cull, "Dark Fluid backpack site (efd6)", kC2, 6},
    {0xADAB38F5E35BE4AEull, "payload site, records only (adab38f5)", kC3, 6},
    {0xD0A2CA9932FDA6E7ull, "payload site, records only (d0a2ca99)", kC4, 2},
    {0xBA1D0DDA4936F92Eull, "payload site, records only (ba1d0dda)", kC5, 4},
    {0x74606ED3950F307Eull, "payload site, records only (74606ed3)", kC6, 2},
    {0x80ACA473FBE35102ull, "Deploy Dark Fluid, records only (80aca473)", kC7, 1},
    {0x465AF3D39AB42964ull, "Deactivate TCS site A (271dd)", kC8, 10},
    {0x004F0E2FC6BCDB32ull, "Deactivate TCS site B (e3319)", kC9, 8},
    {0xA04D0A191031ACADull, "Deactivate TCS, records only (a04d0a19)", kC10, 6}};
#define PORTED_COUNT (sizeof kPorted / sizeof kPorted[0])


typedef struct { uint32_t set, id, n; uintptr_t at; uint64_t list; } Group;
static Group g_groups[1024];
static uint32_t g_groupCount;
static int g_ported;

static void ReadGroups(uint64_t holder)
{
    uint64_t sets = 0;
    uint32_t count = 0;
    g_groupCount = 0;
    if (!Rd((uintptr_t)holder + 0x38, &sets, 8) || !Rd((uintptr_t)holder + 0xBC, &count, 4) || !sets || !count || count > 256) return;
    for (uint32_t s = 0; s < count; s++)
    {
        uint64_t set = 0, groups = 0;
        uint32_t setId = 0, n = 0;
        if (!Rd((uintptr_t)sets + 8 * (uintptr_t)s, &set, 8) || !Rd((uintptr_t)set, &setId, 4) || !Rd((uintptr_t)set + 0x28, &groups, 8) ||
            !Rd((uintptr_t)set + 0x30, &n, 4) || !groups || n > 256) continue;
        for (uint32_t g = 0; g < n && g_groupCount < sizeof g_groups / sizeof g_groups[0]; g++)
        {
            Group* e = &g_groups[g_groupCount];
            e->set = setId;
            e->at = (uintptr_t)groups + 0x18 * (uintptr_t)g;
            if (!Rd(e->at, &e->id, 4) || !Rd(e->at + 8, &e->list, 8) || !Rd(e->at + 0x10, &e->n, 4) || e->n > 4096) continue;
            g_groupCount++;
        }
    }
}

/* The group holding an item with this stamp key (and the item), or NULL. */
static Group* FindItem(uint64_t key, uint8_t* item)
{
    for (uint32_t g = 0; g < g_groupCount; g++)
        for (uint32_t i = 0; i < g_groups[g].n && g_groups[g].list; i++)
        {
            uint8_t buf[LOCATION_ITEM];
            uint64_t k;
            if (!Rd((uintptr_t)g_groups[g].list + (uintptr_t)i * LOCATION_ITEM, buf, sizeof buf)) continue;
            memcpy(&k, buf + 8, 8);
            if (k != key) continue;
            if (item) memcpy(item, buf, sizeof buf);
            return &g_groups[g];
        }
    return NULL;
}

static int TypeIndex(uint32_t objective)
{
    for (uint32_t i = 1; i < OBJ_TYPES; i++)
    {
        uint32_t v = 0;
        if (Rd(g_game + RVA_OBJ_TYPES + 4 * i, &v, 4) && v == objective) return (int)i;
    }
    return -1;
}

/* ---- the stamp/objective hash map ---- */
static uint64_t g_omBuckets, g_omEmpty;
static uint32_t g_omCount;

/* The entry holding key, or 0; *slot receives the first free bucket from key % count on (0 if none).
 * Read live (0.5.0): the container's empty marker (+0x48) is ~0, but free buckets hold key 0, and the game's entries sit
 * at (2 * key) % count, not key % count. GenerateMissionStampList probes from key % count and stops only at the key or
 * the marker, so it walks on through free buckets until it meets the key: an entry in any free bucket is found, the
 * first one after key % count soonest. So this walks the whole table the same way. */
static uintptr_t OmFind(uint64_t key, uintptr_t* slot)
{
    if (slot) *slot = 0;
    if (!g_omBuckets || !g_omCount) return 0;
    for (uint32_t p = 0, i = (uint32_t)(key % g_omCount); p < g_omCount; p++, i = (i + 1 == g_omCount) ? 0 : i + 1)
    {
        uintptr_t e = (uintptr_t)g_omBuckets + 0x40 * (uintptr_t)i;
        uint64_t k = *(volatile const uint64_t*)(e + 0x20); /* the game's own heap, in this process */
        if (k == key) return e;
        if (slot && !*slot && (k == 0 || k == g_omEmpty)) *slot = e;
    }
    return 0;
}

static int OmLists(uintptr_t entry, int index)
{
    uint8_t list[32];
    if (!entry || index < 0 || !Rd(entry, list, sizeof list)) return 0;
    for (int b = 0; b < 32; b++) if (list[b] == index) return 1;
    return 0;
}

/* ---- the stamp/package hash map (found from its drill and ICBM entries) ---- */
static uint64_t g_pmBuckets;
static uint32_t g_pmCount;
static volatile LONG g_pmSearched;

static int ReadProc(uintptr_t a, void* out, size_t n)
{
    SIZE_T got = 0;
    return ReadProcessMemory(GetCurrentProcess(), (LPCVOID)a, out, n, &got) && got == n;
}

/* Calls back for every 8-byte-aligned u64 in committed, readable, private memory. */
typedef int (*ScanFn)(uintptr_t at, uint64_t value, void* user);
static void ScanMemory(ScanFn fn, void* user)
{
    static uint8_t chunk[1 << 20];
    uintptr_t at = 0x10000;
    MEMORY_BASIC_INFORMATION mi;
    while (at < 0x7FFFFFFF0000ull && VirtualQuery((LPCVOID)at, &mi, sizeof mi) == sizeof mi)
    {
        uintptr_t base = (uintptr_t)mi.BaseAddress, end = base + mi.RegionSize;
        DWORD prot = mi.Protect & 0xFF;
        if (mi.State == MEM_COMMIT && mi.Type == MEM_PRIVATE && !(mi.Protect & PAGE_GUARD) && (prot == PAGE_READWRITE || prot == PAGE_READONLY))
            for (uintptr_t p = base; p < end; p += sizeof chunk)
            {
                SIZE_T want = end - p < sizeof chunk ? (SIZE_T)(end - p) : sizeof chunk, got = 0;
                if (p >= (uintptr_t)chunk && p < (uintptr_t)chunk + sizeof chunk) continue;
                if (!ReadProcessMemory(GetCurrentProcess(), (LPCVOID)p, chunk, want, &got) || got < 16) continue;
                for (SIZE_T o = 0; o + 8 <= got; o += 8)
                {
                    uint64_t v;
                    memcpy(&v, chunk + o, 8);
                    if (fn(p + o, v, user)) return;
                }
            }
        at = end;
    }
}

/* Both maps are found from entries the game already has (drill_01 and launch_icbm_01):
 *   stamp/package: 16-byte entries {key, package};
 *   stamp/objective: 64-byte entries, 32 type-index bytes then the key (ICBM's list holds 49, the drill's one index).
 * A header is a pointer B to the buckets holding both entries, with a u32 bucket count N right after it (as
 * GenerateMissionStampList reads [r11+0x18] / [r11+0x20]) for which both entries sit within a short probe of key % N.
 * The stamp/objective header gives its container (pointer - 0x18); the empty key is container + 0x48. */
typedef struct { uint64_t key[2], pkg[2]; uintptr_t pm[2], om[2]; int icbmIndex; } MapFind;
static uint64_t g_omHeader;

static int SmallList(uintptr_t at, int must)
{
    uint8_t list[32];
    if (!ReadProc(at, list, sizeof list)) return 0;
    int has = must < 0;
    for (int b = 0; b < 32; b++) { if (list[b] >= OBJ_TYPES) return 0; if (list[b] == must) has = 1; }
    return has && list[0] != 0;
}

static int FindEntries(uintptr_t at, uint64_t v, void* user)
{
    MapFind* f = user;
    for (int i = 0; i < 2; i++)
    {
        if (v != f->key[i]) continue;
        uint64_t next;
        if (!f->pm[i] && ReadProc(at + 8, &next, 8) && next == f->pkg[i]) f->pm[i] = at;
        if (!f->om[i] && at >= 0x20 && (at - 0x20) % 16 == 0 && SmallList(at - 0x20, i == 1 ? f->icbmIndex : -1)) f->om[i] = at - 0x20;
    }
    return f->pm[0] && f->pm[1] && f->om[0] && f->om[1];
}

/* Does B (read at loc) head buckets of `stride` bytes holding both entries, with a fitting count at loc + 8? */
static uint32_t Fits(uintptr_t loc, uint64_t v, const uintptr_t e[2], const uint64_t key[2], uint32_t stride)
{
    uint32_t n = 0;
    for (int i = 0; i < 2; i++) if (!e[i] || v > e[i] || (e[i] - v) % stride || e[i] - v > (uint64_t)stride << 24) return 0;
    if (!ReadProc(loc + 8, &n, 4) || n < 16 || n > (1u << 24)) return 0;
    for (int i = 0; i < 2; i++)
    {
        uint64_t slot = (e[i] - v) / stride;
        if (slot >= n || (slot + n - key[i] % n) % n >= 64) return 0;
    }
    return n;
}

static int FindHeaders(uintptr_t at, uint64_t v, void* user)
{
    MapFind* f = user;
    uint32_t n;
    if (!g_omHeader && (n = Fits(at, v, f->om, f->key, 0x40)))
    {
        g_omHeader = at - 0x18;
        hf->log(g_mod, "port: stamp/objective map: container %#llx, buckets %#llx, %u buckets", (unsigned long long)g_omHeader, (unsigned long long)v, n);
    }
    if (!g_pmBuckets && (n = Fits(at, v, f->pm, f->key, 16)))
    {
        g_pmBuckets = v; g_pmCount = n;
        hf->log(g_mod, "port: stamp/package map: header %#llx, buckets %#llx, %u buckets", (unsigned long long)at, (unsigned long long)v, n);
    }
    return g_omHeader && g_pmBuckets;
}

static DWORD WINAPI SearchPackageMap(LPVOID unused)
{
    (void)unused;
    MapFind f = {{kStamps[K_DRILL1].key, kStamps[K_ICBM1].key}, {PKG_DRILL1, PKG_ICBM1}, {0, 0}, {0, 0}, TypeIndex(OBJ_ICBM)};
    ScanMemory(FindEntries, &f);
    hf->log(g_mod, "port: entries: package drill %#llx ICBM %#llx; objective drill %#llx ICBM %#llx (ICBM type %d)",
            (unsigned long long)f.pm[0], (unsigned long long)f.pm[1], (unsigned long long)f.om[0], (unsigned long long)f.om[1], f.icbmIndex);
    if ((f.pm[0] && f.pm[1]) || (f.om[0] && f.om[1])) ScanMemory(FindHeaders, &f);
    if (!g_pmBuckets) hf->log(g_mod, "port: the stamp/package map's header was not found");
    if (!g_omHeader) hf->log(g_mod, "port: the stamp/objective map's header was not found");
    else DumpAt("port: stamp/objective container", (uintptr_t)g_omHeader, 0x100);
    InterlockedExchange(&g_pmSearched, 1);
    return 0;
}

static uintptr_t PmFindKey(uint64_t key, uintptr_t* slot)
{
    if (slot) *slot = 0;
    if (!g_pmBuckets || !g_pmCount) return 0;
    for (uint32_t p = 0, i = (uint32_t)(key % g_pmCount); p < g_pmCount; p++, i = (i + 1 == g_pmCount) ? 0 : i + 1)
    {
        uintptr_t e = (uintptr_t)g_pmBuckets + 16 * (uintptr_t)i;
        uint64_t k;
        if (!Rd(e, &k, 8)) return 0;
        if (k == key) return e;
        if (k == 0) { if (slot) *slot = e; return 0; }
    }
    return 0;
}

static void OfferMission(void)
{
    uint32_t missionId = 0, faction = 0;
    for (uint32_t r = 0; r < MISSION_ROWS && !missionId; r++)
    {
        uint8_t row[0x40 + 0x18 * 2];
        if (!Rd(g_game + RVA_MISSIONS + r * MISSION_ROW, row, sizeof row)) continue;
        for (int k = 0; k < 2; k++)
        {
            uint32_t id;
            memcpy(&id, row + 0x40 + 0x18 * k, 4);
            if (id == OBJ_DEPLOY) { memcpy(&missionId, row, 4); faction = row[8]; }
        }
    }
    if (!missionId) { hf->log(g_mod, "offer: no mission row lists Deploy Dark Fluid"); return; }
    uintptr_t at = 0;
    uint8_t head[0x30];
    for (uint32_t o = 0; o < OPERATION_ROWS && !at; o++)
    {
        uint32_t id;
        uintptr_t row = g_game + RVA_OPERATIONS + o * OPERATION_ROW;
        if (Rd(row, &id, 4) && id == TEMPLATE_ID) at = row;
    }
    if (!at || !Rd(at, head, sizeof head) || head[8] != faction)
    {
        hf->log(g_mod, "offer: template %u (ACTIVATE TCS+) not found or not Terminid: nothing changed", TEMPLATE_ID); return;
    }
    /* the template must draw exactly one kind-3 mission */
    uint8_t quota[16 * 16];
    uint32_t quotas = 0, special = 0;
    if (!Rd(at + 0x178, quota, sizeof quota) || !Rd(at + 0x208, &quotas, 4) || quotas > 16) { hf->log(g_mod, "offer: quota table unreadable"); return; }
    for (uint32_t q = 0; q < quotas; q++)
    {
        uint32_t kind, lo, hi;
        memcpy(&kind, quota + 16 * q, 4); memcpy(&lo, quota + 16 * q + 4, 4); memcpy(&hi, quota + 16 * q + 8, 4);
        if (kind == MISSION_KIND_SPECIAL && lo >= 1 && hi >= 1) special = 1;
    }
    if (!special) { hf->log(g_mod, "offer: template %u has no kind-3 quota: nothing changed", TEMPLATE_ID); return; }
    /* replace every kind-3 mission of its list with Deploy Dark Fluid, so each operation of it has the mission */
    uint32_t list[32][2], replaced = 0, used = 0;
    if (!Rd(at + 0x38, list, sizeof list)) return;
    for (; used < 32 && list[used][0]; used++)
    {
        uint32_t id = list[used][0];
        if (id == missionId) { replaced++; continue; }
        for (uint32_t r = 0; r < MISSION_ROWS; r++)
        {
            uint8_t m[0x38];
            uint32_t mid;
            if (!Rd(g_game + RVA_MISSIONS + r * MISSION_ROW, m, sizeof m)) continue;
            memcpy(&mid, m, 4);
            if (mid != id) continue;
            if (m[0x34] == MISSION_KIND_SPECIAL)
            {
                if (hf->write(at + 0x38 + 8 * (uintptr_t)used, &missionId, 4) == 0)
                {
                    replaced++;
                    hf->log(g_mod, "offer: template %u list slot %u: mission %08x (row %u) -> Deploy Dark Fluid %08x", TEMPLATE_ID, used, id, r, missionId);
                }
            }
            break;
        }
    }
    if (!replaced && used < 32)
    {
        uint32_t entry[2] = {missionId, 0};
        float weight = 1.0f;
        memcpy(&entry[1], &weight, 4);
        if (hf->write(at + 0x38 + 8 * (uintptr_t)used, entry, sizeof entry) == 0) replaced++;
    }
    if (!replaced) { hf->log(g_mod, "offer: template %u list not changed", TEMPLATE_ID); return; }
    /* This mission's map preset only (mission row +0x38, written into the level seed). Kind-3 missions share the
     * TCS+ station's small map (0x9510800888744760), built for one stamp; Deploy Dark Fluid places two (the backpack
     * site, 142.9 m, and a payload site, 165-187 m). 0.5.2's diagnostics: both objectives placed every time, then
     * SpreadLocations failed and the level was generated again, forever. The 2024 mission was a full 40-minute map, so
     * this row alone gets the Terminid standard preset; the TCS+ station and every other mission keep theirs, and the
     * kind byte (+0x34) stays 3, so template 6's special slot still draws it. */
    for (uint32_t r = 0; r < MISSION_ROWS; r++)
    {
        uintptr_t row = g_game + RVA_MISSIONS + r * MISSION_ROW;
        uint32_t mid = 0;
        uint64_t preset = 0, standard = MAP_PRESET_BUG_STANDARD;
        if (!Rd(row, &mid, 4) || mid != missionId || !Rd(row + 0x38, &preset, 8)) continue;
        if (preset == MAP_PRESET_SPECIAL_SMALL && hf->write(row + 0x38, &standard, 8) == 0)
            hf->log(g_mod, "offer: mission row %u map preset %016llx -> %016llx (the Terminid standard map, this mission only)", r,
                    (unsigned long long)preset, (unsigned long long)standard);
        else
            hf->log(g_mod, "offer: mission row %u map preset %016llx left as it is", r, (unsigned long long)preset);
        break;
    }
    /* the deleted 2024 operation's own name and summary (template +0x24 name, +0x2C summary; +0x28 briefing kept) */
    uint32_t name = TEXT_DF_OPERATION, summary = TEXT_DF_SUMMARY;
    int texts = hf->write(at + 0x24, &name, 4) == 0 && hf->write(at + 0x2C, &summary, 4) == 0;
    hf->log(g_mod, "offer: Deploy Dark Fluid (mission %08x) is template %u's special mission%s. Operations built from now on carry it: "
                   "set a Terminid planet's template to 6 (host-ops-test F11) to rebuild its operation", missionId, TEMPLATE_ID,
            texts ? ", operation renamed DEPLOY DARK FLUID" : "");
}

static void Port(uint64_t holder, uint64_t lg)
{
    g_ported = 1; /* one attempt per session; every refusal is logged */
    ReadGroups(holder);
    uint8_t drill[LOCATION_ITEM];
    Group* drillGroup = FindItem(kStamps[K_DRILL1].key, drill);
    int idxIcbm = TypeIndex(OBJ_ICBM);
    if (!drillGroup || idxIcbm < 0) { hf->log(g_mod, "port: reference drill item or ICBM type index missing: nothing changed"); return; }
    /* The stamp/objective map, checked on the ICBM and drill entries. */
    uint64_t om = g_omHeader;
    (void)lg;
    if (!om || !Rd((uintptr_t)om + 0x18, &g_omBuckets, 8) || !Rd((uintptr_t)om + 0x20, &g_omCount, 4) ||
        !Rd((uintptr_t)om + 0x48, &g_omEmpty, 8) || !g_omBuckets || !g_omCount || g_omCount > (1u << 22))
    {
        hf->log(g_mod, "port: no stamp/objective map: nothing changed"); return;
    }
    uintptr_t icbmEntry = OmFind(kStamps[K_ICBM1].key, NULL), drillEntry = OmFind(kStamps[K_DRILL1].key, NULL);
    if (!OmLists(icbmEntry, idxIcbm) || !drillEntry)
    {
        hf->log(g_mod, "port: the stamp/objective map does not hold the ICBM stamp under type %d (entry %#llx) or the drill (entry %#llx): "
                       "nothing changed", idxIcbm, (unsigned long long)icbmEntry, (unsigned long long)drillEntry);
        return;
    }
    uint64_t drillPkgEntry = PmFindKey(kStamps[K_DRILL1].key, NULL), icbmPkgEntry = PmFindKey(kStamps[K_ICBM1].key, NULL), pkg = 0;
    if (!drillPkgEntry || !icbmPkgEntry || !Rd((uintptr_t)drillPkgEntry + 8, &pkg, 8) || pkg != PKG_DRILL1)
    {
        hf->log(g_mod, "port: the stamp/package map does not resolve the drill and ICBM stamps: nothing changed"); return;
    }
    hf->log(g_mod, "port: checks passed: stamp/objective map %#llx (%u buckets, ICBM listed under type %d), stamp/package map %u buckets",
            (unsigned long long)om, g_omCount, idxIcbm, g_pmCount);

    uint32_t done = 0;
    for (size_t p = 0; p < PORT_COUNT; p++)
    {
        int k = kPort[p].stamp, index = TypeIndex(kPort[p].objective);
        uint64_t key = kStamps[k].key;
        uintptr_t omSlot = 0, pmSlot = 0;
        uintptr_t omHave = OmFind(key, &omSlot), pmHave = PmFindKey(key, &pmSlot);
        Group* itemHave = FindItem(key, NULL);
        if (index < 0 || (!omHave && !omSlot) || (!pmHave && !pmSlot))
        {
            hf->log(g_mod, "port: %s: no type index or no free bucket: skipped", kStamps[k].name); continue;
        }
        /* 1. stamp -> package */
        if (!pmHave)
        {
            uint64_t e[2] = {key, kPort[p].package};
            if (hf->write(pmSlot, e, sizeof e) != 0) { hf->log(g_mod, "port: %s: package entry refused", kStamps[k].name); continue; }
        }
        /* 2. stamp -> objective type (a copy of the drill's entry: its record index and count cleared) */
        if (!omHave)
        {
            uint8_t e[0x40];
            if (!Rd(drillEntry, e, sizeof e)) continue;
            uint32_t zero = 0;
            memset(e, 0, 32);
            e[0] = (uint8_t)index;
            memcpy(e + 0x20, &key, 8);
            memcpy(e + 0x28, &zero, 4); memcpy(e + 0x2C, &zero, 4);
            /* the key last, so a reader never sees a half-written entry under this key */
            if (hf->write(omSlot, e, 0x20) != 0 || hf->write(omSlot + 0x28, e + 0x28, 0x18) != 0 || hf->write(omSlot + 0x20, e + 0x20, 8) != 0)
            {
                hf->log(g_mod, "port: %s: objective entry refused", kStamps[k].name); continue;
            }
        }
        /* 3. a location item in the drill's group */
        if (!itemHave)
        {
            uint8_t item[LOCATION_ITEM];
            float zero = 0.0f, radius = kPort[p].radius;
            memcpy(item, drill, sizeof item);
            memcpy(item + 8, &key, 8);
            memcpy(item + 0xD8, &radius, 4); memcpy(item + 0xDC, &zero, 4); memcpy(item + 0xE0, &zero, 4); memcpy(item + 0xE4, &radius, 4);
            uint32_t n = drillGroup->n;
            uint8_t* fresh = VirtualAlloc(NULL, (size_t)(n + 1) * LOCATION_ITEM, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
            if (!fresh || !Rd((uintptr_t)drillGroup->list, fresh, (size_t)n * LOCATION_ITEM)) { if (fresh) VirtualFree(fresh, 0, MEM_RELEASE); continue; }
            memcpy(fresh + (size_t)n * LOCATION_ITEM, item, LOCATION_ITEM);
            uint64_t list = (uint64_t)(uintptr_t)fresh;
            uint32_t count = n + 1;
            if (hf->write(drillGroup->at + 8, &list, 8) != 0 || hf->write(drillGroup->at + 0x10, &count, 4) != 0)
            {
                hf->log(g_mod, "port: %s: location item refused", kStamps[k].name); continue;
            }
            drillGroup->list = list; drillGroup->n = count;
        }
        done++;
        hf->log(g_mod, "port: %s restored: objective type %d, package %016llx, radius %.1f, group %08x", kStamps[k].name, index,
                (unsigned long long)kPort[p].package, (double)kPort[p].radius, drillGroup->id);
    }
    if (done == PORT_COUNT) OfferMission(); /* the backpack site and both payload-site stamps */
    else hf->log(g_mod, "offer: the Deploy Dark Fluid sites are not restored, so the mission is not offered");
}

/* ---- 0.5.0: no memory search -------------------------------------------------------------------------------------
 *
 * 0.3/0.4 looked for both maps by scanning memory from the ship and never found them: the stamp/objective map that
 * GenerateMissionStampList reads is the pass context's ([pass + 0x38], read at 0xF8E567), and the stamp/package map's
 * layout is not a plain key % count table. So:
 *   1. location items: copies of drill_01's item with the stamp's key and radius, appended to the drill's group (from
 *      the ship, as before; the game's own items are not changed);
 *   2. stamp packages: loaded with hf->load_package and held for the session, instead of a stamp/package entry;
 *   3. stamp/objective entries: a hook on GenerateMissionStampList (0xF8E530) inserts them into [pass + 0x38] just
 *      before the game reads it (bucket key % count, linear probing, as at 0xF8E700), so every map generated gets them.
 * The mission is offered (template 6) once the items are placed, the hook is in and the packages are resident. */
#define RVA_GEN_STAMP_LIST 0xF8E530u
#define GEN_STAMP_LIST_BYTES "48 89 54 24 10 48 89 4C 24 08 56 41 54 41 56 48 81 EC 00 01 00 00"
typedef void (*GenStampListFn)(void* rcx, void* rdx, void* pass);
static GenStampListFn g_genOriginal;
static volatile LONG g_genCalls;
/* The current game's bundles holding what the three stamps' 2024 packages list and the game still has (rocks, hive
 * props, markers...): 672 resources, covered greedily from index_25480438 (analysis, 2026-11-26). The ported resources
 * themselves are in the zz.darkfluid-resources patch of the boot bundle and load at start. A package id is a bundle
 * name in this engine: requesting the 2024 ids (0.5.0's first try) made it open bundles that do not exist (0x42554E444C45). */
static const uint64_t kDepBundles[] = {
    0x08E7B8B93E0DC696ull, 0x6B461D0AFD99A5E0ull, 0xC42E3747EFFBFB33ull, 0xD87885C7EAACE597ull,
    0xDA5AEDA5F9638F67ull, 0xDE8ECCC7AA419FC2ull, 0x4FE8DAED70460F7Dull, 0x0DC8FCABDB02111Dull,
    0x3752BFA96CAD0109ull, 0x6FA8FB795E82246Dull, 0xF35C71C092CB956Dull, 0xF6EA02942C173DA0ull,
    0xA97433C287126414ull, 0xB2580B3085C35E37ull, 0xB07E45F469CD5847ull, 0x90A7565630084517ull,
    0xEB620E7A413BDD5Cull, 0x7F4696055D63DCE0ull, 0x7ED1F941859987B4ull, 0xDC179F3D17D27B3Cull,
    0x039FB224D76CCAB4ull, 0x8A895F6DE63C7FC0ull, 0x560C9DE4C61805F2ull, 0x8749FDA0474D5495ull,
    0xF3F9846F08C98455ull, 0x3B2C86DD5B316EB2ull, 0xD357DFDFCDAA6979ull,
};
#define DEP_COUNT (sizeof kDepBundles / sizeof kDepBundles[0])
static int g_itemsPlaced, g_offered, g_hookTried, g_pkgState[DEP_COUNT], g_pkgRequested, g_pkgWaitLogged, g_pkgResidentLogged = -1;

#define OBJ_TCS_STATION 0x13E63075u /* Activate Termicide Tower (TCS+ station): its stamps fit the kind-3 small map */
/* The location-group holder of the level this pass generates: pass+0x18 = lg, [lg+0x2D0] = holder. */
static uint64_t HolderOf(uintptr_t pass)
{
    uint64_t lg = 0, holder = 0;
    if (!pass || !Rd(pass + 0x18, &lg, 8) || !lg || !Rd((uintptr_t)lg + 0x2D0, &holder, 8)) return 0;
    return holder;
}
static void PlaceItems(uintptr_t pass, uint64_t templateKey);

/* Inserts the ported stamps' entries into the stamp/objective map of this generation pass. */
static void InsertObjectiveEntries(uintptr_t pass)
{
    LONG call = InterlockedIncrement(&g_genCalls);
    uint64_t c = 0;
    if (!pass || !Rd(pass + 0x38, &c, 8) || !c || !Rd((uintptr_t)c + 0x18, &g_omBuckets, 8) || !Rd((uintptr_t)c + 0x20, &g_omCount, 4) ||
        !Rd((uintptr_t)c + 0x48, &g_omEmpty, 8) || !g_omBuckets || !g_omCount || g_omCount > (1u << 24))
    {
        if (call <= 3) hf->log(g_mod, "gen %ld: no stamp/objective map at pass %#llx (container %#llx): nothing inserted", call,
                               (unsigned long long)pass, (unsigned long long)c);
        return;
    }
    uintptr_t drillEntry = OmFind(kStamps[K_DRILL1].key, NULL), icbmEntry = OmFind(kStamps[K_ICBM1].key, NULL);
    int idxIcbm = TypeIndex(OBJ_ICBM);
    if (!drillEntry || !OmLists(icbmEntry, idxIcbm))
    {
        if (call <= 3) hf->log(g_mod, "gen %ld: map %#llx (%u buckets, empty key %016llx) lacks the drill (%#llx) or the ICBM under type %d (%#llx): "
                               "nothing inserted", call, (unsigned long long)g_omBuckets, g_omCount, (unsigned long long)g_omEmpty,
                               (unsigned long long)drillEntry, idxIcbm, (unsigned long long)icbmEntry);
        return;
    }
    /* 0.5.1: the template is the TCS+ station's stamp (Activate Termicide Tower, objective 13e63075), whose stamps the
     * kind-3 missions' small map places; drill_01's (a standard-map stamp) were chosen as candidates there but never
     * placed. Its entry gives the record index / count (+0x28..), its location item the new items' class, shape and
     * group. */
    /* 0.5.4: the template is the Launch ICBM stamp (gen_prime_launch_icbm_01), the main objective of the Terminid
     * standard map, which this mission now uses (its own map preset, 0.5.3). 0.5.1-0.5.3 used the TCS+ station's
     * stamp (flags 0x101, centre 0,0: the centrepiece of its own small map): both Dark Fluid objectives were placed,
     * then SpreadLocations failed on every map, likely two locations pinned at the centre. */
    uintptr_t tcsEntry = icbmEntry;
    ReadGroups(HolderOf(pass));
    if (!FindItem(kStamps[K_ICBM1].key, NULL))
    {
        if (call <= 3) hf->log(g_mod, "gen %ld: the ICBM stamp has no location item: nothing inserted", call);
        return;
    }
    uint64_t tcsKey = *(volatile const uint64_t*)(tcsEntry + 0x20);
    if (!g_itemsPlaced) PlaceItems(pass, tcsKey);
    uint8_t drill[0x40];
    if (!Rd(tcsEntry, drill, sizeof drill)) return;
    uint32_t added = 0, had = 0;
    for (size_t p = 0; p < PORTED_COUNT; p++)
    {
        uint64_t key = kPorted[p].key;
        int index = 0;
        uintptr_t slot = 0, have = OmFind(key, &slot);
        if (index < 0) continue;
        if (have)
        {
            had++;
            if (call <= 3)
            {
                uint8_t types[4] = {0};
                uint32_t r[6] = {0};
                Rd(have, types, 4); Rd(have + 0x28, r, sizeof r);
                hf->log(g_mod, "gen %ld: %s: from the settings patch: types %u %u, records %u+%u, names %u+%u", call, kPorted[p].name,
                        types[0], types[1], r[0], r[1], r[4], r[5]);
            }
            continue;
        }
        /* 0.6.0: the entries come with the stamps' own 2024 records from the settings patch (port_lgs.py, the boot
         * bundle's 9ba626afa44a3aa3.patch_0 in zz.darkfluid-resources). Without it nothing is made up any more. */
        if (call <= 3) hf->log(g_mod, "gen %ld: %s is not in the stamp settings: the settings patch is not loaded", call, kPorted[p].name);
        continue;
        if (!slot) { if (call <= 3) hf->log(g_mod, "gen %ld: %s: no free bucket", call, kPorted[p].name); continue; }
        uint8_t e[0x40];
        memcpy(e, drill, sizeof e); /* the TCS+ stamp's record index / count (+0x28..): its records, as the item's */
        memset(e, 0, 32);
        e[0] = (uint8_t)index;
        memcpy(e + 0x20, &key, 8);
        /* 0.5.9: no record ranges. +0x28/+0x2C and +0x38/+0x3C index the template stamp's own ground records (ICBM:
         * 122 and 17, TCS+ station: 991 and 252, sized with the stamp); with the ICBM's, the Dark Fluid sites got its
         * ground paint (purple in this biome) and its blockers (invisible walls that match nothing there). */
        memset(e + 0x28, 0, 0x18);
        /* the key last, so the probe never meets a half-written entry under this key */
        if (hf->write(slot, e, 0x20) != 0 || hf->write(slot + 0x28, e + 0x28, 0x18) != 0 || hf->write(slot + 0x20, e + 0x20, 8) != 0) continue;
        added++;
    }
    if (call <= 3 || added)
        hf->log(g_mod, "gen %ld: stamp/objective map %#llx (%u buckets): %u added, %u already there; location items %s", call,
                (unsigned long long)g_omBuckets, g_omCount, added, had, g_itemsPlaced == 1 ? "placed" : "NOT placed");
}

/* After the game's pass: what it chose. rcx: [rcx+0x18] the mission stamp list (0x10 each: set, group, item, objective
 * id), [rcx+0x20] the objective list S (+4 ids, +0xD4 count; stamp starts/counts at S-0x78/S-0xF0 with HellForge's
 * 30-objective layout, S+0x54/S+0x94 without); pass+8 the level's radii. Logged for the first 12 passes. */
static void LogStampChoice(uintptr_t rcx, uintptr_t pass, LONG call)
{
    if (call > 12 && call % 25) return;
    uint64_t list = 0, s = 0, radii = 0;
    uint32_t n = 0;
    if (!Rd(rcx + 0x18, &list, 8) || !Rd(rcx + 0x20, &s, 8) || !s || !Rd((uintptr_t)s + 0xD4, &n, 4) || n > 30) return;
    float r[8] = {0};
    if (Rd(pass + 8, &radii, 8) && radii) Rd((uintptr_t)radii, r, sizeof r);
    hf->log(g_mod, "gen %ld result: %u objective(s); radii %.1f %.1f %.1f %.1f %.1f %.1f %.1f %.1f", call, n, (double)r[0], (double)r[1],
            (double)r[2], (double)r[3], (double)r[4], (double)r[5], (double)r[6], (double)r[7]);
    uint32_t total = 0;
    for (uint32_t i = 0; i < n; i++)
    {
        uint32_t id = 0, sa = 0, ca = 0, sb = 0, cb = 0;
        Rd((uintptr_t)s + 4 + 4 * i, &id, 4);
        Rd((uintptr_t)s - 0x78 + 4 * i, &sa, 4); Rd((uintptr_t)s - 0xF0 + 4 * i, &ca, 4);
        Rd((uintptr_t)s + 0x54 + 4 * i, &sb, 4); Rd((uintptr_t)s + 0x94 + 4 * i, &cb, 4);
        const char* name = id == OBJ_DEPLOY ? " DEPLOY DARK FLUID" : id == OBJ_PAYLOAD ? " PAYLOAD SITE" : "";
        hf->log(g_mod, "    objective %u id %08x%s: stamps start/count %u/%u (30-layout) or %u/%u (game layout)", i, id, name, sa, ca, sb, cb);
        if (ca < 1024 && sa + ca > total) total = sa + ca;
    }
    for (uint32_t k = 0; k < total && k < 48 && list; k++)
    {
        uint32_t e[4] = {0};
        if (!Rd((uintptr_t)list + 16 * (uintptr_t)k, e, sizeof e)) break;
        uint64_t key = 0;
        /* the item's stamp key, from set e[0] / group e[1] / item e[2] is not resolvable here; ids only */
        (void)key;
        hf->log(g_mod, "    stamp %u: set %u group %u item %u objective %08x", k, e[0], e[1], e[2], e[3]);
    }
}

static void GenStampListDetour(void* rcx, void* rdx, void* pass)
{
    InsertObjectiveEntries((uintptr_t)pass);
    g_genOriginal(rcx, rdx, pass);
    LogStampChoice((uintptr_t)rcx, (uintptr_t)pass, g_genCalls);
}

/* ---- 0.5.2 diagnostics: which later pass rejects the stamps ----
 * GenerateObjectiveLocations (0xF8F1D0) adds one place per placement attempt (count at block+0x11A7170, block =
 * [lg+0x2C8], lg = pass+0x18: HellForge's difficulty.c); SpreadLocations (0xF7C310) spreads them and on failure the level
 * is generated again. Both are only observed. */
#define RVA_GEN_OBJECTIVES 0xF8F1D0u
#define GEN_OBJECTIVES_BYTES "48 89 5C 24 20 55 56 57 41 54 41 55 41 56 41 57"
#define RVA_SPREAD 0xF7C310u
#define SPREAD_BYTES "48 8B C4 4C 89 40 18 48 89 50 10 48 89 48 08 55"
typedef void (*PassFn)(void* rcx, void* rdx, void* pass);
static PassFn g_objOriginal, g_spreadOriginal;
static volatile LONG g_objCalls, g_spreadCalls;

static uint64_t BlockOf(uintptr_t pass)
{
    uint64_t lg = 0, block = 0;
    if (!pass || !Rd(pass + 0x18, &lg, 8) || !lg || !Rd((uintptr_t)lg + 0x2C8, &block, 8)) return 0;
    return block;
}

static void ObjectivesDetour(void* rcx, void* rdx, void* pass)
{
    uint64_t block = BlockOf((uintptr_t)pass);
    uint32_t before = 0, after = 0;
    if (block) Rd((uintptr_t)block + 0x11A7170, &before, 4);
    g_objOriginal(rcx, rdx, pass);
    LONG c = InterlockedIncrement(&g_objCalls);
    if (c > 24 && c % 25) return;
    if (block) Rd((uintptr_t)block + 0x11A7170, &after, 4);
    uint64_t s = 0;
    uint32_t n = 0;
    uint8_t usedA[32] = {0}, usedB[16] = {0};
    if (Rd((uintptr_t)rcx + 0x20, &s, 8) && s && Rd((uintptr_t)s + 0xD4, &n, 4) && n <= 30)
    {
        Rd((uintptr_t)s + 0x7C, usedA, n);
        Rd((uintptr_t)s + 0x44, usedB, n < 16 ? n : 16);
    }
    char a[100] = {0}, b[50] = {0};
    for (uint32_t i = 0; i < n && i < 30; i++) { sprintf(a + 3 * i, "%02x ", usedA[i]); if (i < 16) sprintf(b + 3 * i, "%02x ", usedB[i]); }
    hf->log(g_mod, "objectives pass %ld (gen %ld): places %u -> %u; %u objective(s), used flags (30-layout) %s/ (game) %s", c, g_genCalls,
            before, after, n, a, b);
}

static void SpreadDetour(void* rcx, void* rdx, void* pass)
{
    LONG c = InterlockedIncrement(&g_spreadCalls);
    g_spreadOriginal(rcx, rdx, pass);
    if (c <= 24) hf->log(g_mod, "spread pass %ld done (gen %ld, objectives pass %ld)", c, g_genCalls, g_objCalls);
}

/* ---- 0.5.6: the entity queue (game.dll 0xFDA650) ----
 * At mission start it walks a 4096-entry queue at [mgr + 0xF2AEF8] ({i32 id, u32 unit handle}, id -1 = empty), reads each
 * unit's "entity" script data (key 0x21FE2A86, through EngineApi +0xB8: fn = [[domain] + 0x10](out, unit, 1, key),
 * returning out whose first field is the string), hashes it and spawns the entity (SpawnEntity 0xFDC140). A unit whose
 * lookup returns no string crashed it at 0xFDA6E7 (0.5.5). This runs the same lookup first, logs every such unit (handle,
 * and the unit object's first 0x100 bytes, to find its resource) and marks its entry empty, so the game skips it. */
#define RVA_ENTITY_QUEUE 0xFDA650u
#define ENTITY_QUEUE_BYTES "4C 8B DC 49 89 73 20 55 41 54 41 55 41 56 41 57"
#define ENTITY_KEY 0x21FE2A86u
typedef void* (*EntityQueueFn)(void* mgr, void* rdx, void* r8, void* r9);
typedef void* (*ScriptDataFn)(void* out, uint32_t unit, uint32_t count, uint32_t key);

/* Unit handle -> resource, recorded at World.spawn_unit (helldivers2.exe WorldSpawnUnit: +0x08 of the unit object is its
 * handle), so a queued unit that no longer exists can still be named. */
static struct { uint32_t handle; uint64_t resource; } g_spawned[16384];
static HfFn_WorldSpawnUnit g_spawnOriginal;
static int g_resOffset = -1;

static void* SpawnUnitDetour(void* world, const uint64_t* resource, const uint64_t* name, const float* pose, uint32_t s5, uint64_t s6, uint8_t s7, uint64_t s8)
{
    void* uo = g_spawnOriginal(world, resource, name, pose, s5, s6, s7, s8);
    if (uo && resource && g_resOffset == -1)
    {
        /* where the unit object keeps its resource name, to name units the guard skips */
        for (int o = 0x10; o < 0x400; o += 8)
        {
            uint64_t v = 0;
            if (Rd((uintptr_t)uo + o, &v, 8) && v == *resource) { g_resOffset = o; break; }
        }
        if (g_resOffset < 0) g_resOffset = -2; /* not found in the first unit: give up */
    }
    if (uo && resource)
    {
        uint32_t h = *(volatile uint32_t*)((uintptr_t)uo + 8);
        g_spawned[h & 16383].handle = h;
        g_spawned[h & 16383].resource = *resource;
    }
    return uo;
}

/* 0.5.8: a guard on the crash itself instead of a scan at the function's entry (0.5.7's scan misread the queue). At
 * 0xFDA6DA the loop takes the unit's "entity" string (mov rcx,[rax]; mov rax,-1) and measures it; a unit without one
 * gives NULL there. The 10 bytes jump to a stub that, for NULL, records the unit (edi) and goes on with the next entry
 * (0xFDAAB6: add rsi,8 ...), else does the same two instructions and goes back to the length loop (0xFDA6E4). */
#define RVA_ENTITY_STRING 0xFDA6DAu
#define ENTITY_STRING_BYTES "48 8B 08 48 C7 C0 FF FF FF FF 48 FF C0 44"
#define RVA_ENTITY_NEXT 0xFDAAB6u
#define RVA_ENTITY_STRLEN 0xFDA6E4u
static volatile uint32_t g_nullEntity[2 + 64]; /* [1] how many units had no entity string, [2 + n % 64] their handles */
static uint32_t g_nullLogged;

static uint8_t* AllocNear(uintptr_t target, size_t size)
{
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    uintptr_t g = si.dwAllocationGranularity;
    for (uintptr_t d = g; d < 0x7FF00000ull; d += g)
    {
        uintptr_t tries[2] = {(target - d) & ~(g - 1), (target + d) & ~(g - 1)};
        for (int k = 0; k < 2; k++)
        {
            void* p = VirtualAlloc((void*)tries[k], size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
            if (p) return (uint8_t*)p;
        }
    }
    return NULL;
}

static void GuardEntityString(void)
{
    uintptr_t site = g_game + RVA_ENTITY_STRING;
    if (!hf->code_matches(site, ENTITY_STRING_BYTES)) { hf->log(g_mod, "entity string guard: not the reviewed code: not placed"); return; }
    uint8_t* stub = AllocNear(site, 0x1000);
    if (!stub) { hf->log(g_mod, "entity string guard: no memory near game.dll: not placed"); return; }
    uint8_t c[80];
    size_t n = 0;
    uint64_t next = g_game + RVA_ENTITY_NEXT, strl = g_game + RVA_ENTITY_STRLEN, slot = (uint64_t)(uintptr_t)g_nullEntity;
    const uint8_t a1[] = {0x48, 0x8B, 0x08, 0x48, 0x85, 0xC9, 0x75, 0x25}; /* mov rcx,[rax]; test rcx,rcx; jnz ok (+0x25: 10+13+14) */
    memcpy(c + n, a1, sizeof a1); n += sizeof a1;
    c[n++] = 0x48; c[n++] = 0xB8; memcpy(c + n, &slot, 8); n += 8;        /* mov rax, &g_nullEntity      (10) */
    c[n++] = 0x8B; c[n++] = 0x48; c[n++] = 0x04;                           /* mov ecx, [rax+4]            (3)  */
    c[n++] = 0x83; c[n++] = 0xE1; c[n++] = 0x3F;                           /* and ecx, 63                 (3)  */
    c[n++] = 0x89; c[n++] = 0x7C; c[n++] = 0x88; c[n++] = 0x08;            /* mov [rax+rcx*4+8], edi      (4)  */
    c[n++] = 0xFF; c[n++] = 0x40; c[n++] = 0x04;                           /* inc dword [rax+4]           (3)  */
    c[n++] = 0xFF; c[n++] = 0x25; memset(c + n, 0, 4); n += 4; memcpy(c + n, &next, 8); n += 8; /* jmp [rip] -> next (14) */
    const uint8_t a2[] = {0x48, 0xC7, 0xC0, 0xFF, 0xFF, 0xFF, 0xFF};       /* ok: mov rax,-1 */
    memcpy(c + n, a2, sizeof a2); n += sizeof a2;
    c[n++] = 0xFF; c[n++] = 0x25; memset(c + n, 0, 4); n += 4; memcpy(c + n, &strl, 8); n += 8; /* jmp [rip] -> length loop */
    memcpy(stub, c, n);
    FlushInstructionCache(GetCurrentProcess(), stub, n);
    int64_t rel = (int64_t)(uintptr_t)stub - (int64_t)(site + 5);
    if (rel > INT32_MAX || rel < INT32_MIN) { hf->log(g_mod, "entity string guard: stub out of reach: not placed"); return; }
    uint8_t patch[10] = {0xE9, 0, 0, 0, 0, 0x90, 0x90, 0x90, 0x90, 0x90};
    int32_t r32 = (int32_t)rel;
    memcpy(patch + 1, &r32, 4);
    if (hf->write(site, patch, sizeof patch) == 0) hf->log(g_mod, "entity string guard placed at game.dll+%#x (stub %p)", RVA_ENTITY_STRING, (void*)stub);
    else hf->log(g_mod, "entity string guard: write refused");
}

/* From the game thread: name the units the guard skipped (their resource, read from the unit object). */
static void ReportNullEntities(void)
{
    uint32_t count = g_nullEntity[1];
    if (count == g_nullLogged) return;
    HfFn_UnitFromHandle unitOf = hf_fn_UnitFromHandle(g_mod);
    uint32_t from = count - g_nullLogged > 64 ? count - 64 : g_nullLogged;
    for (uint32_t k = from; k < count; k++)
    {
        uint32_t unit = g_nullEntity[2 + (k & 63)];
        uint64_t res = 0;
        void* uo = unitOf ? unitOf(unit) : NULL;
        if (uo && g_resOffset >= 0) Rd((uintptr_t)uo + (uintptr_t)g_resOffset, &res, 8);
        if (!res && g_spawned[unit & 16383].handle == unit) res = g_spawned[unit & 16383].resource;
        hf->log(g_mod, "entity string guard: skipped #%u unit %08x: unit resource %016llx%s", k + 1, unit, (unsigned long long)res,
                uo ? "" : " (unit gone)");
    }
    g_nullLogged = count;
}

/* ---- 0.7.2: which Dark Fluid entities spawn, and whether the game ever takes up the drill stratagem.
 * The payload objective (0f19...) grants TCS03 Thumper within 12 m; the drill never showed. These say whether the
 * objective entity is spawned at all, whether the drill's packages are ever acquired (StratagemPackages, which the
 * game calls when a stratagem enters a loadout) and whether the picker counts the drill as available. */
static HfFn_SpawnEntity g_spawnEntOriginal;
static HfFn_StratagemPackages g_stratPkgOriginal;
static const struct { uint64_t res; const char* name; } kWatch[] = {
    {0xBA58A7667AA11627ull, "Deploy Dark Fluid objective (ba58)"},
    {0x0F19B7B633E9AA3Cull, "Payload Site objective (0f19, drill stratagem within 12 m)"},
    {0xB127552416CE512Eull, "drill delivery (b127, TCS03 Thumper)"},
    {0x073270650F859DD0ull, "Dark Fluid backpack (0732)"},
    {0x0E4E0A4947D5BFBEull, "vessel (0e4e)"}};
#define WATCH_N (sizeof kWatch / sizeof kWatch[0])
static volatile LONG g_watchCount[WATCH_N];

static uint64_t SpawnEntityDetour(void* manager, uint32_t entity, uint64_t resource, const uint8_t* info, uint64_t s5, uint64_t s6)
{
    uint64_t r = g_spawnEntOriginal(manager, entity, resource, info, s5, s6);
    for (size_t i = 0; i < WATCH_N; i++)
        if (kWatch[i].res == resource)
        {
            LONG n = InterlockedIncrement(&g_watchCount[i]);
            if (n <= 8)
                hf->log(g_mod, "spawned %s: entity %u, authority %u (#%ld)", kWatch[i].name, entity, info ? info[0] : 0u, n);
        }
    return r;
}

static uint32_t StratagemPackagesDetour(void* rcx, uint32_t type, uint64_t unused, uint32_t r9)
{
    uint32_t r = g_stratPkgOriginal(rcx, type, unused, r9);
    if (type == 64u || type == 29u)
        hf->log(g_mod, "stratagem packages: type %u (%s), r9 %u -> %u", type, type == 64u ? "TCS03 drill" : "Dark Fluid backpack", r9, r);
    return r;
}

static int g_drillAvail = -1;

static void LogDrillAvailable(void)
{
    static unsigned tick;
    if (++tick % 60u) return;
    HfFn_StratagemAvailable avail = hf_fn_StratagemAvailable(g_mod);
    void* prog = hf_global_Progression(g_mod);
    if (!avail || !prog) return;
    int a = avail(prog, 64u) ? 1 : 0;
    if (a != g_drillAvail) { g_drillAvail = a; hf->log(g_mod, "picker: TCS03 drill %s", a ? "available" : "not available"); }
}

static void HookDiagnostics(void)
{
    if (hf->code_matches(g_game + RVA_GEN_OBJECTIVES, GEN_OBJECTIVES_BYTES) &&
        hf->hook(g_mod, g_game + RVA_GEN_OBJECTIVES, (void*)ObjectivesDetour, (void**)&g_objOriginal) == 0)
        hf->log(g_mod, "hooked GenerateObjectiveLocations (diagnostics)");
    else { g_objOriginal = NULL; hf->log(g_mod, "GenerateObjectiveLocations: not hooked"); }
    if (hf->code_matches(g_game + RVA_SPREAD, SPREAD_BYTES) &&
        hf->hook(g_mod, g_game + RVA_SPREAD, (void*)SpreadDetour, (void**)&g_spreadOriginal) == 0)
        hf->log(g_mod, "hooked SpreadLocations (diagnostics)");
    else { g_spreadOriginal = NULL; hf->log(g_mod, "SpreadLocations: not hooked"); }
    GuardEntityString();
    if (hf_hook_WorldSpawnUnit(g_mod, SpawnUnitDetour, &g_spawnOriginal, 0) == 0) hf->log(g_mod, "hooked WorldSpawnUnit (unit names for the guard)");
    if (hf_hook_SpawnEntity(g_mod, SpawnEntityDetour, &g_spawnEntOriginal, 0) == 0) hf->log(g_mod, "hooked SpawnEntity (Dark Fluid entities)");
    else g_spawnEntOriginal = NULL;
    if (hf_hook_StratagemPackages(g_mod, StratagemPackagesDetour, &g_stratPkgOriginal, 0) == 0) hf->log(g_mod, "hooked StratagemPackages (drill)");
    else g_stratPkgOriginal = NULL;
}

static void HookGenerator(void)
{
    if (g_hookTried || !g_game) return;
    g_hookTried = 1;
    if (!hf->code_matches(g_game + RVA_GEN_STAMP_LIST, GEN_STAMP_LIST_BYTES))
    {
        hf->log(g_mod, "GenerateMissionStampList is not the reviewed code: no hook, the stamps get no objective types");
        return;
    }
    if (hf->hook(g_mod, g_game + RVA_GEN_STAMP_LIST, (void*)GenStampListDetour, (void**)&g_genOriginal) != 0)
    {
        hf->log(g_mod, "GenerateMissionStampList: hook refused");
        g_genOriginal = NULL;
        return;
    }
    hf->log(g_mod, "hooked GenerateMissionStampList (game.dll+%#x)", RVA_GEN_STAMP_LIST);
    HookDiagnostics();
}

/* ---- 0.6.1: the stamps' own shape records ----
 * A location item's +0x00 points at its stamp's shape record (the stamp key 8 bytes before it): a header of
 * {pointer, count} arrays, the first the collision circles (the ground they flatten), the others rectangles, entry
 * points, splines, extensions and proxies (proxies: invisible collision units). Items copied from the ICBM stamp kept
 * the ICBM's record: its two small circles (the Dark Fluid sites stood on unflattened ground) and its proxies (the
 * invisible walls). Each port gets its own record: the 2024 circles (stamps-20240601.json), every other array empty.
 * The circle layout is read from the ICBM's record, whose two circles are known (generated_stamp_settings). */
#define SHAPE_HEADER 0x80u

static int FindFloat(const uint8_t* b, size_t n, float v, size_t from)
{
    for (size_t o = from; o + 4 <= n; o += 4)
    {
        float f;
        memcpy(&f, b + o, 4);
        if (f > v - 0.01f && f < v + 0.01f) return (int)o;
    }
    return -1;
}

/* A shape record for one port, built from the template's (ICBM) layout; NULL when the layout is not understood. */
static uint64_t MakeShape(const uint8_t* tplItem, uint64_t key, size_t p)
{
    uint64_t shape = 0, circles = 0, n = 0;
    memcpy(&shape, tplItem, 8);
    uint8_t head[SHAPE_HEADER], c[0x80];
    if (!shape || !Rd((uintptr_t)shape, head, sizeof head)) return 0;
    memcpy(&circles, head, 8); memcpy(&n, head + 8, 8);
    if (!circles || n != 2 || !Rd((uintptr_t)circles, c, sizeof c)) { hf->log(g_mod, "shape: the template has %llu circles, not the ICBM's 2", (unsigned long long)n); return 0; }
    int x0 = FindFloat(c, sizeof c, 3.69027901f, 0), y0 = FindFloat(c, sizeof c, -5.00320387f, 0), r0 = FindFloat(c, sizeof c, 39.1407433f, 0);
    int x1 = FindFloat(c, sizeof c, -15.1819553f, 4);
    if (x0 != 0 || y0 < 0 || r0 < 0 || x1 <= 0) { hf->log(g_mod, "shape: circle layout not recognised (x %d y %d r %d next %d)", x0, y0, r0, x1); return 0; }
    size_t stride = (size_t)x1;
    const Circle* src = kPorted[p].circles;
    size_t count = kPorted[p].count;
    uint8_t* block = VirtualAlloc(NULL, 8 + SHAPE_HEADER + count * stride, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!block) return 0;
    memcpy(block, &key, 8);
    uint8_t* h = block + 8;
    memcpy(h, head, SHAPE_HEADER);
    uint8_t* arr = h + SHAPE_HEADER;
    for (size_t i = 0; i < count; i++)
    {
        memcpy(arr + i * stride, c, stride); /* the template circle: its flattening fields */
        memcpy(arr + i * stride + x0, &src[i].x, 4);
        memcpy(arr + i * stride + y0, &src[i].y, 4);
        memcpy(arr + i * stride + r0, &src[i].r, 4);
    }
    uint64_t a = (uint64_t)(uintptr_t)arr, cnt = count;
    memcpy(h, &a, 8); memcpy(h + 8, &cnt, 8);
    /* every other {pointer, count} pair of the header emptied: no rectangles, entry points, splines, extensions, proxies */
    char cleared[160] = {0};
    size_t cl = 0;
    /* +0x10..+0x5F: rectangles, entry points, splines, extensions, proxies (the ICBM's 7 at +0x50). Not +0x70: its
     * one entry is the rotation types (2024: random for these stamps too); 0.6.2 emptied it and no stamp could be
     * placed (the map regenerated forever). */
    for (size_t o = 0x10; o + 16 <= 0x60; o += 8)
    {
        uint64_t ptr, num;
        memcpy(&ptr, h + o, 8); memcpy(&num, h + o + 8, 8);
        if (ptr > 0x10000 && ptr < 0x7FFFFFFFFFFFull && num > 0 && num < 0x10000)
        {
            uint64_t z = 0;
            memcpy(h + o, &z, 8); memcpy(h + o + 8, &z, 8);
            cl += (size_t)snprintf(cleared + cl, sizeof cleared - cl, " +%#zx(%llu)", o, (unsigned long long)num);
            o += 8;
        }
    }
    char hx[SHAPE_HEADER * 2 + 1] = {0};
    for (size_t k = 0; k < SHAPE_HEADER; k++) sprintf(hx + 2 * k, "%02x", head[k]);
    hf->log(g_mod, "shape: %s: %zu circles (stride %zu, x %d y %d r %d), cleared%s; template header %s", kPorted[p].name, count, stride, x0, y0,
            r0, cl ? cleared : " nothing", hx);
    return (uint64_t)(uintptr_t)h;
}

/* Location items for the ported stamps: copies of the template stamp's item (the TCS+ station's, 0.5.1) with the
 * ported stamp's key and radius, appended to the template's group. The game's own items stay as they are. */
static void PlaceItems(uintptr_t pass, uint64_t templateKey)
{
    ReadGroups(HolderOf(pass));
    uint8_t tpl[LOCATION_ITEM];
    Group* group = FindItem(templateKey, tpl);
    if (!group) { hf->log(g_mod, "items: the template stamp %016llx has no location item: nothing placed", (unsigned long long)templateKey); g_itemsPlaced = -1; return; }
    float tr[4];
    uint32_t level = 0, flags = 0;
    memcpy(tr, tpl + 0xD8, sizeof tr); memcpy(&level, tpl + 0x60, 4); memcpy(&flags, tpl + 0xE8, 4);
    hf->log(g_mod, "items: template stamp %016llx in set %08x group %08x (%u items): radius %.1f centre %.1f,%.1f radius %.1f, level word %08x, flags %08x",
            (unsigned long long)templateKey, group->set, group->id, group->n, (double)tr[0], (double)tr[1], (double)tr[2], (double)tr[3], level, flags);
    uint32_t done = 0;
    for (size_t p = 0; p < PORTED_COUNT; p++)
    {
        uint64_t key = kPorted[p].key;
        /* 0.7.1: no item for the record-only stamps. They have no level and no package in the 2024 build either (its
         * hash_lookup has none), so the 2024 game never placed them; placed, they showed the missing-unit "?" on flat
         * ground and spawned no objective. */
        if (strstr(kPorted[p].name, "records only")) { done++; continue; }
        if (FindItem(key, NULL)) { done++; continue; }
        uint8_t item[LOCATION_ITEM];
        /* 0.5.5 test: the template's own radius (+0xD8..+0xE4 kept) instead of the stamps' 2024 outer radii (143-187 m),
         * to see whether SpreadLocations fails on the radius. */
        float radius = 0.0f;
        memcpy(&radius, tpl + 0xD8, 4);
        memcpy(item, tpl, sizeof item);
        memcpy(item + 8, &key, 8);
        /* radius and centre kept from the template */
        uint64_t own = MakeShape(tpl, key, p);
        if (own) memcpy(item, &own, 8);
        /* 0.5.9: every difficulty. The level word (+0x60, bit n admits level n) of the ICBM template is 0x3FE, levels
         * 1-9 only, so at 10 and 11 the stamps were never offered and the map regenerated forever. */
        uint32_t levels = 0xFFFEu;
        memcpy(item + 0x60, &levels, 4);
        /* 0.8.2: the stamp's own 2024 registration (generated_location_groups.dl_bin of 2024-06-01, decrypted by 0.8.0),
         * which put all five in the ICBM's group. Its fields, carried to this build's layout by the 341 stamps both
         * files have (memory = this build's file + 8): +0x110 4 (ICBM 6), +0x138 18, +0x148 200 m (TCS 24 m),
         * +0x15C 0x060000FF for the two drop sites (0x050000FF backpack site, TCS), +0x160 the 2024 0x74 / 0x7C in
         * this build's bits, 0x434 / 0x43C (ICBM 0x430), and the second list (+0x100) empty except at the backpack
         * site. */
        {
            int pack = key == 0x2DDAA26E56F2CD4Cull, tcs = strstr(kPorted[p].name, "TCS") != NULL;
            uint32_t category = 4, c138 = 18, w15c = (pack || tcs) ? 0x050000FFu : 0x060000FFu, w160 = pack ? 0x43Cu : 0x434u;
            float spacing = tcs ? 24.0f : 200.0f;
            memcpy(item + 0x110, &category, 4);
            memcpy(item + 0x138, &c138, 4);
            memcpy(item + 0x148, &spacing, 4);
            memcpy(item + 0x15C, &w15c, 4);
            memcpy(item + 0x160, &w160, 4);
            if (!pack) memset(item + 0x100, 0, 16);
        }
        uint32_t n = group->n;
        uint8_t* fresh = VirtualAlloc(NULL, (size_t)(n + 1) * LOCATION_ITEM, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        if (!fresh || !Rd((uintptr_t)group->list, fresh, (size_t)n * LOCATION_ITEM)) { if (fresh) VirtualFree(fresh, 0, MEM_RELEASE); continue; }
        memcpy(fresh + (size_t)n * LOCATION_ITEM, item, LOCATION_ITEM);
        uint64_t list = (uint64_t)(uintptr_t)fresh;
        uint32_t count = n + 1;
        /* the list first, then the larger count: a reader sees the old count, or the new list with it */
        if (hf->write(group->at + 8, &list, 8) != 0 || hf->write(group->at + 0x10, &count, 4) != 0)
        {
            hf->log(g_mod, "items: %s: refused", kPorted[p].name);
            continue;
        }
        group->list = list;
        group->n = count;
        done++;
        hf->log(g_mod, "items: %s placed in group %08x (radius %.1f)", kPorted[p].name, group->id, (double)radius);
    }
    g_itemsPlaced = done == PORTED_COUNT ? 1 : -1;
}

/* ---- 0.6.2: the drill as a mission stratagem ----
 * Every mission's group (Reinforce, SOS, Resupply...) starts from a table of 8 sets of 6 stratagem types, one per
 * mission kind (game.dll+0x213F550, read at 0x6AE35D; HellForge's difficulty.c). Deploy Dark Fluid is kind 3 (124, 145,
 * 33, 128): the TCS03 Thumper (64) goes in its first empty place, so every player has the drill from the start. With
 * HellForge's level 11 installed, level-11 missions read a copy of the table (near game.dll, set N's first empty place
 * holding the level's stratagem); that copy gets it too. */
#define RVA_MISSION_GRANTS 0x213F550u
#define STRATAGEM_TCS03_THUMPER 64u
/* The set a mission reads is chosen from its row's kind (+0x34) at 0x6AE2E7: kind 2 -> set 2, 4 -> 3, 5 -> 4, 6 -> 5,
 * 7 -> 6, 8 -> 7, any other (1 and 3, so Deploy Dark Fluid too) -> set 1. 0.6.2 wrote set 3 (kind-4 missions). Set 1 is
 * every standard mission's: they all get the drill. */
#define MISSION_KIND_DF 1u
static int g_drillGranted;

static int AddToSet(uintptr_t table, const char* what)
{
    uint32_t set[6];
    uintptr_t at = table + MISSION_KIND_DF * sizeof set;
    if (!Rd(at, set, sizeof set)) return 0;
    for (int k = 0; k < 6; k++)
    {
        if (set[k] == STRATAGEM_TCS03_THUMPER) return 1;
        if (set[k] == 0)
        {
            uint32_t v = STRATAGEM_TCS03_THUMPER;
            int ok = hf->write(at + 4u * (uintptr_t)k, &v, 4) == 0;
            hf->log(g_mod, "drill: %s: set %u (kinds 1 and 3) grants the TCS03 Thumper (place %d)%s", what, MISSION_KIND_DF, k, ok ? "" : ": write refused");
            return ok;
        }
    }
    hf->log(g_mod, "drill: %s: the kind-3 set is full", what);
    return 0;
}

static void GrantDrill(void)
{
    if (g_drillGranted || !g_game) return;
    g_drillGranted = 1;
    uint32_t base[48];
    if (!Rd(g_game + RVA_MISSION_GRANTS, base, sizeof base) || base[6] != 124u || base[18] != 124u)
    {
        hf->log(g_mod, "drill: the mission stratagem table is not the reviewed one: not granted");
        return;
    }
    AddToSet(g_game + RVA_MISSION_GRANTS, "the game's table");
    /* HellForge's level-11 copy: committed memory within 2 GB of game.dll holding the same table with one more type */
    uintptr_t lo = g_game > 0x80000000ull ? g_game - 0x80000000ull : 0x10000, hi = g_game + 0x80000000ull, at = lo;
    MEMORY_BASIC_INFORMATION mi;
    while (at < hi && VirtualQuery((LPCVOID)at, &mi, sizeof mi) == sizeof mi)
    {
        uintptr_t b = (uintptr_t)mi.BaseAddress, e = b + mi.RegionSize;
        if (mi.State == MEM_COMMIT && mi.Type == MEM_PRIVATE && !(mi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) && mi.RegionSize <= 0x100000)
        {
            static uint8_t region[0x100000];
            SIZE_T got = 0;
            if (ReadProcessMemory(GetCurrentProcess(), (LPCVOID)b, region, (SIZE_T)(e - b), &got) && got >= sizeof base)
                for (size_t o = 0; o + sizeof base <= got; o += 8)
                {
                    const uint32_t* t = (const uint32_t*)(region + o);
                    if (t[0] || t[6] != 124u || t[7] != 145u || t[8] != 33u || t[18] != 124u || t[24] != 93u) continue;
                    if (memcmp(t, base, sizeof base) == 0) continue;
                    char what[64];
                    snprintf(what, sizeof what, "HellForge's level-11 copy at %p", (void*)(b + o));
                    AddToSet(b + o, what);
                }
        }
        at = e;
    }
}

/* ---- 0.7.1: the mission's objective instances (hf_global_ObjectiveManager: +0x24 count, +0x50 instance pointers,
 * an instance's +8 its objective id; the HUD updater 0x18B4E76..0x18B4EB1 reads them so). Logged when the count
 * changes: which objectives actually exist (Deploy Dark Fluid 258ae161, Payload Site 89ee71a8). */
static uint32_t g_objSeen = 0xFFFFFFFFu;

static void LogObjectiveInstances(void)
{
    HfObjectiveManager* m = hf_global_ObjectiveManager(g_mod);
    uint32_t n = 0;
    uint64_t arr = 0;
    if (!m || !Rd((uintptr_t)m + 0x24, &n, 4) || n == g_objSeen) return;
    g_objSeen = n;
    if (n > 64 || !Rd((uintptr_t)m + 0x50, &arr, 8) || !arr) { hf->log(g_mod, "objectives: %u instance(s)", n); return; }
    hf->log(g_mod, "objectives: %u instance(s)", n);
    for (uint32_t i = 0; i < n; i++)
    {
        uint64_t inst = 0;
        uint8_t b[0x30] = {0};
        if (!Rd((uintptr_t)arr + 8u * i, &inst, 8) || !inst || !Rd((uintptr_t)inst, b, sizeof b)) continue;
        uint32_t id;
        memcpy(&id, b + 8, 4);
        const char* nm = id == OBJ_DEPLOY ? " DEPLOY DARK FLUID" : id == OBJ_PAYLOAD ? " PAYLOAD SITE" : "";
        char hx[0x30 * 2 + 1] = {0};
        for (int k = 0; k < 0x30; k++) sprintf(hx + 2 * k, "%02x", b[k]);
        hf->log(g_mod, "    objective %u: id %08x%s  %s", i, id, nm, hx);
    }
}

/* ---- 0.7.3: the players' mission loadouts ----
 * hf_global_PlayerHistory: records of 0x1690, the loadout at +0x38 and its loaded (mission) copy at +0xA28, 0x9F0
 * each: stratagem slots of 0x30 at +0x188 (type at +0), their count at +0x788. The mission's set (game.dll+0x213F550,
 * the code at 0x6AE35D) is put in front of these slots at mission start. Logged when they change: whether the drill (64)
 * is in them. Also where 0x6AE35D's jump (HellForge's level-11 detour) goes. */
static uint32_t g_loadoutSig = 0xFFFFFFFFu;

static void LogGrantDetour(void)
{
    static int done;
    if (done || !g_game) return;
    done = 1;
    uint8_t j[5];
    if (!Rd(g_game + 0x6AE35D, j, 5)) return;
    if (j[0] != 0xE9) { hf->log(g_mod, "grant site 0x6AE35D: %02x %02x %02x %02x %02x (no jump)", j[0], j[1], j[2], j[3], j[4]); return; }
    int32_t rel;
    memcpy(&rel, j + 1, 4);
    uintptr_t t = g_game + 0x6AE35D + 5 + (intptr_t)rel;
    uint8_t c[0x60] = {0};
    Rd(t, c, sizeof c);
    char hx[sizeof c * 2 + 1] = {0};
    for (size_t k = 0; k < sizeof c; k++) sprintf(hx + 2 * k, "%02x", c[k]);
    hf->log(g_mod, "grant site 0x6AE35D jumps to %p: %s", (void*)t, hx);
}

static void LogLoadouts(void)
{
    static unsigned tick;
    if (++tick % 30u) return;
    uint8_t* ph = (uint8_t*)hf_global_PlayerHistory(g_mod);
    uint32_t n = 0;
    if (!ph || !Rd((uintptr_t)ph + 0x2D200, &n, 4) || n > 32) return;
    uint32_t sig = 2166136261u ^ n;
    char line[8][200];
    int lines = 0;
    for (uint32_t i = 0; i < n && i < 4; i++)
        for (int copy = 0; copy < 2; copy++)
        {
            uintptr_t lo = (uintptr_t)ph + 0x1690u * i + (copy ? 0xA28u : 0x38u);
            uint32_t cnt = 0;
            if (!Rd(lo + 0x788, &cnt, 4) || cnt > 32) continue;
            int at = snprintf(line[lines], sizeof line[0], "player %u %s: %u slot(s):", i, copy ? "mission copy" : "loadout", cnt);
            for (uint32_t k = 0; k < cnt; k++)
            {
                uint32_t type = 0;
                Rd(lo + 0x188 + 0x30u * k, &type, 4);
                sig = (sig ^ type) * 16777619u;
                if (at < (int)sizeof line[0] - 8) at += snprintf(line[lines] + at, sizeof line[0] - at, " %u%s", type, type == 64u ? "(DRILL)" : "");
            }
            sig = (sig ^ cnt) * 16777619u;
            lines++;
        }
    if (sig == g_loadoutSig) return;
    g_loadoutSig = sig;
    for (int l = 0; l < lines; l++) hf->log(g_mod, "%s", line[l]);
    /* one mission slot whole, as the template a direct insert would copy */
    uintptr_t m = (uintptr_t)ph + 0xA28u;
    uint8_t slot[0x30];
    uint32_t cnt = 0;
    if (Rd(m + 0x788, &cnt, 4) && cnt && cnt <= 32 && Rd(m + 0x188, slot, sizeof slot))
    {
        char hx[0x30 * 2 + 1] = {0};
        for (int k = 0; k < 0x30; k++) sprintf(hx + 2 * k, "%02x", slot[k]);
        hf->log(g_mod, "    player 0 mission slot 0: %s", hx);
    }
}

/* ---- 0.8.0: decrypt the 2024 settings files (data/game, the .dl_bin files) ----
 * The dl_bin files are encrypted on disk; the engine's file reader decrypts them (EngineApi +0x170: +8 read(name,
 * &size) -> buffer, +0x20 free(buffer); game.dll's loaders call it so, e.g. 0x11F20AA). The engine is packed, so the
 * key is not reachable; instead the reader is asked for a current file of the same name while CreateFileW/A (in
 * KernelBase) open the 2024 file in its place. Each result is written to OLD_DIR\dec\<name> with a check that it
 * starts like a decrypted dl_bin ({u32 count, "LDLD"}). One current file is decrypted plainly as the control. */
#define OLD_DIR "F:/Game/A/HD2/A/0/hd2_old/20240601/data/game/"
#define DEC_DIR "F:/Game/A/HD2/A/0/hd2_old/20240601/dec/"
typedef void* (*ReadFn)(const char* name, uint32_t* size);
typedef void (*FreeFn)(void* buffer);
typedef HANDLE (WINAPI *CreateFileWFn)(LPCWSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
typedef HANDLE (WINAPI *CreateFileAFn)(LPCSTR, DWORD, DWORD, LPSECURITY_ATTRIBUTES, DWORD, DWORD, HANDLE);
static CreateFileWFn g_cfwOriginal;
static CreateFileAFn g_cfaOriginal;
static const char* volatile g_redirName;   /* the basename being swapped, while our read runs */
static volatile DWORD g_redirThread;
static volatile LONG g_redirHits;
static char g_seenPath[300];

static int EndsWithA(const char* path, const char* name)
{
    size_t a = strlen(path), b = strlen(name);
    return a >= b && _stricmp(path + a - b, name) == 0 && (a == b || path[a - b - 1] == 92 || path[a - b - 1] == 47);
}

static HANDLE WINAPI CreateFileWDetour(LPCWSTR path, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES sa, DWORD disp, DWORD flags, HANDLE tpl)
{
    if (g_redirName && GetCurrentThreadId() == g_redirThread && path)
    {
        char a[300];
        WideCharToMultiByte(CP_ACP, 0, path, -1, a, sizeof a, NULL, NULL);
        a[sizeof a - 1] = 0;
        if (!g_seenPath[0]) memcpy(g_seenPath, a, sizeof a < sizeof g_seenPath ? sizeof a : sizeof g_seenPath);
        if (EndsWithA(a, g_redirName))
        {
            wchar_t w[400];
            char full[400];
            snprintf(full, sizeof full, "%s%s", OLD_DIR, g_redirName);
            MultiByteToWideChar(CP_ACP, 0, full, -1, w, 400);
            InterlockedIncrement(&g_redirHits);
            return g_cfwOriginal(w, access, share, sa, disp, flags, tpl);
        }
    }
    return g_cfwOriginal(path, access, share, sa, disp, flags, tpl);
}

static HANDLE WINAPI CreateFileADetour(LPCSTR path, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES sa, DWORD disp, DWORD flags, HANDLE tpl)
{
    if (g_redirName && GetCurrentThreadId() == g_redirThread && path)
    {
        if (!g_seenPath[0]) snprintf(g_seenPath, sizeof g_seenPath, "%s", path);
        if (EndsWithA(path, g_redirName))
        {
            char full[400];
            snprintf(full, sizeof full, "%s%s", OLD_DIR, g_redirName);
            InterlockedIncrement(&g_redirHits);
            return g_cfaOriginal(full, access, share, sa, disp, flags, tpl);
        }
    }
    return g_cfaOriginal(path, access, share, sa, disp, flags, tpl);
}

static int WriteWhole(const char* path, const void* data, uint32_t size)
{
    FILE* f = fopen(path, "wb");
    if (!f) return 0;
    size_t w = fwrite(data, 1, size, f);
    fclose(f);
    return w == size;
}

static void DecryptOldSettings(void)
{
    static int done;
    if (done || !g_game) return;
    done = 1;
    uint64_t api = 0, dom = 0, rd = 0, fr = 0;
    if (!Rd(g_game + 0x3326308, &api, 8) || !api || !Rd((uintptr_t)api + 0x170, &dom, 8) || !dom ||
        !Rd((uintptr_t)dom + 8, &rd, 8) || !Rd((uintptr_t)dom + 0x20, &fr, 8) || !rd || !fr)
    {
        hf->log(g_mod, "decrypt: the engine's file reader was not found");
        return;
    }
    HMODULE exe = GetModuleHandleA(NULL);
    hf->log(g_mod, "decrypt: reader %p (exe+%#llx), free %p", (void*)rd, (unsigned long long)(rd - (uint64_t)(uintptr_t)exe), (void*)fr);
    HMODULE kb = GetModuleHandleA("KernelBase.dll");
    FARPROC cfw = kb ? GetProcAddress(kb, "CreateFileW") : NULL, cfa = kb ? GetProcAddress(kb, "CreateFileA") : NULL;
    int hw = cfw && hf->hook(g_mod, (uintptr_t)cfw, (void*)CreateFileWDetour, (void**)&g_cfwOriginal) == 0;
    int ha = cfa && hf->hook(g_mod, (uintptr_t)cfa, (void*)CreateFileADetour, (void**)&g_cfaOriginal) == 0;
    hf->log(g_mod, "decrypt: CreateFileW %s, CreateFileA %s", hw ? "hooked" : "NOT hooked", ha ? "hooked" : "NOT hooked");
    if (!hw) return;
    CreateDirectoryA(DEC_DIR, NULL);
    ReadFn read = (ReadFn)(uintptr_t)rd;
    FreeFn release = (FreeFn)(uintptr_t)fr;
    /* control: a current file, plainly */
    {
        uint32_t size = 0;
        g_seenPath[0] = 0;
        g_redirThread = GetCurrentThreadId();
        g_redirName = "no-such-name.none";
        uint8_t* b = (uint8_t*)read("generated_region_settings.dl_bin", &size);
        g_redirName = NULL;
        hf->log(g_mod, "decrypt: control generated_region_settings.dl_bin -> %u bytes, %02x%02x%02x%02x %02x%02x%02x%02x; opened %s", size,
                b && size >= 8 ? b[0] : 0, b && size >= 8 ? b[1] : 0, b && size >= 8 ? b[2] : 0, b && size >= 8 ? b[3] : 0,
                b && size >= 8 ? b[4] : 0, b && size >= 8 ? b[5] : 0, b && size >= 8 ? b[6] : 0, b && size >= 8 ? b[7] : 0,
                g_seenPath[0] ? g_seenPath : "(nothing seen)");
        if (b && size) WriteWhole(DEC_DIR "current-generated_region_settings.dl_bin", b, size);
        if (b) release(b);
    }
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(OLD_DIR "*.dl_bin", &fd);
    int ok = 0, bad = 0;
    if (h != INVALID_HANDLE_VALUE)
    {
        do
        {
            char cur[400];
            snprintf(cur, sizeof cur, "%s", fd.cFileName);
            uint32_t size = 0;
            LONG before = g_redirHits;
            g_seenPath[0] = 0;
            g_redirThread = GetCurrentThreadId();
            g_redirName = fd.cFileName;
            uint8_t* b = (uint8_t*)read(cur, &size);
            g_redirName = NULL;
            int swapped = g_redirHits != before;
            int good = b && size >= 8 && memcmp(b + 4, "LDLD", 4) == 0;
            if (b && size && swapped)
            {
                char out[400];
                snprintf(out, sizeof out, "%s%s", DEC_DIR, fd.cFileName);
                WriteWhole(out, b, size);
            }
            hf->log(g_mod, "decrypt: %s: %u bytes, %s, %s", fd.cFileName, size, swapped ? "2024 file read" : "NOT swapped", good ? "decrypted" : "NOT a dl_bin");
            if (swapped && good) ok++; else bad++;
            if (b) release(b);
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    hf->log(g_mod, "decrypt: %d of %d 2024 files decrypted into " DEC_DIR, ok, ok + bad);
    /* 0.8.1: the current build's files too, for a file-to-file comparison (DEC_DIR/current/) */
    CreateDirectoryA(DEC_DIR "current", NULL);
    h = FindFirstFileA("F:/Game/A/HD2/HellForgev5/game/data/game/*.dl_bin", &fd);
    ok = 0;
    if (h != INVALID_HANDLE_VALUE)
    {
        do
        {
            uint32_t size = 0;
            g_redirName = NULL;
            uint8_t* b = (uint8_t*)read(fd.cFileName, &size);
            if (b && size)
            {
                char out[400];
                snprintf(out, sizeof out, "%scurrent/%s", DEC_DIR, fd.cFileName);
                if (WriteWhole(out, b, size)) ok++;
            }
            if (b) release(b);
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    hf->log(g_mod, "decrypt: %d current files written into " DEC_DIR "current/", ok);
}

static void Tick(HfMod* mod, float dt, void* user)
{
    (void)mod; (void)user;
    g_wait += dt;
    if (g_wait < 2.0f) return;
    g_wait = 0.0f;
    if (!g_game && !FindGame()) return;
    HookGenerator();
    ReportNullEntities();
    LogObjectiveInstances();
    LogDrillAvailable();
    LogGrantDetour();
    LogLoadouts();
    (void)DecryptOldSettings; /* 0.8.2: off; the 2024 and current files are already in hd2_old/20240601/dec */
    GrantDrill(); /* 0.7.3: on again; LogLoadouts shows whether the drill reaches the players' mission loadouts */
    /* The stamps' packages, held for the session. hf->load_package only takes catalogued packages or a data spec's
     * own, and these come from the zz.darkfluid-resources patch, so the game's own RequestPackages is called (on this,
     * the game thread, as HellForge's load_package does) with the RefcountedPackageSystem; PackageHasLoaded says when
     * the engine has all of each. One reference, taken once, held for the session. */
    int resident = 0;
    HfFn_PackageHasLoaded hasLoaded = hf_fn_PackageHasLoaded(g_mod);
    if (!g_pkgRequested)
    {
        HfFn_RequestPackages request = hf_fn_RequestPackages(g_mod);
        HfPackageSystem* system = hf_global_PackageSystem(g_mod);
        if (request && system && system->map)
        {
            request(system, kDepBundles, (uint32_t)DEP_COUNT);
            g_pkgRequested = 1;
            hf->log(g_mod, "packages: requested the %u bundles the stamps' levels draw on", (unsigned)DEP_COUNT);
        }
        else if (!g_pkgWaitLogged)
        {
            g_pkgWaitLogged = 1;
            hf->log(g_mod, "packages: RequestPackages %s, package system %s: waiting", request ? "found" : "missing", system ? "found" : "missing");
        }
    }
    for (size_t p = 0; p < DEP_COUNT && g_pkgRequested; p++)
    {
        int st = hasLoaded && hasLoaded(kDepBundles[p]) ? HF_PACKAGE_RESIDENT : HF_PACKAGE_LOADING;
        g_pkgState[p] = st;
        if (st == HF_PACKAGE_RESIDENT) resident++;
    }
    if (g_pkgRequested && resident != g_pkgResidentLogged)
    {
        g_pkgResidentLogged = resident;
        hf->log(g_mod, "packages: %d of %u bundles resident", resident, (unsigned)DEP_COUNT);
    }
    uint64_t world = 0, level = 0, holder = 0;
    if (!Rd(g_game + RVA_WORLD, &world, 8) || !Rd((uintptr_t)world + 0xAE288, &level, 8) || !Rd((uintptr_t)level + 0x2D0, &holder, 8) ||
        holder == 0) return;
    if (!g_offered && g_itemsPlaced == 1 && g_genOriginal && resident == (int)DEP_COUNT)
    {
        g_offered = 1;
        OfferMission();
    }
    if (holder == g_holder || g_dumps >= 1) return;
    g_holder = holder;
    g_dumps++;
    LogLocations(holder);
}

HF_MOD_EXPORT int hellforge_mod_init(const HfApi* api, HfMod* mod)
{
    hf = api; g_mod = mod;
    if (hf->version < 16) return 1;
    if (strcmp(hf->build_id(), BUILD) != 0) { hf->log(mod, "game build %s is not " BUILD ": nothing is done", hf->build_id()); return 0; }
    hf->log(mod, "0.8.2: places the Dark Fluid stamps' location items, loads their packages and hooks GenerateMissionStampList to "
                 "give them their objective types; then makes Deploy Dark Fluid template 6's special mission");
    hf->on(mod, HF_EVENT_ENTITIES_READY, Ready, NULL);
    if (!hf->on_game_thread(mod, Tick, NULL)) hf->log(mod, "no game-thread callback: nothing is done");
    /* 0.3/0.4's memory search and table probes, kept for diagnostics */
    (void)SearchPackageMap; (void)Port; (void)PmFindKey;
    return 0;
}
