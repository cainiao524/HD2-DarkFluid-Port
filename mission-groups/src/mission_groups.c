/* Mission Groups (local.mission-groups) 2.x: the host chooses which vanilla operation (任務組) the game creates.
 *
 * Why whole rows. The game builds an operation itself and registers it with the backend as
 *     {operationSettingsId, seed, planetIndex, race, difficulty, missions: [{index}, ...]}
 * (HellForge's backend keeps these bodies in <session>\backend\mission-bodies\*-Operation_Create.json). The missions
 * carry no mission id: every machine of the lobby derives them from the operation row named by operationSettingsId and
 * the seed, with its own tables. So a host that edits a row's mission list would play other missions than a joiner
 * without the mod. What the host may change is only which vanilla row the game uses.
 *
 * So, on the host or in solo, the mod copies a whole vanilla operation row (game.dll + 0x32FEF10, 25 x 0x490), its id
 * included, over the row the game would have picked. The host's game then creates the operation with that vanilla row's
 * id and its own seed; a joiner without the mod looks the id up in its own, untouched table and derives the same
 * missions, objectives, variants and stamps. Mission rows and objective definitions are never written.
 *
 * On a joiner it writes nothing and puts the game's rows back. Nothing here depends on the number of players.
 *
 * Config: mission-groups.txt in the mod's folder, reread on the ship when it changes. mission-groups-catalog.txt in the
 * session's logs folder lists every operation row and mission row. Every operation the host creates is logged from the
 * backend's record (planet, operation row, seed), and every mission start logs the ids in the mission's data, on any
 * machine running the mod, to compare host and joiner.
 *
 * Addresses: game build 25480438 (HellForge's native/mod-loader/difficulty.c, MDBE 0.4.7; GameMode from the SDK
 * reference). On any other build it does nothing. */
#include "hellforge_mod.h"

#include <windows.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUILD "25480438"
#define RVA_MISSIONS 0x3773420u
#define MISSION_ROWS 162u
#define MISSION_ROW 0x380u
#define RVA_OPERATIONS 0x32FEF10u
#define OPERATION_ROWS 25u
#define OPERATION_ROW 0x490u
#define OP_LIST 0x38u
#define OP_SLOTS 32u
#define RVA_GAME_MODE 0x33266A0u
#define MISSION_SCAN 0x1000u

#define MAX_SECTIONS 64
#define CONFIG_MAX (256 * 1024)
#define MAX_BODIES 256

#define HAS(f) (hf->size >= offsetof(HfApi, f) + sizeof hf->f)

static const HfApi* hf;
static HfMod* g_mod;
static uintptr_t g_game;
static int g_ready;

/* The game's tables, as they were when the mod first read them. */
static uint32_t g_mid[MISSION_ROWS];
static uint8_t g_mFaction[MISSION_ROWS], g_mKind[MISSION_ROWS], g_mEnabled[MISSION_ROWS];
static uint32_t g_mLo[MISSION_ROWS], g_mHi[MISSION_ROWS], g_mName[MISSION_ROWS];
static uint8_t g_orig[OPERATION_ROWS][OPERATION_ROW];
static uint32_t g_opId[OPERATION_ROWS];
static uint8_t g_opFaction[OPERATION_ROWS];

/* The config: each section names target rows and the vanilla row they take. */
enum { SEC_ALL = 1, SEC_FACTION, SEC_OPERATION_ROW };
typedef struct
{
    uint8_t kind;
    uint32_t key;
    int line;
    int source; /* the vanilla row to copy, -1 none (keep) */
} Section;
static Section g_sec[MAX_SECTIONS];
static int g_nsec;
static int g_enabled = 1;
static int g_crossFaction;
static int g_configOk;
static FILETIME g_configTime;
static int g_configSeen; /* 0 never looked, 1 file found, 2 file missing */

/* What is in the game now. */
static int g_applied = -1; /* -1 not yet, 0 vanilla, 1 the config's */
static int g_dirty;
static int g_plan[OPERATION_ROWS]; /* per row: the row copied over it, or -1 */
static uint32_t g_substituted;
static float g_wait, g_bodyWait;

/* Operation records already logged. */
static char g_bodySeen[MAX_BODIES][64];
static int g_nBodySeen;

static int Rd(uintptr_t address, void* out, size_t size) { return address != 0 && hf->read(address, out, size) == 0; }

