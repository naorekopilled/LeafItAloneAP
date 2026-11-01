// D3D11 Present hook with a Dear ImGui panel. F8 toggles it.
#include <windows.h>
#include <d3d11.h>
#include <dxgi.h>
#include <fstream>
#include <string>
#include "MinHook.h"
#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"
#include "ap.h"
#include "gen_table.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
bool game_level_owned(int scene);
bool game_in_level();

static bool show = true, ready;
static WNDPROC old_proc;
static ID3D11Device* dev;
static ID3D11DeviceContext* ctx;
static char host[128] = "localhost", port[8] = "38281", slot[64] = "", pass[64] = "";
static bool asked;
static HRESULT(STDMETHODCALLTYPE* o_Present)(IDXGISwapChain*, UINT, UINT);

static void load_cfg() {
    std::ifstream f("lia_ap.cfg");
    std::string a, b, c, d;
    if (std::getline(f, a) && std::getline(f, b) && std::getline(f, c)) {
        std::getline(f, d);
        strncpy(host, a.c_str(), 127); strncpy(port, b.c_str(), 7); strncpy(slot, c.c_str(), 63); strncpy(pass, d.c_str(), 63);
    }
}
static void save_cfg() { std::ofstream("lia_ap.cfg") << host << "\n" << port << "\n" << slot << "\n" << pass << "\n"; }

static LRESULT CALLBACK proc(HWND h, UINT m, WPARAM w, LPARAM l) {
    if (m == WM_KEYDOWN && w == VK_F8) show = !show;
    if (ready && show) {
        ImGui_ImplWin32_WndProcHandler(h, m, w, l);
        if (ImGui::GetIO().WantCaptureKeyboard && (m == WM_KEYDOWN || m == WM_KEYUP || m == WM_CHAR)) return 0;
    }
    return CallWindowProcA(old_proc, h, m, w, l);
}
void overlay_connect_now() {
    save_cfg();
    ap.connect(host, atoi(port), slot, pass);
    asked = true;
}
static void panel() {
    ImGui::SetNextWindowSize(ImVec2(460, 420), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x - 480, 20), ImGuiCond_FirstUseEver);
    ImGui::Begin("Leaf it Alone Archipelago (F8)");
    bool active;
    std::string status;
    std::vector<std::string> log;
    { std::lock_guard<std::recursive_mutex> g(ap.mu); active = ap.active; status = ap.status; log = ap.log; }
    ImGui::BeginDisabled(asked);
    ImGui::InputText("Host", host, sizeof host);
    ImGui::InputText("Port", port, sizeof port);
    ImGui::InputText("Slot", slot, sizeof slot);
    ImGui::InputText("Password", pass, sizeof pass, ImGuiInputTextFlags_Password);
    ImGui::EndDisabled();
    if (!asked && !ap.unsupported && ImGui::Button("Connect")) overlay_connect_now();
    ImGui::TextWrapped("Status: %s", status.c_str());
    if (active) {
        ImGui::Separator();
        const char* names[] = {"Hometown", "Castle", "Sakura"};
        int ids[] = {0, 1, 3};
        ImGui::Text("Levels:");
        for (int i = 0; i < 3; i++) {
            ImGui::SameLine();
            ImGui::TextDisabled(game_level_owned(ids[i]) ? "[%s]" : "%s (locked)", names[i]);
        }
        ImGui::TextWrapped("Switch level: Save & Exit, then Load Game and pick the map.");
    }
    ImGui::Separator();
    ImGui::BeginChild("log");
    for (auto& l : log) ImGui::TextWrapped("%s", l.c_str());
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) ImGui::SetScrollHereY(1.0f);
    ImGui::EndChild();
    ImGui::End();
}
// The game swaps between two swapchains and replaces buffers on focus changes, so no view outlives a frame.
static void draw(IDXGISwapChain* sc) {
    DXGI_SWAP_CHAIN_DESC d{};
    RECT r{};
    if (FAILED(sc->GetDesc(&d)) || !GetClientRect(d.OutputWindow, &r) || r.right <= 0 || r.bottom <= 0) return;
    ID3D11Device* cur = nullptr;
    if (FAILED(sc->GetDevice(__uuidof(ID3D11Device), (void**)&cur))) return;
    bool same = cur == dev;
    cur->Release();
    ID3D11Texture2D* bb = nullptr;
    if (!same || FAILED(sc->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&bb))) return;
    ID3D11RenderTargetView* rtv = nullptr;
    HRESULT hr = dev->CreateRenderTargetView(bb, nullptr, &rtv);
    bb->Release();
    if (FAILED(hr)) return;
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ImGui::GetIO().MouseDrawCursor = true;
    panel();
    ImGui::Render();
    ctx->OMSetRenderTargets(1, &rtv, nullptr);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    rtv->Release();
}
static HRESULT STDMETHODCALLTYPE h_Present(IDXGISwapChain* sc, UINT a, UINT b) {
    if (!ready && SUCCEEDED(sc->GetDevice(__uuidof(ID3D11Device), (void**)&dev))) {
        dev->GetImmediateContext(&ctx);
        DXGI_SWAP_CHAIN_DESC d;
        sc->GetDesc(&d);
        ImGui::CreateContext();
        ImGui::GetIO().IniFilename = nullptr;
        ImGui_ImplWin32_Init(d.OutputWindow);
        ImGui_ImplDX11_Init(dev, ctx);
        old_proc = (WNDPROC)SetWindowLongPtrA(d.OutputWindow, GWLP_WNDPROC, (LONG_PTR)proc);
        load_cfg();
        ready = true;
#ifdef LIA_TEST
        if (slot[0]) overlay_connect_now();
#endif
        alog("overlay ready");
    }
    if (ready && show) draw(sc);
    return o_Present(sc, a, b);
}
void overlay_init() {
    WNDCLASSA wc{}; wc.lpfnWndProc = DefWindowProcA; wc.lpszClassName = "liaovl"; wc.hInstance = GetModuleHandleA(nullptr);
    RegisterClassA(&wc);
    HWND w = CreateWindowA("liaovl", "", WS_OVERLAPPED, 0, 0, 16, 16, nullptr, nullptr, wc.hInstance, nullptr);
    DXGI_SWAP_CHAIN_DESC d{};
    d.BufferCount = 1; d.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; d.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    d.OutputWindow = w; d.SampleDesc.Count = 1; d.Windowed = TRUE;
    IDXGISwapChain* sc; ID3D11Device* td; ID3D11DeviceContext* tc;
    HRESULT r = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0, D3D11_SDK_VERSION, &d, &sc, &td, nullptr, &tc);
    alog("dummy swapchain hr=%08lx", (unsigned long)r);
    if (FAILED(r)) return;
    void* present = (*(void***)sc)[8];
    MH_CreateHook(present, (void*)h_Present, (void**)&o_Present);
    MH_EnableHook(present);
    sc->Release(); tc->Release(); td->Release();
    DestroyWindow(w);
}
