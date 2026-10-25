// Standalone: g++ -std=c++20 -O2 contrib/tests/dungeon_finder_bots.cpp -o /tmp/lfg-bots-test
#include "../../src/server/game/AI/NpcBots/botdungeonfinder.h"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <random>
#include <set>
#include <stdexcept>

using namespace NPCBots::DungeonFinder;
namespace
{
void Check(bool condition, char const* message)
{
    if (!condition)
        throw std::runtime_error(message);
}

void TestQueuePolicy()
{
    Check(!CanFill(144, 100, 45, 1, true, true), "filled before the human-priority interval");
    Check(CanFill(145, 100, 45, 1, true, true), "failed at the configured deadline");
    Check(CanFill(100, 100, 0, 4, true, true), "zero-delay configuration failed");
    Check(!CanFill(99, 100, 0, 1, true, true), "backward clock allowed fill");
    Check(!CanFill(100, -1, 45, 1, true, true), "invalid join time allowed fill");
    Check(CanFill(std::numeric_limits<std::int64_t>::max(), 0, 3600, 1, true, true), "large timestamp overflow");
    for (std::size_t members = 0; members <= 6; ++members)
    {
        Check(CanFill(1000, 0, 45, members, true, true) == (members >= 1 && members <= 4), "invalid party size");
        Check(!CanFill(1000, 0, 45, members, false, true), "disabled feature filled a party");
        Check(!CanFill(1000, 0, 45, members, true, false), "invited or cancelled entry filled twice");
    }
    Check(NormalizeWait(-1) == 45 && NormalizeWait(0) == 0 && NormalizeWait(45) == 45 &&
        NormalizeWait(10000) == 3600, "wait-time normalization failed");
}

void TestComposition()
{
    Check(MissingRoles({Damage}) == RoleCounts{1, 1, 2}, "solo DPS complement");
    Check(MissingRoles({Tank}) == RoleCounts{0, 1, 3}, "solo tank complement");
    Check(MissingRoles({Healer}) == RoleCounts{1, 0, 3}, "solo healer complement");
    Check(MissingRoles({Tank, Healer}) == RoleCounts{0, 0, 3}, "premade complement");
    Check(MissingRoles({Tank | 1, Damage, Damage}) == RoleCounts{0, 1, 1}, "leader flag or mixed premade");
    Check(MissingRoles({Tank, Healer, Damage, Damage, Damage}) == RoleCounts{0, 0, 0}, "full-party complement");
    for (auto const& invalid : std::vector<std::vector<std::uint8_t>>{
        {}, {0}, {Tank, Tank}, {Healer, Healer}, {Damage, Damage, Damage, Damage},
        {Tank | Healer}, {Tank, Healer, Damage, Damage, Damage, Damage}})
        Check(!MissingRoles(invalid), "invalid assigned roles accepted");
}

bool BrutePlan(std::vector<std::uint8_t> const& masks, RoleCounts remaining, std::size_t candidate = 0)
{
    if (remaining == RoleCounts{}) return true;
    if (candidate == masks.size()) return false;
    if (BrutePlan(masks, remaining, candidate + 1)) return true;
    for (std::size_t role = 0; role < Roles.size(); ++role)
        if (remaining[role] && (masks[candidate] & Roles[role]))
        {
            --remaining[role];
            if (BrutePlan(masks, remaining, candidate + 1)) return true;
            ++remaining[role];
        }
    return false;
}

void ValidatePlan(std::vector<std::uint8_t> const& masks, RoleCounts const& needed)
{
    auto plan = PlanBots(masks, needed);
    Check(bool(plan) == BrutePlan(masks, needed), "matching disagrees with exhaustive assignment");
    if (!plan) return;
    RoleCounts actual{};
    std::set<std::size_t> used;
    for (auto const& slot : *plan)
    {
        Check(slot.candidate < masks.size() && used.insert(slot.candidate).second, "template reused in a plan");
        Check((masks[slot.candidate] & slot.role) != 0, "unsupported class role assigned");
        auto role = std::find(Roles.begin(), Roles.end(), slot.role);
        Check(role != Roles.end(), "non-singular bot role");
        ++actual[std::size_t(role - Roles.begin())];
    }
    Check(actual == needed, "partial plan accepted");
}

void TestTemplatePlanning()
{
    ValidatePlan({Tank | Healer, Tank}, {1, 1, 0}); // Greedy tank-first allocation fails here.
    ValidatePlan({Tank | Healer | Damage, Tank, Healer, Damage, Damage}, {1, 1, 3});
    Check(!PlanBots({Tank, Damage, Damage, Damage}, {0, 1, 2}), "missing healer accepted");
    Check(!PlanBots({Tank, Healer, Damage}, {0, 0, 3}), "partial generation accepted");
    Check(!PlanBots({Tank, Tank}, {2, 0, 0}), "invalid role capacity accepted");
    Check(PlanBots({}, {0, 0, 0})->empty(), "full parties need no templates");
    std::mt19937 random(7112026);
    for (unsigned test = 0; test < 12000; ++test)
    {
        std::vector<std::uint8_t> masks(random() % 8);
        for (auto& mask : masks) mask = std::uint8_t((random() % 8) << 1);
        RoleCounts needed{std::uint8_t(random() % 2), std::uint8_t(random() % 2), std::uint8_t(random() % 4)};
        ValidatePlan(masks, needed);
    }
}

void TestPlaceholderIds()
{
    Check(NextPlaceholderCounter(0) == 1 && NextPlaceholderCounter(0xFFFFFE) == 0xFFFFFF &&
        NextPlaceholderCounter(0xFFFFFF) == 1, "invalid creature GUID counter or wrap");
    std::set<std::uint32_t> all;
    std::uint32_t counter = 0xFFFFF0;
    for (unsigned proposal = 0; proposal < 1000; ++proposal)
        for (unsigned bot = 0; bot < 4; ++bot)
        {
            counter = NextPlaceholderCounter(counter);
            Check(counter && counter <= 0xFFFFFF && all.insert(counter).second, "parallel proposals share placeholder IDs");
        }
}
}
int main()
{
    try
    {
        TestQueuePolicy();
        TestComposition();
        TestTemplatePlanning();
        TestPlaceholderIds();
        std::cout << "PASS: LFG deadlines/queue eligibility, role complements, unique proposal IDs, 12000 exhaustive matching comparisons\n";
    }
    catch (std::exception const& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