static void Status(const char* text)
{
    if (HAS(set_status)) hf->set_status(g_mod, text);
}

static const char* FactionName(uint32_t f)
{
    switch (f)
    {
    case 2: return "Terminid";
    case 3: return "Automaton";
    case 4: return "Illuminate";
    default: return "?";
    }
}

static const char* RoleName(uint32_t role)
{
    return role == HF_SESSION_JOINER ? "joiner" : role == HF_SESSION_HOST ? "host" : "solo";
}

static int MissionRow(uint32_t id)
{
    if (id == 0) return -1;
    for (uint32_t i = 0; i < MISSION_ROWS; i++)
        if (g_mid[i] == id) return (int)i;
    return -1;
}

static int OperationRow(uint32_t id)
{
    for (uint32_t r = 0; r < OPERATION_ROWS; r++)
        if (g_opId[r] == id) return (int)r;
    return -1;
}

static uintptr_t OpRow(uint32_t r) { return g_game + RVA_OPERATIONS + r * OPERATION_ROW; }

static uint32_t ListCount(const uint8_t* row)
{
    uint32_t n = 0;
    while (n < OP_SLOTS)
    {
        uint32_t id;
        memcpy(&id, row + OP_LIST + 8 * n, 4);
        if (id == 0) break;
        n++;
    }
    return n;
}

static void ListText(const uint8_t* row, char* text, size_t size)
{
    text[0] = 0;
    uint32_t n = ListCount(row);
    for (uint32_t k = 0; k < n; k++)
    {
        uint32_t id;
        float w;
        memcpy(&id, row + OP_LIST + 8 * k, 4);
        memcpy(&w, row + OP_LIST + 8 * k + 4, 4);
        size_t at = strlen(text);
        snprintf(text + at, size - at, " %d:%g", MissionRow(id), (double)w);
    }
}

/* ---- the tables ------------------------------------------------------------------------------------------------- */

static int ReadTables(void)
{
    size_t size = 0;
    g_game = hf->module_base("game.dll", &size);
    if (g_game == 0 || size < RVA_MISSIONS + MISSION_ROWS * MISSION_ROW)
    {
        hf->log(g_mod, "game.dll is not loaded as expected (base %#llx, size %#llx): nothing is done",
                (unsigned long long)g_game, (unsigned long long)size);
        return 0;
    }
    for (uint32_t r = 0; r < MISSION_ROWS; r++)
    {
        uint8_t row[0x14];
        uintptr_t at = g_game + RVA_MISSIONS + r * MISSION_ROW;
        if (!Rd(at, row, sizeof row)) continue;
        memcpy(&g_mid[r], row, 4);
        g_mEnabled[r] = row[4];
        g_mFaction[r] = row[8];
        memcpy(&g_mLo[r], row + 0xC, 4);
        memcpy(&g_mHi[r], row + 0x10, 4);
        Rd(at + 0x34, &g_mKind[r], 1);
        Rd(at + 0x340, &g_mName[r], 4);
    }
    for (uint32_t r = 0; r < OPERATION_ROWS; r++)
    {
        if (!Rd(OpRow(r), g_orig[r], OPERATION_ROW))
        {
            hf->log(g_mod, "operation row %u is unreadable: nothing is done", r);
            return 0;
        }
        memcpy(&g_opId[r], g_orig[r], 4);
        g_opFaction[r] = g_orig[r][8];
        g_plan[r] = -1;
    }
    return 1;
}

static FILE* OpenUtf8(const char* folder, const char* name, const wchar_t* mode)
{
    char path[1024];
    wchar_t wide[1024];
    if (folder == NULL || folder[0] == 0) return NULL;
    snprintf(path, sizeof path, "%s\\%s", folder, name);
    if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wide, 1024)) return NULL;
    return _wfopen(wide, mode);
}

