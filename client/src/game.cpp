// Game hooks. Offsets noted per use were read from the disassembly of the matching build.
#include <windows.h>
#include <cstdio>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include "MinHook.h"
#include "ap.h"
#include "gen_rva.h"
#include "gen_table.h"

#define K(T, N) reinterpret_cast<T>(ga + N)
static uintptr_t ga;
typedef void* obj;

// --- runtime helpers ---
static void* find_mgr(uintptr_t data) {
    K(void (*)(void*), F_MdInit)((void*)(ga + data));
    return K(void* (*)(void*), F_Find)(*(void**)(ga + data));
}
static obj mod_settings(int id) {
    K(void (*)(void*), F_MdInit)((void*)(ga + D_UsoKlass));
    K(void (*)(void*), F_MdInit)((void*)(ga + D_ModRegGet));
    uintptr_t st = *(uintptr_t*)(*(uintptr_t*)(ga + D_UsoKlass) + 0xa0);
    return K(obj (*)(obj, int, void*), F_RegGet)(*(obj*)(st + 8), id, *(void**)(ga + D_ModRegGet));
}
static obj mkstr(const std::string& s) {
    wchar_t w[512];
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, w, 512);
    if (n <= 0) return nullptr;
    return K(obj (*)(const wchar_t*, void*), R_PtrToStringUni)(w, nullptr);
}
static void set_text(obj tmp, const std::string& s) {
    if (tmp) K(void (*)(obj, obj, void*), R_TmpSetText)(tmp, mkstr(s), nullptr);
}
static int scene_id() {
    obj gs = K(obj (*)(void*), R_GetGameState)(nullptr);
    return gs ? *(unsigned short*)((char*)gs + 0x28) : -1;
}
static const LocRow* find_loc(int kind, int game, int scene) {
    for (auto& l : LOCS)
        if (l.kind == kind && l.game == game && (scene < 0 || l.scene == scene)) return &l;
    return nullptr;
}
static const ItemRow* item_by_name(const char* n) {
    for (auto& i : ITEMS) if (!strcmp(i.name, n)) return &i;
    return nullptr;
}
static std::string label(int64_t loc, const char* fallback) {
    std::lock_guard<std::recursive_mutex> g(ap.mu);
    auto it = ap.scouts.find(loc);
    if (it == ap.scouts.end()) return fallback;
    return it->second.item + " for " + it->second.player;
}

// --- suppression of vanilla effects ---
static int suppress;   // >0 while a purchase or reward runs: record, grant nothing
static bool applying;  // true while the client applies a received item

static void (*o_Claim)(obj, void*);
static void h_Claim(obj s, void* m) {
    if (ap.active && suppress) return;
    o_Claim(s, m);
}
static void (*o_Reclaim)(obj, void*);
static void h_Reclaim(obj s, void* m) {
    if (ap.active && !applying) return;  // effects return through apply_items
    o_Reclaim(s, m);
}
static void (*o_ModUnlock)(obj, int, void*);
static void h_ModUnlock(obj self, int id, void* m) {
    if (ap.active && suppress) {
        if (auto* l = find_loc(0, id & 0xffff, -1)) ap.check(l->ap);
    }
    o_ModUnlock(self, id, m);
}
static void (*o_Buy)(obj, void*);
static void h_Buy(obj self, void* m) {
    if (!ap.active) return o_Buy(self, m);
    suppress++;
    o_Buy(self, m);
    suppress--;
}
static void (*o_Reward)(obj, int, void*);
static void h_Reward(obj self, int id, void* m) {
    if (!ap.active) return o_Reward(self, id, m);
    if (auto* l = find_loc(2, id & 0xffff, -1)) ap.check(l->ap);
    suppress++;
    o_Reward(self, id, m);
    suppress--;
}
static void (*o_ObjDone)(obj, obj, void*);
static void h_ObjDone(obj self, obj objective, void* m) {
    o_ObjDone(self, objective, m);
    if (!ap.active || !objective) return;
    int id = K(unsigned short (*)(int, void*, obj), F_IfaceInvoke)(4, *(void**)(ga + D_ObjIface), objective);
    alog("[objective] done %d", id);
    const LocRow* l = find_loc(3, id, -1);
    if (!l) l = find_loc(4, id, -1);
    if (l) ap.check(l->ap);
}
static bool (*o_Avail)(obj, void*);
static bool h_Avail(obj self, void* m) { return ap.active ? true : o_Avail(self, m); }

