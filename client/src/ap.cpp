// Archipelago client. Own WebSocket framing over winsock, with Schannel TLS: wss:// first, then ws://.
// WinHTTP's WebSocket API is not usable here: under Wine it fails on server pings with error 12152.
#include "ap.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#define SECURITY_WIN32
#include <security.h>
#include <schannel.h>
#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <thread>
#include "nlohmann/json.hpp"
#include "gen_table.h"
using json = nlohmann::json;

Ap ap;
static FILE* log_file;
void alog(const char* fmt, ...) {
    static std::mutex m;
    std::lock_guard<std::mutex> g(m);
    if (!log_file) log_file = fopen("lia_ap.log", "w");
    va_list a; va_start(a, fmt); vfprintf(log_file, fmt, a); va_end(a);
    fputc('\n', log_file); fflush(log_file);
}
void Ap::note(const std::string& s) {
    std::lock_guard<std::recursive_mutex> g(mu);
    log.push_back(s);
    if (log.size() > 300) log.erase(log.begin());
    alog("[ap] %s", s.c_str());
}
int Ap::count(int64_t item) {
    std::lock_guard<std::recursive_mutex> g(mu);
    int n = 0;
    for (auto i : received) n += i == item;
    return n;
}
std::string Ap::dir() {
    char up[MAX_PATH] = {};
    GetEnvironmentVariableA("USERPROFILE", up, MAX_PATH);
    std::string base = std::string(up) + "\\AppData\\LocalLow\\Eternity\\Leaf it Alone\\AP";
    CreateDirectoryA(base.c_str(), nullptr);
    std::string d = base + "\\" + seed + "_" + slot;
    CreateDirectoryA(d.c_str(), nullptr);
    return d;
}
void Ap::save() {
    std::lock_guard<std::recursive_mutex> g(mu);
    if (seed.empty()) return;
    json j = {{"checked", checked}, {"applied", applied}};
    std::string p = dir() + "\\state.json", tmp = p + ".tmp";
    { std::ofstream(tmp) << j.dump(); }
    MoveFileExA(tmp.c_str(), p.c_str(), MOVEFILE_REPLACE_EXISTING);
}
static void load_state() {
    std::ifstream f(ap.dir() + "\\state.json");
    if (!f) return;
    json j = json::parse(f, nullptr, false);
    if (j.is_discarded()) return;
    for (auto& c : j["checked"]) ap.checked.insert(c.get<int64_t>());
    for (auto& [k, v] : j["applied"].items()) ap.applied[k] = v;
}

// --- websocket ---
static SOCKET sock = INVALID_SOCKET;
static std::mutex send_mu;
static bool raw_send(const char* d, size_t n) {
    while (n) {
        int r = send(sock, d, (int)n, 0);
        if (r <= 0) return false;
        d += r; n -= r;
    }
    return true;
}

// --- TLS ---
static bool tls_on;
static CredHandle tls_cred;
static CtxtHandle tls_ctx;
static SecPkgContext_StreamSizes tls_sizes;
static std::string tls_in, tls_plain;  // received ciphertext not yet decrypted, decrypted bytes not yet read