static void WriteCatalog(void)
{
    if (!HAS(logs_folder)) return;
    FILE* f = OpenUtf8(hf->logs_folder(g_mod), "mission-groups-catalog.txt", L"wb");
    if (f == NULL)
    {
        hf->log(g_mod, "cannot write mission-groups-catalog.txt to the logs folder");
        return;
    }
    static char text[1024];
    fprintf(f, "Mission Groups catalogue, game build %s\n\n", BUILD);
    fprintf(f, "Operation rows: the vanilla operations (任務組) mission-groups.txt can pick, by row or id.\n");
    fprintf(f, "Missions: mission row:weight (the mission rows are listed below).\n\n");
    for (uint32_t r = 0; r < OPERATION_ROWS; r++)
    {
        ListText(g_orig[r], text, sizeof text);
        fprintf(f, "row %2u  id %08x (%u)  faction %u %-11s %2u missions:%s\n", r, g_opId[r], g_opId[r], g_opFaction[r],
                FactionName(g_opFaction[r]), ListCount(g_orig[r]), text);
    }
    fprintf(f, "\nMission rows\nrow  id        enabled faction        kind levels name-text\n");
    for (uint32_t r = 0; r < MISSION_ROWS; r++)
    {
        if (g_mid[r] == 0) continue;
        fprintf(f, "%3u  %08x  %u       %u %-11s  %3u  %2u..%-2u %u\n", r, g_mid[r], g_mEnabled[r], g_mFaction[r],
                FactionName(g_mFaction[r]), g_mKind[r], g_mLo[r], g_mHi[r], g_mName[r]);
    }
    fclose(f);
    hf->log(g_mod, "catalogue written: mission-groups-catalog.txt (logs folder)");
}

/* ---- the config ------------------------------------------------------------------------------------------------- */

static char* Trim(char* s)
{
    while (*s == ' ' || *s == '\t') s++;
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r')) s[--n] = 0;
    return s;
}

static int ParseFaction(const char* t, uint32_t* out)
{
    if (!_stricmp(t, "terminid") || !_stricmp(t, "terminids") || !_stricmp(t, "bugs")) { *out = 2; return 1; }
    if (!_stricmp(t, "automaton") || !_stricmp(t, "automatons") || !_stricmp(t, "bots")) { *out = 3; return 1; }
    if (!_stricmp(t, "illuminate") || !_stricmp(t, "squids")) { *out = 4; return 1; }
    char* end;
    unsigned long v = strtoul(t, &end, 0);
    if (*t && *end == 0 && v > 0 && v < 256) { *out = (uint32_t)v; return 1; }
    return 0;
}

static int ParseU32(const char* t, int base, uint32_t* out)
{
    char* end;
    if (t == NULL || *t == 0) return 0;
    unsigned long long v = strtoull(t, &end, base);
    if (*end != 0 || v > 0xFFFFFFFFull) return 0;
    *out = (uint32_t)v;
    return 1;
}

/* "<row>", "id <hex>" or "keep": the operation row it names, -1 for keep, -2 for neither. */
static int ParseOperation(char* a, char* b, char* c)
{
    uint32_t v;
    if (a && !_stricmp(a, "keep") && !b) return -1;
    if (a && !_stricmp(a, "id") && b && !c && ParseU32(b, 16, &v)) return OperationRow(v) >= 0 ? OperationRow(v) : -2;
    if (a && !b && ParseU32(a, 10, &v) && v < OPERATION_ROWS) return (int)v;
    return -2;
}