// --- tools and item shop ---
static bool tool_owned(int actor) {
    for (auto& i : ITEMS)
        if (i.kind == 3 && i.ids[0] == actor) return ap.count(i.ap) > 0;
    return true;
}
static void (*o_Equip)(obj, obj, void*);
static void h_Equip(obj self, obj item, void* m) {
    if (ap.active && !applying && item) {
        int id = *(unsigned short*)(*(char**)((char*)item + 0x28) + 0x40);
        bool tool = false;
        for (auto& i : ITEMS) if (i.kind == 3 && i.ids[0] == id) tool = true;
        if (tool && !tool_owned(id)) { alog("[lock] blocked tool %d", id); return; }
    }
    o_Equip(self, item, m);
}
constexpr int PLAYER_ACTOR = 1, TRACTOR_FEATURE = 39;
struct ShopEntry { int actor; float price; };
// Each exhibit sells a chain: bag exhibits advance to the next bag after each purchase.
static const std::map<int, std::vector<ShopEntry>> CHAINS_H = {{19, {{19, 0.75f}, {6, 3.49f}}}, {3, {{3, 7.49f}}}, {4, {{4, 49.99f}}}};
static const std::map<int, std::vector<ShopEntry>> CHAINS_C = {{6, {{6, 7.49f}, {68, 14.99f}}}, {4, {{4, 74.99f}}}};
static const std::map<int, std::vector<ShopEntry>> CHAINS_S = {{90, {{90, 199.99f}}}};
static const ShopEntry* current_entry(obj view, const LocRow** loc) {
    obj actor = *(obj*)((char*)view + 0x88);
    if (!actor) return nullptr;
    int id = *(unsigned short*)(*(char**)((char*)actor + 0x28) + 0x40), sc = scene_id();
    auto& ch = sc == 0 ? CHAINS_H : sc == 1 ? CHAINS_C : CHAINS_S;
    auto it = ch.find(id);
    if (it == ch.end()) return nullptr;
    std::lock_guard<std::recursive_mutex> g(ap.mu);
    for (auto& e : it->second) {
        auto* l = find_loc(1, e.actor, sc);
        if (l && !ap.checked.count(l->ap)) { *loc = l; return &e; }
    }
    *loc = nullptr;
    return &it->second.back();
}
static void (*o_ItemRefresh)(obj, void*);
static void h_ItemRefresh(obj view, void* m) {
    o_ItemRefresh(view, m);
    if (!ap.active) return;
    const LocRow* loc = nullptr;
    auto* e = current_entry(view, &loc);
    obj money = find_mgr(D_MoneyMgr);
    bool can = loc && e && K(bool (*)(obj, float, void*), R_CanAfford)(money, e->price, nullptr);
    K(void (*)(obj, bool, void*), R_SetInteractable)(*(obj*)((char*)view + 0xb8), can, nullptr);
}
static void (*o_ItemOpen)(obj, obj, void*);
static void h_ItemOpen(obj view, obj handle, void* m) {
    o_ItemOpen(view, handle, m);
    if (!ap.active) return;
    const LocRow* loc = nullptr;
    auto* e = current_entry(view, &loc);
    if (!e) return;
    char price[32];
    snprintf(price, sizeof price, "$%.2f", e->price);
    set_text(*(obj*)((char*)view + 0xa8), loc ? price : "SOLD");
    set_text(*(obj*)((char*)view + 0x90), loc ? label(loc->ap, loc->name) : "Sold out");
    h_ItemRefresh(view, nullptr);
}
static void (*o_BuyItem)(obj, void*);
static void h_BuyItem(obj view, void* m) {
    if (!ap.active) return o_BuyItem(view, m);
    const LocRow* loc = nullptr;
    auto* e = current_entry(view, &loc);
    if (!e || !loc) return;
    obj money = find_mgr(D_MoneyMgr);
    if (!K(bool (*)(obj, float, void*), R_CanAfford)(money, e->price, nullptr)) return;
    K(void (*)(obj, float, void*), R_SpendMoney)(money, e->price, nullptr);
    ap.check(loc->ap);
    K(void (*)(obj, void*), R_ItemBack)(view, nullptr);
}
static void (*o_UpgradeUI)(obj, bool, void*);
static void h_UpgradeUI(obj self, bool skip, void* m) {
    o_UpgradeUI(self, skip, m);
    if (!ap.active || *((char*)self + 0xb0) || *((char*)self + 0xb1)) return;  // maxed or locked
    obj st = *(obj*)((char*)self + 0x98);
    obj mod = st ? *(obj*)((char*)st + 0x30) : nullptr;
    if (!mod) return;
    if (auto* l = find_loc(0, *(unsigned short*)((char*)mod + 0x30), -1))
        set_text(*(obj*)((char*)self + 0x30), label(l->ap, l->name));
}

