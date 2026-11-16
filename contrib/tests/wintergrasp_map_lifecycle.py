#!/usr/bin/env python3
"""Exercise the actual map callback with fake map/battlefield dependencies."""
from pathlib import Path
import re
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
source = repo / "src/server/scripts/Northrend/zone_wintergrasp.cpp"
text = source.read_text()

# ScriptMgr returns after the first WorldMapScript matching a map ID. A second
# independent registration can silently suppress the army or its map telemetry.
registrations = re.findall(r'WorldMapScript\s*\(\s*"([^"]+)"\s*,\s*571\s*\)', text)
assert registrations == ["WintergraspBotWarMapScript"], registrations

start = text.index("    void OnUpdate(Map* map, uint32 diff) override", text.index("class WintergraspBotWarMapScript"))
brace = text.index("{", start)
depth = 1
end = brace + 1
while depth:
    depth += (text[end] == "{") - (text[end] == "}")
    end += 1
callback = text[start:end].replace(" override", "")

harness = r'''
#include "src/server/scripts/Northrend/wintergrasp_tactics.h"
#include <array>
#include <cassert>
#include <iostream>
using uint32 = std::uint32_t;
using namespace WintergraspBotWar;
enum TeamId { TEAM_ALLIANCE, TEAM_HORDE, TEAM_NEUTRAL };
enum { BATTLEFIELD_WG_DATA_VEHICLE_A, BATTLEFIELD_WG_DATA_VEHICLE_H,
       BATTLEFIELD_WG_DATA_MAX_VEHICLE_A, BATTLEFIELD_WG_DATA_MAX_VEHICLE_H };
struct Creature { bool IsAlive() const { return true; } };
struct Map { Creature* GetCreature(int) { return nullptr; } };
struct Config
{
    bool enabled = true;
    bool GetBoolDefault(char const*, bool, bool) { return enabled; }
    int GetIntDefault(char const*, int, bool) { return 120; }
} config;
auto* sConfigMgr = &config;
struct BattlefieldWG
{
    bool active = false;
    bool enabled = true;
    uint32 serial = 0;
    std::array<uint32, 2> humans{};
    TeamId GetDefenderTeam() const { return TEAM_HORDE; }
    uint32 GetBotWarBattleSerial() const { return serial; }
    uint32 GetWarPlayerCount(TeamId team) const { return humans[team]; }
    uint32 GetMaxPlayersPerTeam() const { return 120; }
    uint32 GetData(int) const { return 0; }
} battlefield;
BattlefieldWG* GetBotWarBattlefield()
{
    return battlefield.active && battlefield.enabled && config.enabled ? &battlefield : nullptr;
}
template<class... T> void TestLog(T const&...) { }
#define TC_LOG_INFO(...) TestLog(__VA_ARGS__)
struct VehicleMap
{
    uint32 updates = 0;
    void Update(Map*, uint32) { ++updates; }
};
struct Controller
{
    struct RosterSlot { int Soldier = 0; };
    std::array<std::array<RosterSlot, MaxParticipants>, 2> _roster{};
    VehicleMap _vehicleMap;
    uint32 _timer = 0, _elapsed = 0, _nextReport = 0, _battleSerial = 0;
    TeamId _defender = TEAM_NEUTRAL;
    bool _running = false;
    uint32 reconciles = 0, cleanups = 0;
    std::array<uint32, 2> budgets{};
    void Reconcile(Map*, BattlefieldWG* wg, TeamId team, uint32 target)
    {
        assert(wg->active && (wg->humans[0] || wg->humans[1]));
        ++reconciles;
        budgets[team] = BotBudget(target, wg->GetMaxPlayersPerTeam(), wg->GetWarPlayerCount(team));
    }
    void Cleanup(Map*) { ++cleanups; _running = false; }
CALLBACK
};

int main()
{
    Map map;
    Controller controller;
    controller.OnUpdate(&map, 1000);
    assert(!controller._running && controller.reconciles == 0);
    battlefield.active = true;
    ++battlefield.serial;
    // A scheduled battle without humans must remain unpopulated indefinitely.
    for (int i = 0; i < 5; ++i) controller.OnUpdate(&map, 180000);
    assert(controller.reconciles == 0 && !controller._running);
    assert(controller._vehicleMap.updates == 6);
    // The first accepted entrant can arrive well after the scheduled start.
    battlefield.humans[TEAM_ALLIANCE] = 1;
    for (int i = 0; i < 3; ++i) controller.OnUpdate(&map, 1000);
    assert(controller.reconciles == 0);
    controller.OnUpdate(&map, 1000);
    assert(controller._running && controller.reconciles == 2);
    assert(controller.budgets[TEAM_ALLIANCE] == 119 && controller.budgets[TEAM_HORDE] == 120);
    // Another late entrant changes the budget without restarting the armies.
    battlefield.humans[TEAM_ALLIANCE] = 2;
    controller.OnUpdate(&map, 1000);
    assert(controller.reconciles == 4 && controller.cleanups == 0);
    assert(controller.budgets[TEAM_ALLIANCE] == 118);
    battlefield.humans[TEAM_ALLIANCE] = 0;
    controller.OnUpdate(&map, 1000);
    assert(!controller._running && controller.cleanups == 1 && controller.reconciles == 4);
    controller.OnUpdate(&map, 180000);
    assert(controller.cleanups == 1 && controller.reconciles == 4);
    battlefield.humans[TEAM_HORDE] = 1;
    controller.OnUpdate(&map, 4000);
    assert(controller._running && controller.reconciles == 6);
    assert(controller.budgets[TEAM_ALLIANCE] == 120 && controller.budgets[TEAM_HORDE] == 119);
    // Battle end, a new scheduled battle, and a fast GM restart.
    battlefield.active = false;
    controller.OnUpdate(&map, 1000);
    assert(controller.cleanups == 2 && !controller._running);
    battlefield.active = true;
    ++battlefield.serial;
    controller.OnUpdate(&map, 4000);
    assert(controller.reconciles == 8);
    ++battlefield.serial;
    controller.OnUpdate(&map, 1000);
    assert(controller.cleanups == 3 && !controller._running);
    controller.OnUpdate(&map, 4000);
    assert(controller.reconciles == 10 && controller._running);
    uint32 telemetry = controller._vehicleMap.updates;
    config.enabled = false;
    controller.OnUpdate(&map, 1000);
    assert(controller.cleanups == 4 && !controller._running);
    assert(controller._vehicleMap.updates == telemetry + 1);
    controller.OnUpdate(&map, 180000);
    assert(controller.cleanups == 4 && controller.reconciles == 10);
    config.enabled = true;
    controller.OnUpdate(&map, 4000);
    assert(controller.reconciles == 12 && controller._running);
    battlefield.enabled = false;
    controller.OnUpdate(&map, 1000);
    assert(controller.cleanups == 5 && !controller._running);
    std::cout << "PASS: one Northrend map script, actual callback lifecycle, human gate, late entrants, telemetry, cleanup and restarts\n";
}
'''.replace("CALLBACK", callback)

with tempfile.TemporaryDirectory(prefix="wg-map-lifecycle-") as tmp:
    cpp = Path(tmp) / "test.cpp"
    exe = Path(tmp) / "test"
    cpp.write_text(harness)
    subprocess.run(["c++", "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror", "-I", str(repo), str(cpp), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