static int ParseConfig(char* text)
{
    int errors = 0, line = 0;
    Section* cur = NULL;
    g_nsec = 0;
    g_enabled = 1;
    g_crossFaction = 0;
    if ((unsigned char)text[0] == 0xEF && (unsigned char)text[1] == 0xBB && (unsigned char)text[2] == 0xBF) text += 3;
    for (char* next = text; next != NULL;)
    {
        char* s = next;
        char* nl = strchr(s, '\n');
        next = nl ? nl + 1 : NULL;
        if (nl) *nl = 0;
        line++;
        char* hash = strpbrk(s, "#;");
        if (hash) *hash = 0;
        s = Trim(s);
        if (*s == 0) continue;

        if (*s == '[')
        {
            char* close = strchr(s, ']');
            cur = NULL;
            if (close == NULL) { hf->log(g_mod, "mission-groups.txt line %d: no ']'", line); errors++; continue; }
            *close = 0;
            char* a = strtok(s + 1, " \t");
            char* b = a ? strtok(NULL, " \t") : NULL;
            char* c = b ? strtok(NULL, " \t") : NULL;
            if (g_nsec == MAX_SECTIONS) { hf->log(g_mod, "line %d: more than %d sections", line, MAX_SECTIONS); errors++; continue; }
            Section sec = {0};
            sec.line = line;
            sec.source = -2;
            uint32_t v = 0;
            if (a && !_stricmp(a, "all") && !b) sec.kind = SEC_ALL;
            else if (a && !_stricmp(a, "faction") && b && !c && ParseFaction(b, &v)) { sec.kind = SEC_FACTION; sec.key = v; }
            else if (a && !_stricmp(a, "operation") && b && !_stricmp(b, "id") && c && ParseU32(c, 16, &v) && OperationRow(v) >= 0)
            { sec.kind = SEC_OPERATION_ROW; sec.key = (uint32_t)OperationRow(v); }
            else if (a && !_stricmp(a, "operation") && b && !c && ParseU32(b, 10, &v) && v < OPERATION_ROWS) { sec.kind = SEC_OPERATION_ROW; sec.key = v; }
            else
            {
                hf->log(g_mod, "mission-groups.txt line %d: unknown section (use [all], [faction terminid|automaton|illuminate|<n>], "
                               "[operation <0..%u>] or [operation id <hex>], an id from the catalogue)", line, OPERATION_ROWS - 1);
                errors++;
                continue;
            }
            g_sec[g_nsec] = sec;
            cur = &g_sec[g_nsec++];
            continue;
        }

        char* eq = strchr(s, '=');
        if (eq)
        {
            *eq = 0;
            char* key = Trim(s);
            int value = atoi(Trim(eq + 1));
            if (!_stricmp(key, "enabled")) g_enabled = value != 0;
            else if (!_stricmp(key, "cross_faction")) g_crossFaction = value != 0;
            else { hf->log(g_mod, "mission-groups.txt line %d: unknown option '%s'", line, key); errors++; }
            continue;
        }

        char* a = strtok(s, " \t");
        char* b = a ? strtok(NULL, " \t") : NULL;
        char* c = b ? strtok(NULL, " \t") : NULL;
        char* d = c ? strtok(NULL, " \t") : NULL;
        if (a && !_stricmp(a, "use"))
        {
            if (cur == NULL) { hf->log(g_mod, "mission-groups.txt line %d: 'use' outside a section", line); errors++; continue; }
            if (cur->source != -2) { hf->log(g_mod, "mission-groups.txt line %d: the section already has a 'use'", line); errors++; continue; }
            int src = d ? -2 : ParseOperation(b, c, NULL);
            if (src == -2)
            {
                hf->log(g_mod, "mission-groups.txt line %d: 'use' takes an operation row (0..%u), 'id <hex>' or 'keep' "
                               "(the catalogue lists them)", line, OPERATION_ROWS - 1);
                errors++;
                continue;
            }
            cur->source = src;
            continue;
        }
        /* 1.x listed missions here; a host-only mission list does not reach joiners, so it is refused. */
        hf->log(g_mod, "mission-groups.txt line %d: not understood. A section takes one 'use <operation row>' line; lists of "
                       "missions are not possible host-only (joiners derive the missions from the operation's vanilla row)", line);
        errors++;
    }
    for (int i = 0; i < g_nsec; i++)
        if (g_sec[i].source == -2)
        {
            hf->log(g_mod, "mission-groups.txt line %d: the section has no 'use' line", g_sec[i].line);
            errors++;
        }
    return errors;
}

/* Rereads mission-groups.txt when it changed; returns 1 when it did. */
static int LoadConfig(void)
{
    char path[1024];
    wchar_t wide[1024];
    WIN32_FILE_ATTRIBUTE_DATA info;
    snprintf(path, sizeof path, "%s\\mission-groups.txt", hf->mod_folder(g_mod));
    if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wide, 1024)) return 0;
    if (!GetFileAttributesExW(wide, GetFileExInfoStandard, &info))
    {
        if (g_configSeen == 2) return 0;
        g_configSeen = 2;
        g_nsec = 0;
        g_configOk = 0;
        hf->log(g_mod, "no mission-groups.txt in the mod's folder: the game's own operations");
        return 1;
    }
    if (g_configSeen == 1 && CompareFileTime(&info.ftLastWriteTime, &g_configTime) == 0) return 0;
    g_configSeen = 1;
    g_configTime = info.ftLastWriteTime;
    if (info.nFileSizeHigh != 0 || info.nFileSizeLow > CONFIG_MAX)
    {
        hf->log(g_mod, "mission-groups.txt is larger than %d bytes: not read", CONFIG_MAX);
        g_nsec = 0;
        g_configOk = 0;
        return 1;
    }
    FILE* f = _wfopen(wide, L"rb");
    if (f == NULL) { hf->log(g_mod, "cannot open mission-groups.txt"); return 0; }
    static char text[CONFIG_MAX + 1];
    size_t n = fread(text, 1, CONFIG_MAX, f);
    fclose(f);
    text[n] = 0;
    int errors = ParseConfig(text);
    g_configOk = errors == 0; /* a config with mistakes changes nothing */
    if (errors)
        hf->log(g_mod, "mission-groups.txt has %d mistake(s) (above): the game's own operations until it is fixed", errors);
    else
        hf->log(g_mod, "mission-groups.txt read: %d section(s), enabled %d, cross_faction %d", g_nsec, g_enabled, g_crossFaction);
    return 1;
}

