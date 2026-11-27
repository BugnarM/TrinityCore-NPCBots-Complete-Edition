// Standalone regression test, no running realm/database required:
// c++ -std=c++20 -O2 contrib/tests/wintergrasp_bot_war.cpp -o /tmp/wg-test && /tmp/wg-test
#include "../../src/server/scripts/Northrend/wintergrasp_bot_war.h"
#include "../../src/server/scripts/Northrend/wintergrasp_vehicle_navigation.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace WintergraspBotWar;

void Check(bool condition, char const* message)
{
    if (!condition)
    {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

int main()
{
    for (uint32_t credits=0; credits<=20; ++credits)
    {
        Check(HasVehicleBuildRank(credits,true), "attackers can build vehicles without rank");
        Check(HasVehicleBuildRank(credits,false), "defenders can build vehicles without rank");
    }
    Check(FactoryPhaseMask(true,false)==32&&FactoryPhaseMask(false,false)==16, "field factory phases differ from flag phase 1");
    Check(FactoryPhaseMask(true,true)==128&&FactoryPhaseMask(false,true)==64, "keep factory phases");
    Check(MaxConcurrentBuilders==2, "only two bots may fetch vehicles at once");
    Check(BotRespawnDelayMs == 30000, "Wintergrasp BotWar must keep its isolated 30s respawn timer");
    Check(WorkshopCaptureReserveSquads == 8, "workshop contest reserve updated");
    Check(Navigation::VerifiedPath(1, 2) && Navigation::VerifiedPath(4, 2), "valid navmesh paths accepted");
    Check(Navigation::VerifiedPath(1 | 0x80u, 2) && Navigation::VerifiedPath(4 | 0x80u, 2),
        "FARFROMPOLY_END navmesh prefixes accepted for vehicle ramps and bridges");
    Check(!Navigation::VerifiedPath(2, 2) && !Navigation::VerifiedPath(1 | 0x08u, 2) && !Navigation::VerifiedPath(1, 1),
        "shortcuts, incomplete-start paths and single points remain rejected");
    Check(Navigation::RecoveryDetour(0).Distance == 64.f && Navigation::RecoveryDetour(8).Distance == 24.f
        && Navigation::RecoveryDetour(16).Distance == 12.f, "recovery detour rings cover 64m, 24m and 12m");
    Check(BotBudget(120, 120, 0) == 120, "empty realm must fill to 120");
    Check(BotBudget(120, 120, 37) == 83, "humans must displace bots");
    Check(BotBudget(120, 120, 120) == 0, "full faction must have no bots");
    Check(BotBudget(120, 120, 121) == 0, "overfull faction must not underflow");
    Check(BotBudget(40, 120, 10) == 30, "smaller configured targets must be respected");
    Check(BotBudget(120, 100, 10) == 90, "legacy player limit must be respected");
    Check(BotBudget(0, 120, 0) == 0, "zero target must disable filling");
    Check(BotBudget(std::numeric_limits<uint32_t>::max(), 500, 0) == 120, "hard limit must remain 120");

    for (uint32_t target = 0; target <= 150; ++target)
        for (uint32_t capacity = 0; capacity <= 150; ++capacity)
            for (uint32_t humans = 0; humans <= 150; ++humans)
            {
                uint32_t bots = BotBudget(target, capacity, humans);
                Check(bots <= 120, "bot count exceeds hard limit");
                Check(humans >= target || bots + humans <= target, "target population exceeded");
                Check(humans >= capacity || bots + humans <= capacity, "capacity exceeded");
                Check(BotBudget(target, capacity, humans + 1) <= bots, "joining human increased bots");
            }

    for (bool attacker : { false, true })
    {
        std::array<uint32_t, LaneCount> counts{};
        for (uint32_t slot = 0; slot < 120; ++slot)
        {
            Lane lane = SquadLane(attacker, slot);
            Check(lane < LaneCount, "invalid route");
            Check(lane == SquadLane(attacker, slot - slot % SquadSize), "squad has conflicting orders");
            Check(attacker ? !IsCounterAttack(lane) : !IsAssault(lane), "faction assigned an enemy objective");
            ++counts[lane];
        }
        if (attacker)
        {
            Check(counts[Center] == 30 && counts[West] == 30 && counts[East] == 30, "assault lanes unbalanced");
            Check(counts[GuardSouth] == 10 && counts[GuardSouthWest] == 10 && counts[GuardSouthEast] == 10,
                "attacker south tower guards missing");
            Check(counts[WorkshopWest] == 0 && counts[WorkshopEast] == 0, "tower guards diverted to workshops");
        }
        else
        {
            Check(counts[KeepCenter] + counts[KeepWest] + counts[KeepEast] == 75, "keep defense missing");
            Check(counts[South] == 15 && counts[SouthWest] == 15 && counts[SouthEast] == 15, "a south tower is uncovered");
        }
    }
    for (Lane lane : {GuardSouth, GuardSouthWest, GuardSouthEast})
    {
        Check(IsTowerDefense(lane) && !IsAssault(lane) && !IsCounterAttack(lane), "tower duty is offensive");
        Check(GuardedTowerEntry(lane) >= 190356 && GuardedTowerEntry(lane) <= 190358, "wrong guarded tower");
        Check(ResolveTowerDefense(lane, false) == lane, "intact/unloaded tower abandoned");
        Check(IsAssault(ResolveTowerDefense(lane, true)), "destroyed tower guards fail to reinforce assault");
    }
    Check(GuardedTowerEntry(GuardSouth) == 190357 && ResolveTowerDefense(GuardSouth, true) == Center, "south tower assignment");
    Check(GuardedTowerEntry(GuardSouthWest) == 190356 && ResolveTowerDefense(GuardSouthWest, true) == West, "west tower assignment");
    Check(GuardedTowerEntry(GuardSouthEast) == 190358 && ResolveTowerDefense(GuardSouthEast, true) == East, "east tower assignment");
    for (uint32_t size = 30; size <= 120; ++size)
    {
        std::array<uint32_t, LaneCount> counts{};
        for (uint32_t slot = 0; slot < size; ++slot)
            ++counts[SquadLane(true, slot)];
        Check(counts[GuardSouth] && counts[GuardSouthWest] && counts[GuardSouthEast], "small roster leaves a tower bare");
    }
    for (uint32_t mask = 0; mask < 8; ++mask)
    {
        std::array<uint32_t, LaneCount> counts{};
        for (uint32_t slot = 0; slot < 120; ++slot)
        {
            Lane lane = SquadLane(true, slot);
            unsigned bit = lane == GuardSouth ? 0 : lane == GuardSouthWest ? 1 : 2;
            ++counts[ResolveTowerDefense(lane, IsTowerDefense(lane) && (mask & (1u << bit)))];
        }
        Check(counts[Center] == 30u + ((mask & 1u) ? 10u : 0u), "south reinforcements lost");
        Check(counts[West] == 30u + ((mask & 2u) ? 10u : 0u), "west reinforcements lost");
        Check(counts[East] == 30u + ((mask & 4u) ? 10u : 0u), "east reinforcements lost");
    }
    std::cout << "PASS: population bounds, human priority, squad cohesion and all tactical objectives\n";
}