static bool tls_read_more() {
    char b[16384];
    int r = recv(sock, b, sizeof b, 0);
    if (r <= 0) return false;
    tls_in.append(b, r);
    return true;
}
static void tls_free() {
    if (!tls_on) return;
    DeleteSecurityContext(&tls_ctx);
    FreeCredentialsHandle(&tls_cred);
    tls_on = false;
    tls_in.clear();
    tls_plain.clear();
}
static bool tls_handshake(const std::string& host) {
    SCHANNEL_CRED sc{};
    sc.dwVersion = SCHANNEL_CRED_VERSION;
    sc.dwFlags = SCH_CRED_NO_DEFAULT_CREDS | SCH_CRED_AUTO_CRED_VALIDATION;
    if (AcquireCredentialsHandleA(nullptr, (char*)UNISP_NAME_A, SECPKG_CRED_OUTBOUND, nullptr, &sc, nullptr, nullptr, &tls_cred, nullptr) != SEC_E_OK) return false;
    const DWORD want = ISC_REQ_SEQUENCE_DETECT | ISC_REQ_REPLAY_DETECT | ISC_REQ_CONFIDENTIALITY | ISC_REQ_ALLOCATE_MEMORY | ISC_REQ_STREAM;
    bool first = true;
    tls_in.clear();
    for (;;) {
        SecBuffer inb[2] = {{(unsigned long)tls_in.size(), SECBUFFER_TOKEN, tls_in.data()}, {0, SECBUFFER_EMPTY, nullptr}};
        SecBuffer outb[1] = {{0, SECBUFFER_TOKEN, nullptr}};
        SecBufferDesc ind{SECBUFFER_VERSION, 2, inb}, outd{SECBUFFER_VERSION, 1, outb};
        DWORD attrs;
        SECURITY_STATUS st = InitializeSecurityContextA(&tls_cred, first ? nullptr : &tls_ctx, (char*)host.c_str(), want, 0, 0,
                                                        first ? nullptr : &ind, 0, first ? &tls_ctx : nullptr, &outd, &attrs, nullptr);
        first = false;
        if (outb[0].cbBuffer && outb[0].pvBuffer) {
            bool ok = raw_send((char*)outb[0].pvBuffer, outb[0].cbBuffer);
            FreeContextBuffer(outb[0].pvBuffer);
            if (!ok) return false;
        }
        if (st == SEC_E_INCOMPLETE_MESSAGE) { if (!tls_read_more()) return false; continue; }
        if (st != SEC_E_OK && st != SEC_I_CONTINUE_NEEDED) { alog("[ap] tls handshake status %08lx", (unsigned long)st); return false; }
        std::string rest;
        if (inb[1].BufferType == SECBUFFER_EXTRA) rest = tls_in.substr(tls_in.size() - inb[1].cbBuffer);
        tls_in = rest;
        if (st == SEC_E_OK) break;
        if (tls_in.empty() && !tls_read_more()) return false;
    }
    QueryContextAttributesA(&tls_ctx, SECPKG_ATTR_STREAM_SIZES, &tls_sizes);
    tls_on = true;
    return true;
}
static bool tls_send(const char* d, size_t n) {
    std::string b;
    while (n) {
        size_t chunk = n < tls_sizes.cbMaximumMessage ? n : tls_sizes.cbMaximumMessage;
        b.resize(tls_sizes.cbHeader + chunk + tls_sizes.cbTrailer);
        memcpy(b.data() + tls_sizes.cbHeader, d, chunk);
        SecBuffer sb[4] = {{tls_sizes.cbHeader, SECBUFFER_STREAM_HEADER, b.data()},
                           {(unsigned long)chunk, SECBUFFER_DATA, b.data() + tls_sizes.cbHeader},
                           {tls_sizes.cbTrailer, SECBUFFER_STREAM_TRAILER, b.data() + tls_sizes.cbHeader + chunk},
                           {0, SECBUFFER_EMPTY, nullptr}};
        SecBufferDesc desc{SECBUFFER_VERSION, 4, sb};
        if (EncryptMessage(&tls_ctx, 0, &desc, 0) != SEC_E_OK) return false;
        if (!raw_send(b.data(), sb[0].cbBuffer + sb[1].cbBuffer + sb[2].cbBuffer)) return false;
        d += chunk; n -= chunk;
    }
    return true;
}
static bool tls_fill() {  // decrypts until at least one plaintext byte is available
    while (tls_plain.empty()) {
        if (tls_in.empty() && !tls_read_more()) return false;
        SecBuffer sb[4] = {{(unsigned long)tls_in.size(), SECBUFFER_DATA, tls_in.data()}, {0, SECBUFFER_EMPTY, nullptr}, {0, SECBUFFER_EMPTY, nullptr}, {0, SECBUFFER_EMPTY, nullptr}};
        SecBufferDesc desc{SECBUFFER_VERSION, 4, sb};
        SECURITY_STATUS st = DecryptMessage(&tls_ctx, &desc, 0, nullptr);
        if (st == SEC_E_INCOMPLETE_MESSAGE) { if (!tls_read_more()) return false; continue; }
        if (st != SEC_E_OK && st != SEC_I_RENEGOTIATE) { alog("[ap] tls decrypt status %08lx", (unsigned long)st); return false; }
        std::string rest;
        for (auto& b : sb) {
            if (b.BufferType == SECBUFFER_DATA) tls_plain.append((char*)b.pvBuffer, b.cbBuffer);
            if (b.BufferType == SECBUFFER_EXTRA) rest.assign((char*)b.pvBuffer, b.cbBuffer);
        }
        tls_in = rest;
    }
    return true;
}