/* ---- the plan --------------------------------------------------------------------------------------------------- */

/* The section for row r: [operation] over [faction] over [all]; the first of equal ones. */
static const Section* SectionFor(uint32_t r)
{
    const Section* best = NULL;
    int rank = 0;
    for (int i = 0; i < g_nsec; i++)
    {
        const Section* s = &g_sec[i];
        int k = 0;
        if (s->kind == SEC_OPERATION_ROW && s->key == r) k = 3;
        else if (s->kind == SEC_FACTION && s->key == g_opFaction[r]) k = 2;
        else if (s->kind == SEC_ALL) k = 1;
        if (k > rank) { rank = k; best = s; }
    }
    return best;
}

static void Plan(int want, int log)
{
    g_substituted = 0;
    for (uint32_t r = 0; r < OPERATION_ROWS; r++)
    {
        g_plan[r] = -1;
        if (!want) continue;
        const Section* s = SectionFor(r);
        if (s == NULL || s->source < 0 || (uint32_t)s->source == r) continue;
        int src = s->source;
        if (!g_crossFaction && g_opFaction[src] != g_opFaction[r])
        {
            if (log)
                hf->log(g_mod, "  row %u (%s) keeps its own: row %d is %s (cross_faction = 0)", r, FactionName(g_opFaction[r]), src,
                        FactionName(g_opFaction[src]));
            continue;
        }
        g_plan[r] = src;
        g_substituted++;
    }
    if (!log) return;
    for (uint32_t r = 0; r < OPERATION_ROWS; r++)
        if (g_plan[r] >= 0)
            hf->log(g_mod, "  row %u %08x (%s) -> creates row %d's operation %08x (%u missions)", r, g_opId[r],
                    FactionName(g_opFaction[r]), g_plan[r], g_opId[g_plan[r]], ListCount(g_orig[g_plan[r]]));
    /* A row whose id no row keeps any more cannot be looked up on this machine until the rows are put back. */
    for (uint32_t r = 0; r < OPERATION_ROWS; r++)
    {
        if (g_plan[r] < 0) continue;
        int kept = 0;
        for (uint32_t i = 0; i < OPERATION_ROWS; i++)
            kept |= g_opId[g_plan[i] >= 0 ? (uint32_t)g_plan[i] : i] == g_opId[r];
        if (!kept)
            hf->log(g_mod, "  note: operation %08x (row %u) is not offered on this machine while this config is on", g_opId[r], r);
    }
}

/* Writes the plan (want 1) or the game's own rows (want 0). */
static void Apply(int want, int log)
{
    if (log) hf->log(g_mod, want ? "applying mission-groups.txt:" : "putting the game's own operation rows back");
    Plan(want, log);
    uint32_t written = 0;
    for (uint32_t r = 0; r < OPERATION_ROWS; r++)
    {
        const uint8_t* desired = g_orig[g_plan[r] >= 0 ? (uint32_t)g_plan[r] : r];
        uint8_t current[OPERATION_ROW];
        if (!Rd(OpRow(r), current, OPERATION_ROW)) continue;
        if (memcmp(current, desired, OPERATION_ROW) == 0) continue;
        if (hf->write(OpRow(r), desired, OPERATION_ROW) != 0)
        {
            hf->log(g_mod, "operation row %u: the write was refused", r);
            continue;
        }
        written++;
    }
    if (log || written) hf->log(g_mod, "%u operation row(s) written; %u row(s) now create another vanilla operation", written, g_substituted);
}

/* ---- the backend's operation records ---------------------------------------------------------------------------- */

