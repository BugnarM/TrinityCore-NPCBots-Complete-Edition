// g++ -std=c++20 -O2 -Wall -Wextra -Werror contrib/tests/dynamic_zone_test.cpp -o /tmp/dynamic-zone-test
// Exercises production policy, rather than a second simulated implementation.
#include "../../src/server/game/AI/NpcBots/botdynamiczonepolicy.h"
#include <climits>
#include <iostream>
#include <stdexcept>

using namespace NPCBots::DynamicZone;

static void Require(bool condition, char const* message)
{
    if (!condition) throw std::runtime_error(message);
}

int main()
{
    try
    {
        Require(ZoneKey(0, 12) != ZoneKey(1, 12), "Same zone on different maps must not share population");
        Require(ZoneKey(0, 12) != ZoneKey(0, 40), "Zones must remain distinct");
        Require(ZoneKey(UINT_MAX, UINT_MAX) == UINT64_MAX, "Full width key");
        Require(InScope(1, 40, 0, 0), "Manual wanderers retain unrestricted movement");
        Require(InScope(0, 40, 0, 40), "Dynamic bot can move inside home zone on map zero");
        Require(!InScope(1, 40, 0, 40), "Dynamic bot cannot change continent");
        Require(!InScope(0, 33, 0, 40), "Dynamic bot cannot move to another zone");

        auto const westfall = SpawnLevels(10, 20, 1);
        Require(westfall && westfall->min == 10 && westfall->max == 20, "Use node level, not visiting player's level");
        Require(!SpawnLevels(1, 10, 55), "Death knight cannot appear in starter zone");
        auto const dk = SpawnLevels(50, 60, 55);
        Require(dk && dk->min == 55 && dk->max == 60, "Class minimum intersects node range");
        Require(!SpawnLevels(40, 20, 1), "Reject inverted range");
        Require(!SpawnLevels(81, 90, 1), "Reject range above expansion cap");
        Require(!SpawnLevels(1, 0, 1), "Reject empty range");
        Require(!SpawnLevels(1, 10, 1, 0), "Reject empty cap");
        Require(SpawnLevels(0, 255, 1)->max == 80, "Clamp malformed upper bound");
        std::size_t ranges = 0;
        for (unsigned min = 1; min <= 80; ++min)
            for (unsigned max = min; max <= 80; ++max)
                for (unsigned classMin : {1u, 20u, 30u, 40u, 55u, 60u})
                {
                    auto const range = SpawnLevels(min, max, classMin);
                    Require(bool(range) == (classMin <= max), "Class eligibility must equal intersection existence");
                    if (range)
                        for (unsigned level = range->min; level <= range->max; ++level)
                            Require(level >= min && level <= max && level >= classMin && level <= 80,
                                "Every selectable spawn level must satisfy both constraints");
                    ++ranges;
                }

        Require(Normalize(-1, 0, 100) == 0, "Negative count cannot wrap to billions");
        Require(Normalize(INT_MIN, 1, 300) == 1, "Zero/negative interval must not busy-loop");
        Require(Normalize(INT_MAX, 1, 300) == 300, "Interval multiplication cannot overflow");
        Require(Normalize(8, 0, 100) == 8, "Default count preserved");
        Require(ZoneTarget(0, 0, 8, 200) == 0, "Empty world has no allocation");
        Require(ZoneTarget(1, 1, 8, 200) == 0, "Out-of-range index");
        std::size_t budgets = 0;
        for (unsigned zones = 1; zones <= 150; ++zones)
            for (unsigned cap = 0; cap <= 240; ++cap)
                for (unsigned perZone : {0u, 1u, 8u, 100u})
                {
                    unsigned total = 0, smallest = UINT_MAX, largest = 0;
                    for (unsigned i = 0; i < zones; ++i)
                    {
                        unsigned target = ZoneTarget(i, zones, perZone, cap);
                        Require(target <= perZone, "Per-zone target respected");
                        total += target;
                        smallest = std::min(smallest, target);
                        largest = std::max(largest, target);
                    }
                    Require(total == std::min(zones * perZone, cap), "Global budget filled without oversubscription");
                    Require(largest - smallest <= 1, "Even distribution across active zones");
                    ++budgets;
                }
        std::cout << "PASS: production dynamic zone policy: " << ranges << " level intersections, "
                  << budgets << " population budgets, map/zone scope and configuration bounds.\n";
    }
    catch (std::exception const& e)
    {
        std::cerr << "FAIL: " << e.what() << '\n';
        return 1;
    }
}
