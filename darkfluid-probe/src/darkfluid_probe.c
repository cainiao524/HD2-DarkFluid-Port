/* Dark Fluid Probe (local.darkfluid-probe): a read-only HellForge code mod.
 *
 * It never writes game memory. It copies four of the game's tables out of game.dll's unpacked image into the session's
 * logs folder (darkfluid-probe-*.bin), so the Dark Fluid / TCS mission group can be analysed offline:
 *
 *   missions.bin    162 x 0x380   mission rows        (game.dll + 0x3773420)
 *   objectives.bin  153 x 0xA0    objective defs      (game.dll + 0x32EF870)
 *   operations.bin   25 x 0x490   operation rows      (game.dll + 0x32FEF10)
 *   locations-N.bin               the level's location-group sets ([[world + 0xAE288] + 0x2D0]): every set, group and
 *                                 0x1C8-byte item (item + 8 is the stamp key), written when a level generator exists
 *                                 and again when its holder changes (at most 4 times).
 *
 * The addresses are HellForge's own (native/mod-loader/difficulty.c) for game build 25480438; on any other build the
 * probe does nothing. */
#include "hellforge_mod.h"

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
#define ITEM 0x1C8u

static const HfApi* hf;
static HfMod* g_mod;
static uintptr_t g_game;
static uint64_t g_lastHolder;
static uint32_t g_locationDumps;
static float g_wait;

static HANDLE OpenOut(const char* name)
{
    char path[1024];
    wchar_t wide[1024];
    const char* folder = hf->logs_folder(g_mod);
    if (folder == NULL || folder[0] == 0) return INVALID_HANDLE_VALUE;
    _snprintf_s(path, sizeof path, _TRUNCATE, "%s\\darkfluid-probe-%s", folder, name);
    if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wide, 1024)) return INVALID_HANDLE_VALUE;
    return CreateFileW(wide, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
}

static void Put(HANDLE file, const void* data, uint32_t size)
{
    DWORD done = 0;
    WriteFile(file, data, size, &done, NULL);
}

/* Copies size bytes at address to the file in 4 KiB steps; unreadable steps are written as zeroes. Returns the bytes
 * that could not be read. */
static uint32_t Copy(HANDLE file, uintptr_t address, uint32_t size)
{
    static uint8_t buffer[0x1000];
    uint32_t missed = 0;
    for (uint32_t at = 0; at < size; at += sizeof buffer)
    {
        uint32_t step = size - at < sizeof buffer ? size - at : (uint32_t)sizeof buffer;
        if (hf->read(address + at, buffer, step) != 0) { memset(buffer, 0, step); missed += step; }
        Put(file, buffer, step);
    }
    return missed;
}

static void DumpTable(const char* name, uint32_t rva, uint32_t rows, uint32_t row)
{
    HANDLE file = OpenOut(name);
    if (file == INVALID_HANDLE_VALUE) { hf->log(g_mod, "probe: cannot create %s (error %lu)", name, GetLastError()); return; }
    uint32_t missed = Copy(file, g_game + rva, rows * row);
    CloseHandle(file);
    hf->log(g_mod, "probe: %s written, %u rows x 0x%X from game.dll+0x%X (%u bytes unreadable)", name, rows, row, rva, missed);
}

static int Rd(uintptr_t address, void* out, size_t size) { return address != 0 && hf->read(address, out, size) == 0; }

/* locations-N.bin: "DFPL", holder (0xC0 raw), then per set: "SET ", set (0x40 raw), per group: "GRP ", group (0x18 raw),
 * per item: "ITEM", item (0x1C8 raw), class count (u32), classes (count bytes, at most 64). */