static int JsonNumber(const char* text, const char* key, long long* out)
{
    char pattern[64];
    snprintf(pattern, sizeof pattern, "\"%s\"", key);
    const char* at = strstr(text, pattern);
    if (at == NULL) return 0;
    at = strchr(at + strlen(pattern), ':');
    if (at == NULL) return 0;
    *out = strtoll(at + 1, NULL, 10);
    return 1;
}

static void LogOperationRecord(const wchar_t* path, const char* name)
{
    FILE* f = _wfopen(path, L"rb");
    if (f == NULL) return;
    char text[4096];
    size_t n = fread(text, 1, sizeof text - 1, f);
    fclose(f);
    text[n] = 0;
    long long settings = 0, seed = 0, planet = -1, difficulty = 0, race = 0;
    if (!JsonNumber(text, "operationSettingsId", &settings)) return;
    JsonNumber(text, "seed", &seed);
    JsonNumber(text, "planetIndex", &planet);
    JsonNumber(text, "difficulty", &difficulty);
    JsonNumber(text, "race", &race);
    int row = OperationRow((uint32_t)settings);
    int planned = 0;
    for (uint32_t r = 0; r < OPERATION_ROWS; r++)
        if (g_plan[r] == row) planned = 1;
    hf->log(g_mod, "operation created (%s): planet %lld, difficulty %lld, race %lld, operation %08llx = vanilla row %d (%s), seed %lld%s",
            name, planet, difficulty, race, (unsigned long long)(uint32_t)settings, row, row >= 0 ? FactionName(g_opFaction[row]) : "?", seed,
            planned ? " - from mission-groups.txt" : "");
}

/* New *-Operation_Create.json files in <session>\backend\mission-bodies. */
static void ScanOperationRecords(void)
{
    if (!HAS(logs_folder)) return;
    char path[1024];
    wchar_t wide[1024], file[1100];
    snprintf(path, sizeof path, "%s\\backend\\mission-bodies\\*-Operation_Create.json", hf->logs_folder(g_mod));
    if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wide, 1024)) return;
    WIN32_FIND_DATAW found;
    HANDLE h = FindFirstFileW(wide, &found);
    if (h == INVALID_HANDLE_VALUE) return;
    do
    {
        char name[64];
        if (!WideCharToMultiByte(CP_UTF8, 0, found.cFileName, -1, name, sizeof name, NULL, NULL)) continue;
        int seen = 0;
        for (int i = 0; i < g_nBodySeen && !seen; i++) seen = strcmp(g_bodySeen[i], name) == 0;
        if (seen || g_nBodySeen == MAX_BODIES) continue;
        memcpy(g_bodySeen[g_nBodySeen++], name, sizeof name);
        wchar_t* star = wcsrchr(wide, L'\\');
        size_t dir = star ? (size_t)(star - wide) + 1 : 0;
        wcsncpy(file, wide, dir);
        file[dir] = 0;
        wcsncat(file, found.cFileName, 1100 - dir - 1);
        LogOperationRecord(file, name);
    } while (FindNextFileW(h, &found));
    FindClose(h);
}

/* ---- the game thread -------------------------------------------------------------------------------------------- */

static uint32_t Role(void)
{
    HfSession s = {0};
    if (!HAS(session)) return HF_SESSION_SOLO;
    hf->session(g_mod, &s);
    return s.role;
}

static void UpdateStatus(uint32_t role)
{
    char text[160];
    if (role == HF_SESSION_JOINER) snprintf(text, sizeof text, "joined: the host's operations (nothing changed here)");
    else if (!g_configOk)
        snprintf(text, sizeof text, g_configSeen == 2 ? "no mission-groups.txt: the game's own operations"
                                                      : "mission-groups.txt has mistakes (see the log): the game's own operations");
    else if (!g_enabled) snprintf(text, sizeof text, "off (enabled = 0)");
    else snprintf(text, sizeof text, "%s: %u operation row(s) follow mission-groups.txt", RoleName(role), g_substituted);
    Status(text);
}