// --- levels ---
static obj (*o_StoryCreate)(int, void*);
static obj h_StoryCreate(int scene, void* m) { return o_StoryCreate(ap.active ? ap.start_scene : scene, m); }
static bool level_owned(int scene) {
    for (auto& i : ITEMS) if (i.kind == 5 && i.ids[0] == scene) return ap.count(i.ap) > 0;
    return false;
}
static void (*o_RequestScene)(obj, obj, int, void*);
static void h_RequestScene(obj save, obj state, int scene, void* m) {
    scene &= 0xffff;
    if (ap.active && scene != ap.start_scene && !level_owned(scene)) {
        ap.note("Level is locked");
        K(void (*)(void*), R_ReturnToMenu)(nullptr);  // the vanilla story flow would otherwise stall here
        return;
    }
    o_RequestScene(save, state, scene, m);
}
static int (*o_UnlockNext)(void*);
static int h_UnlockNext(void* m) { return ap.active ? 0 : o_UnlockNext(m); }

// --- saves ---
static void (*o_Save)(obj, obj, void*);
static void h_Save(obj self, obj save, void* m) {
    o_Save(self, save, m);
    if (ap.active) ap.save();
}
static bool saves_redirected;
static obj main_menu;
static void (*o_MenuRefresh)(obj, void*);
static void h_MenuRefresh(obj self, void* m) {
    main_menu = self;
    o_MenuRefresh(self, m);
}
static void redirect_saves() {
    if (saves_redirected) return;
    saves_redirected = true;
    K(void (*)(void*), F_MdInit)((void*)(ga + D_SaveKlass));
    uintptr_t st = *(uintptr_t*)(*(uintptr_t*)(ga + D_SaveKlass) + 0xa0);
    std::string d = ap.dir();
    *(obj*)st = mkstr(d);  // static holding the absolute saves folder
    alog("[save] redirected to %s", d.c_str());
    if (main_menu) o_MenuRefresh(main_menu, nullptr);  // Continue must reflect this seed's saves
}

// --- leaderboard and stat blocks ---
static int block_ok() { return 1; }
static void block_steam() {
    HMODULE s = GetModuleHandleA("steam_api64.dll");
    if (!s) { alog("[steam] module missing"); return; }
    for (const char* n : {"SteamAPI_ISteamUserStats_UploadLeaderboardScore", "SteamAPI_ISteamUserStats_StoreStats",
                          "SteamAPI_ISteamUserStats_SetAchievement", "SteamAPI_ISteamUserStats_SetStatInt32",
                          "SteamAPI_ISteamUserStats_SetStatFloat", "SteamAPI_ISteamUserStats_UpdateAvgRateStat"}) {
        void* t = (void*)GetProcAddress(s, n);
        void* o;
        int r = t ? MH_CreateHook(t, (void*)block_ok, &o) : -1;
        if (t) MH_EnableHook(t);
        alog("[steam] blocked %s: %d", n, r);
    }
}

