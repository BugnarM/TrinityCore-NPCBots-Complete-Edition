// Standalone regression test, no running realm/database required:
// c++ -std=c++20 -O2 contrib/tests/wintergrasp_bot_war.cpp -o /tmp/wg-test && /tmp/wg-test
#include "../../src/server/scripts/Northrend/wintergrasp_bot_war.h"
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
            Check(counts[WorkshopWest] == 15 && counts[WorkshopEast] == 15, "workshop guards missing");
        }
        else
        {
            Check(counts[KeepCenter] + counts[KeepWest] + counts[KeepEast] == 75, "keep defense missing");
            Check(counts[South] == 15 && counts[SouthWest] == 15 && counts[SouthEast] == 15, "a south tower is uncovered");
        }
    }
    std::cout << "PASS: population bounds, human priority, squad cohesion and all tactical objectives\n";
}