static void Tick(HfMod* mod, float dt, void* user)
{
    (void)mod; (void)user;
    if (!g_ready) return;
    g_wait += dt;
    g_bodyWait += dt;
    if (g_bodyWait >= 3.0f)
    {
        g_bodyWait = 0.0f;
        ScanOperationRecords();
    }
    if (g_wait < 1.0f) return;
    g_wait = 0.0f;
    uint32_t state = HAS(game_state) ? hf->game_state(g_mod) : HF_STATE_SHIP;
    /* Rows change only between missions. */
    if (state == HF_STATE_MISSION || state == HF_STATE_PREPARE_MISSION) return;
    if (LoadConfig()) g_dirty = 1;
    uint32_t role = Role();
    int want = g_configOk && g_enabled && role != HF_SESSION_JOINER;
    if (want == g_applied && !g_dirty) return;
    if (role == HF_SESSION_JOINER && g_applied == 1) hf->log(g_mod, "joined a lobby: the host's operations; the game's rows back");
    Apply(want, g_dirty || want != g_applied);
    g_applied = want;
    g_dirty = 0;
    UpdateStatus(role);
}

/* At a mission start, on any machine: the operation and mission rows the mission's data names, and its first bytes. */
static void MissionStart(HfMod* mod, const HfStateEvent* event, void* user)
{
    (void)mod; (void)event; (void)user;
    if (!g_ready) return;
    uint32_t role = Role();
    uint64_t gameMode = 0, mission = 0;
    if (!Rd(g_game + RVA_GAME_MODE, &gameMode, 8) || !Rd((uintptr_t)gameMode + 0x38, &mission, 8) || mission == 0)
    {
        hf->log(g_mod, "mission start (%s): no mission data to read", RoleName(role));
        return;
    }
    uint8_t flags = 0;
    Rd((uintptr_t)mission + 0x14, &flags, 1);
    hf->log(g_mod, "mission start (%s, host bit %u, config %s here): mission data %#llx", RoleName(role), flags & 1u,
            g_applied == 1 ? "applied" : "not applied", (unsigned long long)mission);
    uint32_t hits = 0;
    for (uint32_t at = 0; at < MISSION_SCAN; at += 0x100)
    {
        uint8_t chunk[0x100];
        if (!Rd((uintptr_t)mission + at, chunk, sizeof chunk)) break;
        if (at == 0)
        {
            char hex[0x100 * 2 + 1];
            for (uint32_t b = 0; b < sizeof chunk; b++) snprintf(hex + 2 * b, 3, "%02x", chunk[b]);
            hf->log(g_mod, "  data +0: %s", hex);
        }
        for (uint32_t o = 0; o + 4 <= sizeof chunk && hits < 16; o += 4)
        {
            uint32_t v;
            memcpy(&v, chunk + o, 4);
            if (v == 0) continue;
            int row = MissionRow(v);
            if (row >= 0)
            {
                hf->log(g_mod, "  data +%#x: mission row %d id %08x (%s, kind %u, levels %u..%u)", at + o, row, v,
                        FactionName(g_mFaction[row]), g_mKind[row], g_mLo[row], g_mHi[row]);
                hits++;
            }
            int op = OperationRow(v);
            if (op >= 0)
            {
                hf->log(g_mod, "  data +%#x: operation %08x = vanilla row %d (%s)", at + o, v, op, FactionName(g_opFaction[op]));
                hits++;
            }
        }
    }
    if (hits == 0) hf->log(g_mod, "  no mission or operation id in the first %#x bytes", MISSION_SCAN);
}

static void Ready(HfMod* mod, HfEvent event, void* user)
{
    (void)mod; (void)event; (void)user;
    if (g_ready || !ReadTables()) return;
    WriteCatalog();
    g_ready = 1;
    g_dirty = 1;
    if (!HAS(on_game_thread) || !hf->on_game_thread(g_mod, Tick, NULL))
    {
        hf->log(g_mod, "no game-thread callback: nothing is changed");
        Status("off: no game-thread callback");
        g_ready = 0;
    }
}

HF_MOD_EXPORT int hellforge_mod_init(const HfApi* api, HfMod* mod)
{
    hf = api;
    g_mod = mod;
    if (hf->version < 16) return 1;
    if (strcmp(hf->build_id(), BUILD) != 0)
    {
        hf->log(mod, "game build %s is not " BUILD ": nothing is done", hf->build_id());
        Status("off: this game build is not supported");
        return 0;
    }
    hf->on(mod, HF_EVENT_ENTITIES_READY, Ready, NULL);
    if (!hf_on_mission_start(mod, MissionStart, NULL, 0)) hf->log(mod, "mission starts are not logged");
    Status("waiting for the game's data");
    return 0;
}
/* No unload export: every change is an hf->write, which the loader puts back when the mod is taken out. */