// --- applying received items ---
static std::set<std::string> session;
static int last_scene = -1;
static obj last_mod_mgr;
static void claim(int id) {
    std::string k = "m" + std::to_string(id);
    if (!session.insert(k).second) return;
    obj s = mod_settings(id);
    if (!s) { alog("[apply] no modifier %d", id); return; }
    applying = true;
    h_Claim(s, nullptr);
    applying = false;
    alog("[apply] modifier %d", id);
}
static void grant_actor(int actor) {
    obj mgr = find_mgr(D_ActorMgr);
    if (!mgr) return;
    obj eq = K(obj (*)(obj, void*), F_FindBehaviour)(mgr, *(void**)(ga + D_EquipFind));
    if (!eq) return;
    bool tractor = actor == 90;
    obj pm = find_mgr(D_ProgMgr);
    if (tractor ? K(bool (*)(obj, int, void*), R_FeatureUnlocked)(pm, TRACTOR_FEATURE, nullptr)
                : K(bool (*)(obj, int, void*), R_HasItem)(eq, actor, nullptr)) return;
    K(void (*)(void*), F_MdInit)((void*)(ga + D_EmptyStates));
    uintptr_t k = *(uintptr_t*)(ga + D_EmptyStates);
    uintptr_t rg = *(uintptr_t*)(k + 0x38);
    uintptr_t st = *(uintptr_t*)(*(uintptr_t*)(rg + 0x10) + 0xa0);
    float pos[3] = {0, 0, 0};
    obj player = K(obj (*)(obj, int, void*), R_FirstActor)(mgr, PLAYER_ACTOR, nullptr);
    if (player) {  // the exhibit stays for its shop check, so a received tool is a new actor by the player
        obj tr = K(obj (*)(obj, void*), R_GetTransform)(player, nullptr);
        K(void (*)(float*, obj, void*), R_TransformGetPos)(pos, tr, nullptr);
        pos[0] += 1.5f;
    }
    obj a = K(obj (*)(obj, int, float*, obj, void*), R_CreateActor)(*(obj*)((char*)mgr + 0x98), actor, pos, *(obj*)st, nullptr);
    if (!a) { alog("[apply] create failed %d", actor); return; }
    if (tractor) {
        K(void (*)(obj, int, void*), R_UnlockFeature)(pm, TRACTOR_FEATURE, nullptr);
    } else {
        applying = true;
        o_Equip(eq, a, nullptr);
        applying = false;
    }
    alog("[apply] actor %d", actor);
}
static bool sets_ready;
static void apply_items(int scene) {
    obj money = find_mgr(D_MoneyMgr);
    int tokens = 0;
    for (auto& it : ITEMS) {
        int c = ap.count(it.ap);
        if (it.kind == 6) tokens = c;
        if (!c) continue;
        switch (it.kind) {
        case 0: case 1:
            if (it.scene == scene) for (int i = 0; i < c && i < it.n; i++) claim(it.ids[i]);
            break;
        case 2: {
            obj pm = find_mgr(D_ProgMgr);
            if (it.scene == scene) for (int i = 0; i < it.n; i++)
                if (!K(bool (*)(obj, int, void*), R_FeatureUnlocked)(pm, it.ids[i], nullptr)) {
                    obj pmo = pm;
                    K(void (*)(obj, int, void*), R_UnlockFeature)(pmo, it.ids[i], nullptr);
                }
            break;
        }
        case 3: {
            grant_actor(it.ids[0]);
            break;
        }
        case 4:
            if (it.scene == scene) grant_actor(it.ids[(c < it.n ? c : it.n) - 1]);  // only the best bag owned
            break;
        case 7: {
            if (it.scene >= 0 && it.scene != scene) break;
            std::lock_guard<std::recursive_mutex> g(ap.mu);
            int& done = ap.applied[it.name];
            while (done < c) {
                K(void (*)(obj, float, void*), R_AddMoney)(money, it.amount, nullptr);
                done++;
                alog("[apply] money %.2f", it.amount);
            }
            break;
        }
        }
    }
    if (tokens >= 3) ap.goal();
    // The Load Game screen offers every scene in this set, so travel stays on the vanilla save path.
    obj gsave = K(obj (*)(void*), R_GetGameSave)(nullptr);
    obj mode = gsave ? *(obj*)((char*)gsave + 0x68) : nullptr;
    obj scenes = mode ? *(obj*)((char*)mode + 0x10) : nullptr;
    if (scenes && !sets_ready) { K(void (*)(void*), F_MdInit)((void*)(ga + D_HashAdd)); sets_ready = true; }
    if (scenes) for (auto& it : ITEMS)
        if (it.kind == 5 && ap.count(it.ap))
            K(bool (*)(obj, int, void*), F_HashAdd)(scenes, it.ids[0], *(void**)(ga + D_HashAdd));
}
void test_tick(uintptr_t ga);
void overlay_init();
static void (*o_Update)(obj, void*);
static void h_Update(obj self, void* m) {
    o_Update(self, m);
    static bool started;
    if (!started) {  // Steam and D3D setup race with work done any earlier
        started = true;
        block_steam();
        overlay_init();
    }
    if (!ap.active) return;
    redirect_saves();
    if (!K(obj (*)(void*), R_GetGameState)(nullptr)) return;
    int sc = scene_id();
    obj mm = sc >= 0 ? find_mgr(D_ModMgr) : nullptr;
    if (!mm || !find_mgr(D_MoneyMgr)) { last_mod_mgr = nullptr; return; }
    if (sc != last_scene || mm != last_mod_mgr) {
        last_scene = sc; last_mod_mgr = mm;
        session.clear();
        alog("[level] scene %d ready", sc);
    }
    static int tick;
    if (++tick % 15 == 0) apply_items(sc);
#ifdef LIA_TEST
    test_tick(ga);
#endif
}

