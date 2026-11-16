#!/usr/bin/env python3
"""Verify production IC objective selection, combat cast priority, vehicle suppression, and banner capture."""
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
ai = (repo / 'src/server/game/AI/NpcBots/bot_battlegrounds.cpp').read_text()

dispatch = ai[ai.index('    if (!finalAssault && !(sa && objective)'):ai.index('    // Carry a real bomb payload')]

def get_block(text, start_str):
    start = text.index(start_str)
    brace = text.index('{', start)
    depth = 1
    idx = brace + 1
    while depth > 0 and idx < len(text):
        if text[idx] == '{':
            depth += 1
        elif text[idx] == '}':
            depth -= 1
        idx += 1
    return text[start:idx]

nearby_block = get_block(ai, '            if (!objective && !me->GetVehicle())')

code = r'''
#include "src/server/game/AI/NpcBots/botbgtactics.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <map>
#include <vector>

using uint32 = std::uint32_t;
using uint8 = std::uint8_t;
using TeamId = unsigned;
using namespace NPCBots::SiegeBG;

enum {
    BATTLEGROUND_SA = 9,
    BATTLEGROUND_IC = 30,
    STATUS_IN_PROGRESS = 1,
    NODE_TYPE_REFINERY = 0,
    NODE_TYPE_QUARRY = 1,
    NODE_TYPE_DOCKS = 2,
    NODE_TYPE_HANGAR = 3,
    NODE_TYPE_WORKSHOP = 4,
    NODE_TYPE_GRAVEYARD_A = 5,
    NODE_TYPE_GRAVEYARD_H = 6,
    MAX_NODE_TYPES = 7,
    TEAM_ALLIANCE = 0,
    TEAM_HORDE = 1,
    TEAM_NEUTRAL = 2,
    GO_FLAG_NOT_SELECTABLE = 1,
    CURRENT_GENERIC_SPELL = 1,
    OPEN_FLAG_BG = 21651,
    SPELL_CAST_OK = 255,
    UNIT_FLAG_IMMUNE = 1,
    BOT_ROLE_HEAL = 1
};

float constexpr INTERACTION_DISTANCE = 5.f;

struct Guid {
    unsigned value;
    unsigned GetCounter() const { return value; }
    operator unsigned() const { return value; }
    bool operator==(Guid const& o) const = default;
};

struct Position {
    float x = 0, y = 0, z = 0;
    Position const& GetPosition() const { return *this; }
    float GetExactDist(Position const* o) const {
        return std::sqrt((x - o->x) * (x - o->x) + (y - o->y) * (y - o->y) + (z - o->z) * (z - o->z));
    }
};

struct Targets {
    Guid guid{0};
    Guid GetGOTargetGUID() const { return guid; }
};

struct Info { unsigned Id = 21651; };
struct Spell {
    Info info;
    Info* m_spellInfo = &info;
    Targets m_targets;
};

struct BattlegroundIC;
struct GameObject;

struct Unit : Position {
    BattlegroundIC* bg = nullptr;
    Guid guid{4};
    bool alive = true, combat = false, mounted = true, form = true, los = true, vehicle = false;
    unsigned phase = 1, casts = 0, attackStops = 0, movementStops = 0;
    GameObject* pending = nullptr;
    Spell* casting = nullptr;

    Guid GetGUID() const { return guid; }
    bool IsAlive() const { return alive; }
    bool IsInCombat() const { return combat; }
    bool GetVehicle() const { return vehicle; }
    Unit* GetVehicleBase() { return this; }
    bool GetTransport() const { return false; }
    bool InSamePhase(Unit const* o) const { return phase & o->phase; }
    float GetExactDist(Unit const* o) const {
        return std::sqrt((x - o->x) * (x - o->x) + (y - o->y) * (y - o->y) + (z - o->z) * (z - o->z));
    }
    bool IsWithinDistInMap(Unit const* o, float distance) const { return GetExactDist(o) <= distance; }
    bool IsWithinLOSInMap(Unit const*) const { return los; }
    Spell* GetCurrentSpell(unsigned) const { return casting; }
    void StopMoving() { ++movementStops; }
    void AttackStop() { ++attackStops; }
    unsigned CastSpell(GameObject* object, unsigned spell, bool triggered) {
        if (spell != OPEN_FLAG_BG || triggered) std::abort();
        ++casts;
        pending = object;
        return SPELL_CAST_OK;
    }
};

using Creature = Unit;

struct GameObject : Unit {
    unsigned entry = 0;
    bool spawned = true, disabled = false;
    bool isSpawned() const { return spawned; }
    bool HasFlag(unsigned) const { return disabled; }
    unsigned GetEntry() const { return entry; }
};

struct ICNodePoint {
    unsigned gameobject_type;
    unsigned gameobject_entry;
    TeamId faction;
    unsigned nodeType;
    unsigned banners[4];
    bool needChange;
    unsigned timer;
    unsigned last_entry;
    unsigned worldStates[5];
    unsigned nodeState;
};

struct BattlegroundIC {
    unsigned status = 1, type = BATTLEGROUND_IC;
    std::map<unsigned, GameObject> objects;
    Guid BgObjects[50];
    ICNodePoint nodePoint[MAX_NODE_TYPES];
    unsigned captures = 0;

    GameObject* GetBGObject(unsigned type, bool) {
        auto it = objects.find(type);
        return it != objects.end() ? &it->second : nullptr;
    }
    TeamId GetBotTeamId(Guid) const { return TEAM_ALLIANCE; }
};

namespace BotMgr {
    std::vector<Unit*> allies;
    auto const& GetAllGroupMembers(Unit*) { return allies; }
}

struct Controller {
    Creature* me;
    unsigned boards = 0;
    bool vehicleAvailable = true;

    BattlegroundIC* GetBG() { return me->bg; }
    bool TrySiegeVehicle(bool, unsigned) { ++boards; return vehicleAvailable; }
    void DismountBot() { me->mounted = false; }
    void removeShapeshiftForm() { me->form = false; }

    bool Act(BattlegroundIC* bg, GameObject* objective, bool finalAssault = false) {
        (void)bg;
        bool sa = false;
        bool defend = false;
        unsigned squad = 0;
        DISPATCH
        return false;
    }

    void ScanNearby(BattlegroundIC* ic, ICNodePoint const* nodes, TeamId team, GameObject*& objective, Position& destination) {
        auto ControlledBy = [](ICNodePoint const& np, TeamId t) {
            return np.faction == t;
        };
        (void)ControlledBy;
        NEARBY_SCAN;
    }
};

void Check(bool ok, char const* message) {
    if (!ok) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

int main() {
    BattlegroundIC bg;
    Creature bot;
    bot.bg = &bg;
    bot.guid = {4};
    Controller ai{&bot};

    GameObject banner;
    banner.guid = {101};
    banner.entry = 195133;
    banner.x = 10.f; banner.y = 10.f; banner.z = 0.f;
    bg.objects[4] = banner;
    bg.BgObjects[4] = banner.guid;

    // 1. Verify combat does not prevent flag cast in IC
    bot.x = 10.f; bot.y = 10.f; bot.z = 0.f;
    bot.combat = true;
    bot.mounted = true;
    bot.form = true;
    bool acted = ai.Act(&bg, &banner);
    Check(acted, "IC bot failed to act on objective while in combat");
    Check(bot.casts == 1, "IC bot did not cast OPEN_FLAG_BG");
    Check(ai.boards == 0, "IC bot boarded vehicle instead of capturing flag");
    Check(!bot.mounted, "IC bot did not dismount before casting");
    Check(!bot.form, "IC bot did not remove form before casting");
    Check(bot.attackStops == 1, "IC bot did not stop attack before casting");

    // 2. Verify TrySiegeVehicle is suppressed when objective is active
    bot.combat = false;
    ai.boards = 0;
    acted = ai.Act(&bg, &banner);
    Check(acted && ai.boards == 0, "IC bot boarded vehicle even though objective was present");

    // 3. Verify Nearby Scan logic
    for (uint8 i = 0; i < MAX_NODE_TYPES; ++i) {
        bg.nodePoint[i].gameobject_type = i;
        bg.nodePoint[i].faction = TEAM_NEUTRAL;
    }
    TeamId team = TEAM_ALLIANCE;
    GameObject* objective = nullptr;
    Position destination;
    ai.ScanNearby(&bg, bg.nodePoint, team, objective, destination);
    Check(objective == &bg.objects[4], "Nearby scan failed to pick uncaptured banner within 25 yards");

    std::cout << "PASS: IC objective capture in combat, vehicle suppression, and nearby banner scan verified\n";
    return 0;
}
'''

code = code.replace('DISPATCH', dispatch).replace('NEARBY_SCAN', nearby_block)

with tempfile.TemporaryDirectory(prefix='ioc-objectives-') as d:
    cpp = Path(d) / 'test.cpp'
    exe = Path(d) / 'test'
    cpp.write_text(code)
    subprocess.run(['c++', '-std=c++20', '-O2', '-Wall', '-Wextra', '-Wno-unused-variable', '-Wno-misleading-indentation', '-I', str(repo), str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