static bool send_all(const char* d, size_t n) { return tls_on ? tls_send(d, n) : raw_send(d, n); }
static bool recv_all(char* d, size_t n) {
    while (n) {
        int r;
        if (tls_on) {
            if (!tls_fill()) return false;
            r = (int)(n < tls_plain.size() ? n : tls_plain.size());
            memcpy(d, tls_plain.data(), r);
            tls_plain.erase(0, r);
        } else {
            r = recv(sock, d, (int)n, 0);
            if (r <= 0) return false;
        }
        d += r; n -= r;
    }
    return true;
}
static bool ws_send(int op, const std::string& s) {
    std::lock_guard<std::mutex> g(send_mu);
    if (sock == INVALID_SOCKET) return false;
    std::string f;
    f.push_back((char)(0x80 | op));
    if (s.size() < 126) f.push_back((char)(0x80 | s.size()));
    else if (s.size() < 65536) { f.push_back((char)(0x80 | 126)); f.push_back((char)(s.size() >> 8)); f.push_back((char)s.size()); }
    else { f.push_back((char)(0x80 | 127)); for (int i = 7; i >= 0; i--) f.push_back((char)((uint64_t)s.size() >> (i * 8))); }
    uint32_t mask = GetTickCount() * 2654435761u;
    char* m = (char*)&mask;
    f.append(m, 4);
    for (size_t i = 0; i < s.size(); i++) f.push_back(s[i] ^ m[i & 3]);
    return send_all(f.data(), f.size());
}
static bool ws_recv(std::string& out) {
    out.clear();
    for (;;) {
        unsigned char h[2];
        if (!recv_all((char*)h, 2)) return false;
        uint64_t len = h[1] & 0x7f;
        if (len == 126) { unsigned char e[2]; if (!recv_all((char*)e, 2)) return false; len = e[0] << 8 | e[1]; }
        else if (len == 127) { unsigned char e[8]; if (!recv_all((char*)e, 8)) return false; len = 0; for (auto b : e) len = len << 8 | b; }
        std::string p(len, 0);
        if (len && !recv_all(p.data(), len)) return false;
        int op = h[0] & 15;
        if (op == 8) return false;
        if (op == 9) { ws_send(10, p); continue; }
        if (op == 10) continue;
        out += p;
        if (h[0] & 0x80) return true;
    }
}
static bool ws_open(const std::string& host, int port, bool secure) {
    addrinfo hints{}, *res;
    hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res)) return false;
    SOCKET s = socket(res->ai_family, SOCK_STREAM, 0);
    if (connect(s, res->ai_addr, (int)res->ai_addrlen)) { freeaddrinfo(res); closesocket(s); return false; }
    freeaddrinfo(res);
    sock = s;
    if (secure && !tls_handshake(host)) return false;
    std::string rq = "GET / HTTP/1.1\r\nHost: " + host + ":" + std::to_string(port) +
        "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n\r\n";
    send_all(rq.data(), rq.size());
    std::string r;
    char c;
    while (r.find("\r\n\r\n") == std::string::npos) {
        if (!recv_all(&c, 1)) return false;
        r.push_back(c);
    }
    return r.find(" 101 ") != std::string::npos;
}

// --- protocol ---
static std::map<std::string, std::map<int64_t, std::string>> item_names, loc_names;
static std::map<int, std::string> player_names, player_games;
static std::string my_game = "Leaf it Alone";
static std::string conn_host, conn_pass;
static int conn_port;
static int my_slot_id;
static bool goal_sent;