// --- overlay entry points ---
bool game_level_owned(int scene) { return scene == ap.start_scene || level_owned(scene); }
bool game_in_level() { return ga && K(obj (*)(void*), R_GetGameState)(nullptr); }

#define HOOK(R, f, o) MH_CreateHook((void*)(ga + R), (void*)f, (void**)&o)
extern "C" __declspec(dllexport) void game_module_ready() {}
DWORD WINAPI game_main(void*) {
    while (!(ga = (uintptr_t)GetModuleHandleA("GameAssembly.dll"))) Sleep(20);
    MH_Initialize();
    // Addresses are per build. On any other build stay out of the way instead of crashing the game.
    WIN32_FILE_ATTRIBUTE_DATA fa{};
    GetFileAttributesExA("GameAssembly.dll", GetFileExInfoStandard, &fa);
    if ((((unsigned long long)fa.nFileSizeHigh << 32) | fa.nFileSizeLow) != GA_SIZE) {
        alog("unsupported game build, mod disabled");
        { std::lock_guard<std::recursive_mutex> g(ap.mu); ap.status = "Unsupported game build: update the mod"; ap.unsupported = true; }
        while (!GetModuleHandleA("steam_api64.dll")) Sleep(100);
        Sleep(15000);  // no tick hook on this path, so wait out startup before touching D3D
        overlay_init();
        return 0;
    }
    HOOK(R_Claim, h_Claim, o_Claim); HOOK(R_Reclaim, h_Reclaim, o_Reclaim); HOOK(R_ModUnlock, h_ModUnlock, o_ModUnlock);
    HOOK(R_BuyNextUpgrade, h_Buy, o_Buy); HOOK(R_RewardUnlock, h_Reward, o_Reward); HOOK(R_ObjCompleted, h_ObjDone, o_ObjDone);
    HOOK(R_UpgradeAvailable, h_Avail, o_Avail); HOOK(R_EquipItem, h_Equip, o_Equip); HOOK(R_ItemOpen, h_ItemOpen, o_ItemOpen);
    HOOK(R_ItemRefresh, h_ItemRefresh, o_ItemRefresh); HOOK(R_BuyItem, h_BuyItem, o_BuyItem); HOOK(R_UpgradeEntryUpdateUI, h_UpgradeUI, o_UpgradeUI);
    HOOK(R_StoryCreate, h_StoryCreate, o_StoryCreate); HOOK(R_RequestScene, h_RequestScene, o_RequestScene); HOOK(R_UnlockNextScene, h_UnlockNext, o_UnlockNext);
    HOOK(R_SaveGameSave, h_Save, o_Save); HOOK(R_TickUpdate, h_Update, o_Update); HOOK(R_MenuRefresh, h_MenuRefresh, o_MenuRefresh);
    int e = MH_EnableHook(MH_ALL_HOOKS);
    alog("hooks enabled: %d", e);
    return 0;
}
#ifdef LIA_TEST
void test_tp(float* p) {
    obj player = K(obj (*)(obj, int, void*), R_FirstActor)(find_mgr(D_ActorMgr), PLAYER_ACTOR, nullptr);
    obj tr = K(obj (*)(obj, void*), R_GetTransform)(player, nullptr);
    K(void (*)(obj, float*, void*), R_TransformSetPos)(tr, p, nullptr);
}
void test_reward(int id) { h_Reward(find_mgr(D_RewardMgr), id, nullptr); }
void test_buy(int id) {
    suppress++;
    o_ModUnlock(find_mgr(D_ModMgr), id, nullptr);
    if (auto* l = find_loc(0, id, -1)) ap.check(l->ap);
    suppress--;
}
#endif