static void DumpLocations(uint64_t holder)
{
    uint64_t sets = 0;
    uint32_t count = 0, groupsTotal = 0, itemsTotal = 0;
    char name[64];
    if (!Rd((uintptr_t)holder + 0x38, &sets, 8) || sets == 0 || !Rd((uintptr_t)holder + 0xBC, &count, 4) || count == 0 || count > 256) return;
    _snprintf_s(name, sizeof name, _TRUNCATE, "locations-%u.bin", g_locationDumps);
    HANDLE file = OpenOut(name);
    if (file == INVALID_HANDLE_VALUE) { hf->log(g_mod, "probe: cannot create %s (error %lu)", name, GetLastError()); return; }
    Put(file, "DFPL", 4);
    Copy(file, (uintptr_t)holder, 0xC0);
    for (uint32_t s = 0; s < count; s++)
    {
        uint64_t set = 0, groups = 0;
        uint32_t groupCount = 0;
        if (!Rd((uintptr_t)sets + 8 * (uintptr_t)s, &set, 8) || set == 0) continue;
        Put(file, "SET ", 4);
        Copy(file, (uintptr_t)set, 0x40);
        if (!Rd((uintptr_t)set + 0x28, &groups, 8) || !Rd((uintptr_t)set + 0x30, &groupCount, 4) || groups == 0 || groupCount > 256) continue;
        for (uint32_t g = 0; g < groupCount; g++)
        {
            uintptr_t group = (uintptr_t)groups + 0x18 * (uintptr_t)g;
            uint64_t items = 0;
            uint32_t itemCount = 0;
            Put(file, "GRP ", 4);
            Copy(file, group, 0x18);
            groupsTotal++;
            if (!Rd(group + 8, &items, 8) || !Rd(group + 0x10, &itemCount, 4) || items == 0 || itemCount > 4096) continue;
            for (uint32_t i = 0; i < itemCount; i++)
            {
                uintptr_t item = (uintptr_t)items + (uintptr_t)i * ITEM;
                uint64_t classes = 0;
                uint32_t classCount = 0;
                uint8_t list[64] = {0};
                Put(file, "ITEM", 4);
                Copy(file, item, ITEM);
                if (!Rd(item + 0x198, &classes, 8) || !Rd(item + 0x1A0, &classCount, 4) || classes == 0 || classCount > sizeof list) classCount = 0;
                if (classCount != 0 && !Rd((uintptr_t)classes, list, classCount)) classCount = 0;
                Put(file, &classCount, 4);
                Put(file, list, classCount);
                itemsTotal++;
            }
        }
    }
    CloseHandle(file);
    hf->log(g_mod, "probe: %s written (holder %#llx, %u sets, %u groups, %u items)", name, (unsigned long long)holder, count, groupsTotal, itemsTotal);
    g_locationDumps++;
}

static void Tick(HfMod* mod, float dt, void* user)
{
    (void)mod; (void)user;
    g_wait += dt;
    if (g_wait < 2.0f || g_locationDumps >= 4) return;
    g_wait = 0.0f;
    uint64_t world = 0, level = 0, holder = 0;
    if (!Rd(g_game + RVA_WORLD, &world, 8) || world == 0 || !Rd((uintptr_t)world + 0xAE288, &level, 8) || level == 0 ||
        !Rd((uintptr_t)level + 0x2D0, &holder, 8) || holder == 0 || holder == g_lastHolder) return;
    g_lastHolder = holder;
    DumpLocations(holder);
}

static void Ready(HfMod* mod, HfEvent event, void* user)
{
    (void)mod; (void)event; (void)user;
    DumpTable("missions.bin", RVA_MISSIONS, MISSION_ROWS, MISSION_ROW);
    DumpTable("objectives.bin", RVA_OBJECTIVES, OBJECTIVE_ROWS, OBJECTIVE_ROW);
    DumpTable("operations.bin", RVA_OPERATIONS, OPERATION_ROWS, OPERATION_ROW);
}

HF_MOD_EXPORT int hellforge_mod_init(const HfApi* api, HfMod* mod)
{
    size_t size = 0;
    hf = api; g_mod = mod;
    if (hf->version < 16) return 1;
    if (strcmp(hf->build_id(), BUILD) != 0) { hf->log(mod, "probe: game build %s is not " BUILD "; nothing is read", hf->build_id()); return 0; }
    g_game = hf->module_base("game.dll", &size);
    if (g_game == 0 || size < RVA_MISSIONS + MISSION_ROWS * MISSION_ROW) { hf->log(mod, "probe: game.dll is not loaded as expected"); return 0; }
    hf->on(mod, HF_EVENT_ENTITIES_READY, Ready, NULL);
    if (!hf->on_game_thread(mod, Tick, NULL)) hf->log(mod, "probe: no game-thread callback; the location groups are not dumped");
    return 0;
}