static void send_cmd(json p) { ws_send(1, json::array({p}).dump()); }
static std::string item_name(int player, int64_t id) {
    auto& m = item_names[player_games[player]];
    auto it = m.find(id);
    return it == m.end() ? "Item " + std::to_string(id) : it->second;
}
static std::string loc_name(int player, int64_t id) {
    auto& m = loc_names[player_games[player]];
    auto it = m.find(id);
    return it == m.end() ? "Location " + std::to_string(id) : it->second;
}
static std::string print_text(const json& data) {
    std::string out;
    for (auto& d : data) {
        std::string t = d.value("type", "text"), v = d.value("text", "");
        if (t == "player_id") out += player_names[atoi(v.c_str())];
        else if (t == "item_id") out += item_name(d.value("player", 0), atoll(v.c_str()));
        else if (t == "location_id") out += loc_name(d.value("player", 0), atoll(v.c_str()));
        else out += v;
    }
    return out;
}
static void send_pending() {
    std::vector<int64_t> v;
    { std::lock_guard<std::recursive_mutex> g(ap.mu); v.assign(ap.checked.begin(), ap.checked.end()); }
    if (!v.empty()) send_cmd({{"cmd", "LocationChecks"}, {"locations", v}});
}
static void on_packet(const json& p, const std::string& slot) {
    std::string cmd = p.value("cmd", "");
    if (cmd == "RoomInfo") {
        std::lock_guard<std::recursive_mutex> g(ap.mu);
        ap.seed = p.value("seed_name", "seed");
        send_cmd({{"cmd", "GetDataPackage"}, {"games", p["games"]}});
    } else if (cmd == "DataPackage") {
        for (auto& [game, d] : p["data"]["games"].items()) {
            for (auto& [n, id] : d["item_name_to_id"].items()) item_names[game][id.get<int64_t>()] = n;
            for (auto& [n, id] : d["location_name_to_id"].items()) loc_names[game][id.get<int64_t>()] = n;
        }
        send_cmd({{"cmd", "Connect"}, {"password", conn_pass}, {"game", my_game}, {"name", slot}, {"uuid", "leaf-it-alone-client"},
                  {"version", {{"major", 0}, {"minor", 6}, {"build", 8}, {"class", "Version"}}},
                  {"items_handling", 7}, {"tags", json::array()}, {"slot_data", true}});
    } else if (cmd == "Connected") {
        {
            std::lock_guard<std::recursive_mutex> g(ap.mu);
            ap.slot = slot;
            ap.start_scene = p["slot_data"].value("start_scene", 0);
            load_state();
            for (auto& c : p["checked_locations"]) ap.checked.insert(c.get<int64_t>());
            my_slot_id = p.value("slot", 0);
            for (auto& [k, v] : p["slot_info"].items()) { player_games[atoi(k.c_str())] = v.value("game", ""); }
            for (auto& q : p["players"]) player_names[q.value("slot", 0)] = q.value("alias", q.value("name", ""));
            ap.status = "Connected as " + slot;
            ap.active = ap.online = true;
        }
        send_pending();
        std::vector<int64_t> shop;
        for (auto& l : LOCS) if (l.kind <= 1) shop.push_back(l.ap);
        send_cmd({{"cmd", "LocationScouts"}, {"locations", shop}, {"create_as_hint", 0}});
    } else if (cmd == "ConnectionRefused") {
        std::lock_guard<std::recursive_mutex> g(ap.mu);
        ap.status = "Refused: " + p.value("errors", json::array()).dump();
    } else if (cmd == "ReceivedItems") {
        std::lock_guard<std::recursive_mutex> g(ap.mu);
        size_t index = p.value("index", 0);
        if (index == 0) ap.received.clear();
        if (index != ap.received.size()) { send_cmd({{"cmd", "Sync"}}); return; }
        for (auto& i : p["items"]) ap.received.push_back(i["item"].get<int64_t>());
        alog("[ap] received total %zu", ap.received.size());
    } else if (cmd == "LocationInfo") {
        std::lock_guard<std::recursive_mutex> g(ap.mu);
        for (auto& l : p["locations"]) {
            int who = l["player"];
            ScoutInfo s{item_name(who, l["item"].get<int64_t>()), player_names[who], l.value("flags", 0)};
            ap.scouts[l["location"].get<int64_t>()] = s;
        }
    } else if (cmd == "PrintJSON") {
        ap.note(print_text(p["data"]));
    }
}
static void run(std::string host, int port, std::string slot) {
    static WSADATA w;
    WSAStartup(MAKEWORD(2, 2), &w);
    for (;;) {
        { std::lock_guard<std::recursive_mutex> g(ap.mu); ap.status = "Connecting to " + host; }
        bool open = ws_open(host, port, true);
        if (!open) {
            std::lock_guard<std::mutex> g(send_mu);
            if (sock != INVALID_SOCKET) closesocket(sock);
            sock = INVALID_SOCKET;
            tls_free();
            open = ws_open(host, port, false);
        }
        if (open) {
            std::string msg;
            while (ws_recv(msg)) {
                json j = json::parse(msg, nullptr, false);
                if (j.is_array()) for (auto& p : j) {
                    try { on_packet(p, slot); } catch (const std::exception& e) { alog("packet error: %s", e.what()); }
                }
            }
        }
        { std::lock_guard<std::mutex> g(send_mu); if (sock != INVALID_SOCKET) closesocket(sock); sock = INVALID_SOCKET; tls_free(); }
        {
            std::lock_guard<std::recursive_mutex> g(ap.mu);
            ap.online = false;
            ap.status = "Disconnected, retrying";
        }
        ap.note("Lost connection");
        Sleep(3000);
    }
}
void Ap::connect(const std::string& host, int port, const std::string& s, const std::string& password) {
    static bool started;
    if (started) return;
    started = true;
    conn_pass = password;
    std::thread(run, host, port, s).detach();
}
void Ap::check(int64_t location) {
    {
        std::lock_guard<std::recursive_mutex> g(mu);
        if (!checked.insert(location).second) return;
        save();
    }
    alog("[check] %lld", (long long)location);
    send_cmd({{"cmd", "LocationChecks"}, {"locations", json::array({location})}});
}
void Ap::goal() {
    if (goal_sent) return;
    goal_sent = online && ws_send(1, json::array({json{{"cmd", "StatusUpdate"}, {"status", 30}}}).dump());
    if (goal_sent) note("Goal reported");
}
