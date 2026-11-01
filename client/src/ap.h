#pragma once
#include <cstdint>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <vector>

struct ScoutInfo { std::string item, player; int flags = 0; };

struct Ap {
    std::recursive_mutex mu;
    std::string status = "Not connected";
    std::vector<std::string> log;
    std::vector<int64_t> received;      // full list, server order
    std::set<int64_t> checked;          // local checks, includes those not yet acknowledged
    std::map<int64_t, ScoutInfo> scouts;
    std::map<std::string, int> applied; // persistent one-shot counters, e.g. money
    std::string seed, slot;
    int start_scene = 0;
    bool active = false;                // true once a slot was joined; stays true across drops
    bool online = false;
    bool unsupported = false;

    void connect(const std::string& host, int port, const std::string& slot, const std::string& password);
    void check(int64_t location);       // safe from any thread, also while offline
    void goal();
    void save();
    int count(int64_t item);
    std::string dir();
    void note(const std::string& line);
};
extern Ap ap;
void alog(const char* fmt, ...);
