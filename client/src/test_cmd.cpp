// Test build only (-DLIA_TEST): runs lines from lia_cmd.txt through real game methods.
#ifdef LIA_TEST
#include <windows.h>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include "ap.h"
#include "gen_rva.h"
#include "gen_table.h"
typedef void* obj;
void test_tick(uintptr_t ga) {
    static int t;
    if (++t % 30) return;
    std::ifstream f("lia_cmd.txt");
    if (!f) return;
    std::string line;
    std::getline(f, line);
    f.close();
    DeleteFileA("lia_cmd.txt");
    alog("[cmd] %s", line.c_str());
    std::istringstream s(line);
    std::string c; s >> c;
    auto mgr = [&](uintptr_t d) { ((void (*)(void*))(ga + F_MdInit))((void*)(ga + d)); return ((obj(*)(void*))(ga + F_Find))(*(void**)(ga + d)); };
    if (c == "money") {
        float v; s >> v;
        ((void (*)(obj, float, void*))(ga + R_AddMoney))(mgr(D_MoneyMgr), v, nullptr);
    } else if (c == "reward") {
        int id; s >> id;
        extern void test_reward(int); test_reward(id);
    } else if (c == "buy_modifier") {
        int id; s >> id;
        extern void test_buy(int); test_buy(id);
    } else if (c == "key") {
        int vk; s >> vk;
        INPUT in[2]{};
        in[0].type = in[1].type = INPUT_KEYBOARD;
        in[0].ki.wVk = in[1].ki.wVk = (WORD)vk;
        in[0].ki.wScan = in[1].ki.wScan = (WORD)MapVirtualKeyA(vk, 0);
        in[1].ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &in[0], sizeof(INPUT));
        Sleep(60);
        SendInput(1, &in[1], sizeof(INPUT));
    } else if (c == "feature") {
        int id; s >> id;
        ((void (*)(obj, int, void*))(ga + R_UnlockFeature))(mgr(D_ProgMgr), id, nullptr);
    } else if (c == "isfeature") {
        int id; s >> id;
        alog("[state] feature %d = %d", id, (int)((bool (*)(obj, int, void*))(ga + R_FeatureUnlocked))(mgr(D_ProgMgr), id, nullptr));
    } else if (c == "moved") {
        int id; s >> id;
        alog("[state] modifier %d unlocked=%d", id, (int)((bool (*)(obj, int, void*))(ga + R_ModIsUnlocked))(mgr(D_ModMgr), id, nullptr));
    } else if (c == "tp") {
        float p[3]; s >> p[0] >> p[1] >> p[2];
        extern void test_tp(float*); test_tp(p);
    } else if (c == "state") {
        obj m = mgr(D_MoneyMgr);
        alog("[state] money=%d", ((int (*)(obj, void*))(ga + R_GetMoney))(m, nullptr));
    }
}
#endif
