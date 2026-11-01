// version.dll proxy: forwards every export to the real system DLL.
#include <windows.h>
static HMODULE real;
extern "C" { void* p_fn[17]; }
static const char* names[17] = {"GetFileVersionInfoA","GetFileVersionInfoByHandle","GetFileVersionInfoExA","GetFileVersionInfoExW","GetFileVersionInfoSizeA","GetFileVersionInfoSizeExA","GetFileVersionInfoSizeExW","GetFileVersionInfoSizeW","GetFileVersionInfoW","VerFindFileA","VerFindFileW","VerInstallFileA","VerInstallFileW","VerLanguageNameA","VerLanguageNameW","VerQueryValueA","VerQueryValueW"};
void proxy_init() {
    char path[MAX_PATH];
    GetSystemDirectoryA(path, MAX_PATH);
    lstrcatA(path, "\\version.dll");
    real = LoadLibraryA(path);
    for (int i = 0; i < 17; i++) p_fn[i] = (void*)GetProcAddress(real, names[i]);
}
#define S(i, n) asm(".globl " #n "\n" #n ":\n jmp *p_fn+" #i "*8(%rip)\n");
S(0,GetFileVersionInfoA) S(1,GetFileVersionInfoByHandle) S(2,GetFileVersionInfoExA) S(3,GetFileVersionInfoExW)
S(4,GetFileVersionInfoSizeA) S(5,GetFileVersionInfoSizeExA) S(6,GetFileVersionInfoSizeExW) S(7,GetFileVersionInfoSizeW)
S(8,GetFileVersionInfoW) S(9,VerFindFileA) S(10,VerFindFileW) S(11,VerInstallFileA) S(12,VerInstallFileW)
S(13,VerLanguageNameA) S(14,VerLanguageNameW) S(15,VerQueryValueA) S(16,VerQueryValueW)
