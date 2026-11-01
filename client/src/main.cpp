#include <windows.h>
void proxy_init();
DWORD WINAPI game_main(void*);
BOOL WINAPI DllMain(HINSTANCE h, DWORD why, void*) {
    if (why == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(h);
        proxy_init();
        CreateThread(nullptr, 0, game_main, nullptr, 0, nullptr);
    }
    return TRUE;
}
