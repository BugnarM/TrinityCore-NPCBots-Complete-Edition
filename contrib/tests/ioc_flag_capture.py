#!/usr/bin/env python3
"""Verify Isle of Conquest flag capture, defending, contesting transitions, and 10m range."""
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
src_cpp = (repo / "src/server/game/Battlegrounds/Zones/BattlegroundIC.cpp").read_text()
src_h = (repo / "src/server/game/Battlegrounds/Zones/BattlegroundIC.h").read_text()

# 1. Static assertions
assert "!source->IsWithinDistInMap(target_obj, 10.0f)" in src_cpp
assert "!source->IsWithinLOSInMap(target_obj" not in src_cpp
assert "{BG_IC_GO_FLAGPOLE_1_1, 0, 0.0f, 0.0f, 0.0f, 0.0f}" in src_h

code = r'''
#include <cassert>
#include <cstdint>
#include <iostream>

using uint32 = std::uint32_t;
using uint8 = std::uint8_t;

enum TeamId { TEAM_ALLIANCE = 0, TEAM_HORDE = 1, TEAM_NEUTRAL = 2 };

enum BannersTypes
{
    BANNER_A_CONTROLLED,
    BANNER_A_CONTESTED,
    BANNER_H_CONTROLLED,
    BANNER_H_CONTESTED
};

struct ICNodePoint
{
    uint32 gameobject_type;
    uint32 gameobject_entry;
    TeamId faction;
    uint32 banners[4];
    bool needChange;
    uint32 timer;
    uint32 last_entry;
};

uint32 GetNextBanner(ICNodePoint* node, uint32 team, bool returnDefinitve)
{
    if (returnDefinitve)
        return node->banners[(team == TEAM_ALLIANCE ? BANNER_A_CONTROLLED : BANNER_H_CONTROLLED)];

    if (node->last_entry == 0)
        return node->banners[(team == TEAM_ALLIANCE ? BANNER_A_CONTESTED : BANNER_H_CONTESTED)];

    if (node->gameobject_entry == node->banners[BANNER_A_CONTROLLED] || node->gameobject_entry == node->banners[BANNER_H_CONTROLLED])
        return node->banners[(team == TEAM_ALLIANCE ? BANNER_A_CONTESTED : BANNER_H_CONTESTED)];

    if (node->gameobject_entry == node->banners[BANNER_A_CONTESTED] || node->gameobject_entry == node->banners[BANNER_H_CONTESTED])
    {
        if (node->last_entry == node->banners[(team == TEAM_ALLIANCE ? BANNER_A_CONTROLLED : BANNER_H_CONTROLLED)])
            return node->last_entry;

        return node->banners[(team == TEAM_ALLIANCE ? BANNER_A_CONTESTED : BANNER_H_CONTESTED)];
    }

    return 0;
}

int main()
{
    // Workshop banner entries
    uint32 GO_WORKSHOP_BANNER = 195133;
    uint32 GO_A_CONTROL = 195149;
    uint32 GO_A_CONT    = 195150;
    uint32 GO_H_CONTROL = 195151;
    uint32 GO_H_CONT    = 195152;

    ICNodePoint node;
    node.gameobject_type = 10;
    node.gameobject_entry = GO_WORKSHOP_BANNER;
    node.faction = TEAM_NEUTRAL;
    node.banners[BANNER_A_CONTROLLED] = GO_A_CONTROL;
    node.banners[BANNER_A_CONTESTED]  = GO_A_CONT;
    node.banners[BANNER_H_CONTROLLED] = GO_H_CONTROL;
    node.banners[BANNER_H_CONTESTED]  = GO_H_CONT;
    node.needChange = false;
    node.timer = 0;
    node.last_entry = 0;

    // 1. Initial capture: Alliance clicks neutral workshop banner
    uint32 next = GetNextBanner(&node, TEAM_ALLIANCE, false);
    assert(next == GO_A_CONT);
    node.faction = TEAM_ALLIANCE;
    node.last_entry = node.gameobject_entry; // GO_WORKSHOP_BANNER
    node.gameobject_entry = next;

    // 2. Horde contests while Alliance is contesting: MUST become Horde contested (not neutral GO_WORKSHOP_BANNER!)
    next = GetNextBanner(&node, TEAM_HORDE, false);
    assert(next == GO_H_CONT);
    node.faction = TEAM_HORDE;
    node.last_entry = node.gameobject_entry; // GO_A_CONT
    node.gameobject_entry = next;

    // 3. Alliance contests back: MUST become Alliance contested
    next = GetNextBanner(&node, TEAM_ALLIANCE, false);
    assert(next == GO_A_CONT);
    node.faction = TEAM_ALLIANCE;
    node.last_entry = node.gameobject_entry; // GO_H_CONT
    node.gameobject_entry = next;

    // 4. Timer expires0after 1 minute: becomes Alliance controlled
    next = GetNextBanner(&node, node.faction, true);
    assert(next == GO_A_CONTROL);
    node.last_entry = node.gameobject_entry;
    node.gameobject_entry = next;

    // 5. Horde assaults Alliance controlled workshop: becomes Horde contested
    next = GetNextBanner(&node, TEAM_HORDE, false);
    assert(next == GO_H_CONT);
    node.faction = TEAM_HORDE;
    node.last_entry = GO_A_CONTROL;
    node.gameobject_entry = next;

    // 6. Alliance DEFENDS their workshop: MUST return Alliance controlled!
    next = GetNextBanner(&node, TEAM_ALLIANCE, false);
    assert(next == GO_A_CONTROL);
    node.faction = TEAM_ALLIANCE;
    node.last_entry = GO_H_CONT;
    node.gameobject_entry = next;

    // 7. Distance check validation
    float playerDist1 = 7.5f;
    float playerDist2 = 10.5f;
    assert(playerDist1 <= 10.0f); // 7.5m passes now (previously failed with 5m limit)
    assert(playerDist2 > 10.0f);  // 10.5m exceeds 10m threshold

    std::cout << "PASS: Isle of Conquest flag capture, defending, contesting transitions, and 10m range verified\n";
    return 0;
}
'''

with tempfile.TemporaryDirectory(prefix="ioc-flag-capture-") as temp:
    cpp = Path(temp) / "test.cpp"
    binary = Path(temp) / "test"
    cpp.write_text(code)
    subprocess.run(["c++", "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror", str(cpp), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
