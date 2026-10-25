// g++ -std=c++20 -O2 -Wall -Wextra -Werror contrib/tests/raid_finder_test.cpp -o /tmp/raid-finder-test
#include "../../src/server/game/AI/NpcBots/botraidfinder.h"
#include <iostream>
#include <random>
#include <set>
#include <stdexcept>
#include <numeric>
using namespace NPCBots::RaidFinder;
static void Require(bool ok, char const* what) { if (!ok) throw std::runtime_error(what); }
static bool Oracle(std::vector<uint8_t> const& masks, RoleCounts cap, size_t index = 0)
{
    if (index == masks.size()) return true;
    for (size_t role = 0; role < Roles.size(); ++role)
        if (cap[role] && (masks[index] & Roles[role]))
        {
            --cap[role];
            if (Oracle(masks, cap, index + 1)) return true;
            ++cap[role];
        }
    return false;
}
int main()
{
    try
    {
        Require(GetRaids().size() == 9, "Nine supported raids");
        std::set<uint32_t> maps;
        for (auto const& raid : GetRaids())
        {
            Require(maps.insert(raid.mapId).second && raid.minLevel == 80 && raid.minItemLevel > 0, "Valid unique definitions");
            Require(FindRaid(raid.key) == &raid && FindRaidById(raid.id) == &raid && FindRaidByMap(raid.mapId) == &raid, "Definition lookup");
        }
        Require(!FindRaid("") && !FindRaid("the") && !FindRaid("no_such_raid"), "Reject empty/ambiguous names");
        Require(FindRaid("toc") == FindRaid("totc") && FindRaid("ony") == FindRaid("onyxia"), "Documented aliases work");
        Require(FindRaid("ICECROWN")->mapId == 631, "Case insensitive unambiguous lookup");
        Require(!MissingRoles({Tank, Tank, Tank}, 10), "Surplus tanks must not become DPS");
        Require(!MissingRoles({uint8_t(Tank | Healer)}, 10) && !MissingRoles({0}, 25), "Assigned roles must be singular");
        Require(!MissingRoles({}, 15) && !MissingRoles(std::vector<uint8_t>(26, Damage), 25), "Reject invalid size/overfill");
        Require(*MissingRoles({Tank, Healer, Damage, Damage, Damage}, 10) == RoleCounts{1, 2, 2}, "Exact missing roles");
        Require(!Ready(129, 100, 30, 1, 25) && Ready(130, 100, 30, 1, 25), "Bot fill deadline");
        Require(Ready(100, 100, 30, 25, 25) && !Ready(99, 100, 30, 1, 25), "Full human team proceeds; backwards clock does not");
        Require(!Ready(200, 100, 30, 26, 25) && !Ready(200, 100, 30, 0, 10), "No oversized or empty raid");

        std::mt19937 rng(20261025);
        for (unsigned trial = 0; trial < 12000; ++trial)
        {
            uint8_t size = trial % 2 ? 10 : 25;
            std::vector<uint8_t> masks;
            for (unsigned i = 0, n = rng() % 12; i < n; ++i) masks.push_back(uint8_t((1 + rng() % 7) << 1));
            auto plan = AssignRoles(masks, size);
            Require(bool(plan) == Oracle(masks, GetCapacity(size)), "Role matching agrees with independent exhaustive oracle");
            if (plan)
            {
                Require(plan->size() == masks.size() && bool(MissingRoles(*plan, size)), "Assignment fits exact capacity");
                for (size_t i = 0; i < plan->size(); ++i) Require((masks[i] & (*plan)[i]) != 0, "No invented player role");
                auto missing = *MissingRoles(*plan, size);
                std::vector<uint8_t> bots(30, Tank | Healer | Damage);
                auto botPlan = PlanRaidBots(bots, missing);
                Require(botPlan && botPlan->size() + masks.size() == size, "Complete complementary bot team");
            }
        }
        Require(!PlanRaidBots({Damage, Damage}, {1, 0, 1}), "Insufficient tank pool fails atomically");
        std::vector<QueueUnit> queue{{0, {Tank, Tank}}, {0, {Damage, Damage, Damage, Damage}},
            {0, {Damage, Damage, Damage}}, {0, {Healer, Healer, Healer}}, {0, {Damage, Damage}}, {1, {Healer}}};
        auto batch = SelectBatch(queue, 0, 10);
        Require(batch && batch->roles.size() == 10, "Maximize humans; greedy 4-DPS premade would strand a role");
        Require(std::find(batch->units.begin(), batch->units.end(), 1) == batch->units.end(), "Skip incompatible indivisible premade");
        Require(std::find(batch->units.begin(), batch->units.end(), 5) == batch->units.end(), "Never merge enemy factions");
        Require(MissingRoles(batch->roles, 10) == std::optional<RoleCounts>{RoleCounts{}}, "Full human raid needs no bots");
        std::vector<QueueUnit> realFirst{{0, {Tank}, 1}, {0, {Healer, Healer, Healer, Damage, Damage, Damage, Damage, Damage}, 1}};
        realFirst.push_back({0, {Tank}, 1});
        for (unsigned i = 0; i < 3; ++i) realFirst.push_back({0, {Healer}, 1});
        for (unsigned i = 0; i < 5; ++i) realFirst.push_back({0, {Damage}, 1});
        auto realBatch = SelectBatch(realFirst, 0, 10);
        Require(realBatch && realBatch->humans == 10 && realBatch->roles.size() == 10, "Prefer humans over another premade's existing bots");
        Require(!SelectBatch(queue, queue.size(), 25), "Reject invalid queue cursor");
        std::cout << "PASS: raid definitions/aliases, exact capacities, deadlines, 12000 exhaustive role comparisons, premade/faction/human-first matchmaking\n";
    }
    catch (std::exception const& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
