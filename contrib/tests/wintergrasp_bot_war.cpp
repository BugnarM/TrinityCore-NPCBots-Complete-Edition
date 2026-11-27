// Standalone regression test, no running realm/database required:
// c++ -std=c++20 -O2 contrib/tests/wintergrasp_bot_war.cpp -o /tmp/wg-test && /tmp/wg-test
#include "../../src/server/scripts/Northrend/wintergrasp_bot_war.h"
#include "../../src/server/scripts/Northrend/wintergrasp_tactics.h"
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
            Check(counts[Center] == 35 && counts[West] == 35 && counts[East] == 35, "assault lanes unbalanced");
            Check(counts[GuardSouth] == 5 && counts[GuardSouthWest] == 5 && counts[GuardSouthEast] == 5,
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
    Check(VehicleLaneForWorkshop(true, Center, true, false, false, false) == West, "attacker NW/SW vehicle pushes West");
    Check(VehicleLaneForWorkshop(true, Center, false, true, false, false) == East, "attacker NE/SE vehicle pushes East");
    Check(VehicleLaneForWorkshop(false, KeepWest, true, false, false, false) == SouthWest, "defender must never take attacker West assault lane");
    Check(VehicleLaneForWorkshop(false, KeepEast, false, true, false, false) == SouthEast, "defender must never take attacker East assault lane");
    Check(VehicleLaneForWorkshop(false, South, false, false, true, false) == KeepWest, "keep west workshop assigns KeepWest");
    Check(BotWarVehicleMaxHealth(35000, 0) == 105000 && BotWarVehicleMaxHealth(55000, 0) == 165000
        && BotWarVehicleMaxHealth(85000, 0) == 255000 && BotWarVehicleMaxHealth(120000, 2) == 120000,
        "BotWar siege vehicle durability scaled cleanly");
    Check(ConcurrentBuilderLimit(0, false) == 2 && ConcurrentBuilderLimit(2, false) == 2
        && ConcurrentBuilderLimit(3, false) == 3 && ConcurrentBuilderLimit(6, false) == 3
        && ConcurrentBuilderLimit(2, true) == 3 && ConcurrentBuilderLimit(4, true) == 4,
        "builder limit scales appropriately for defenders and attackers");
    Check(ShouldHoldSquadStaging(1000, 9000, 2, false, false), "small squad waits briefly to rally");
    Check(!ShouldHoldSquadStaging(1000, 9000, 3, false, false), "3+ squadmates release staging hold");
    Check(!ShouldHoldSquadStaging(9000, 9000, 1, false, false), "expired staging window releases squad");
    Check(!ShouldHoldSquadStaging(1000, 9000, 1, true, false), "combat breaks staging hold");
    Check(CanMobilizeKeepDefender(false, 5392.f) && !CanMobilizeKeepDefender(true, 5392.f)
        && CanMobilizeKeepDefender(true, 4950.f),
        "keep defenders can sortie via portals while attackers never retreat through intact walls");
    Check(NextVehicle(true, 0, 1, 1, true) == VehicleRole::Demolisher
        && NextVehicle(true, 0, 2, 2, true) == VehicleRole::Demolisher
        && NextVehicle(true, 0, 3, 2, true) == VehicleRole::Catapult,
        "attacker prioritizes 5 wall-breakers per catapult while walls stand");
    Check(NextVehicle(false, 0, 1, 1, true) == VehicleRole::Catapult,
        "defender mixes anti-vehicle catapults early");
    Check(BreachScore(true, 0, 10) == 0 && BreachScore(false, 40, 2) < BreachScore(false, 100, 2),
        "breach scoring favors open breaches and already-damaged gates");
    Check(TargetPriority(15.f, 80.f, false, false, false, true, false, false)
        > TargetPriority(15.f, 80.f, false, false, false, false, false, false),
        "siege threats receive top targeting priority");
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
        Check(counts[Center] == 35u + ((mask & 1u) ? 5u : 0u), "south reinforcements lost");
        Check(counts[West] == 35u + ((mask & 2u) ? 5u : 0u), "west reinforcements lost");
        Check(counts[East] == 35u + ((mask & 4u) ? 5u : 0u), "east reinforcements lost");
    }
    std::cout << "PASS: population bounds, human priority, squad cohesion and all tactical objectives\n";
}
