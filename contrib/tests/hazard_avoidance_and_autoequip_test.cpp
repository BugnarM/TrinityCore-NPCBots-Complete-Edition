// g++ -std=c++20 -O2 -Wall -Wextra -pedantic contrib/tests/hazard_avoidance_and_autoequip_test.cpp -o /tmp/hazard-autoequip-test
#include "../../src/server/game/AI/NpcBots/bothazard.h"
#include "../../src/server/game/AI/NpcBots/botautoequip.h"
#include <iostream>
#include <random>
#include <set>
#include <stdexcept>

static void Require(bool condition, char const* message)
{
    if (!condition) throw std::runtime_error(message);
}

static void TestHazards()
{
    using namespace NPCBots::Hazard;
    std::vector<Circle> small{{{0, 0}, 3}};
    auto candidates = EscapeCandidates({0, 0}, 0, small);
    Require(!candidates.empty(), "Centered hazard needs an exit");
    Require(std::abs(Distance({0, 0}, candidates.front()) - 5) < .01f, "Normal escape must use a five metre step");
    Require(candidates.front().y > 4.9f, "Equal length exits prefer a lateral step");
    Require(EscapeCandidates({10, 0}, 0, small).empty(), "Safe bots must not evade");
    Require(EscapeCandidates({0, 0}, 0, {{{0, 0}, 100}}).empty(), "No unbounded/unsafe fallback for huge pools");
    Require(!SafeSegment({-5, 0}, {5, 0}, small), "Cannot cross an intervening pool");
    Require(!SafeSegment({2, 0}, {-5, 0}, small), "Cannot move deeper through own pool");
    Require(SafeSegment({2, 0}, {5, 0}, small), "Outward escape is valid");
    std::vector<Circle> overlapping{{{0, 0}, 7}, {{6, 0}, 5}, {{-6, 0}, 5}};
    candidates = EscapeCandidates({0, 0}, 0, overlapping);
    Require(!candidates.empty(), "Overlapping pools need a joint safe exit");
    for (Point p : candidates)
        Require(!Unsafe(p, overlapping) && SafeSegment({0, 0}, p, overlapping), "All pools must be checked");

    std::mt19937 rng(9012026);
    std::uniform_real_distribution<float> coord(-25, 25), radius(1, 20), angle(-3.14f, 3.14f);
    unsigned checked = 0;
    for (unsigned trial = 0; trial < 8000; ++trial)
    {
        Point start{coord(rng), coord(rng)};
        std::vector<Circle> circles{{start, radius(rng)}};
        for (unsigned j = 0; j < trial % 7; ++j)
            circles.push_back({{coord(rng), coord(rng)}, radius(rng)});
        for (Point end : EscapeCandidates(start, angle(rng), circles))
        {
            Require(std::isfinite(end.x) && std::isfinite(end.y), "Finite destinations");
            Require(Distance(start, end) >= 4.99f && Distance(start, end) <= 40.01f, "Bounded escape distance");
            // Independent dense sampling validates every segment, including a
            // forbidden entry/exit through a second pool with safe endpoints.
            for (Circle const& c : circles)
            {
                bool exited = !Contains(c, start);
                float previous = Distance(start, c.center);
                for (unsigned step = 1; step <= 160; ++step)
                {
                    float t = float(step) / 160;
                    Point p{start.x + t * (end.x - start.x), start.y + t * (end.y - start.y)};
                    float dist = Distance(p, c.center);
                    if (exited) Require(dist + .001f >= c.radius, "No entry into another/re-entered pool");
                    else Require(dist + .002f >= previous, "Penetration must not increase during escape");
                    exited = exited || dist >= c.radius;
                    previous = dist;
                }
                Require(Distance(end, c.center) >= c.radius, "Endpoint clears every pool");
            }
            ++checked;
        }
    }
    Require(checked > 10000, "Randomized geometry coverage");
    std::cout << "PASS: " << checked << " escape paths across 8000 randomized hazard layouts\n";
}

static void TestEquipment()
{
    using namespace NPCBots::AutoEquip;
    for (char const* invalid : {"0", "-1", "+200", "285", "4294967296", "200junk", "t11", "relentless", "200 201", " 200", "200 "})
        Require(!Parse(invalid, 80), "Reject invalid preset without changing equipment");
    for (unsigned level = 1; level <= 284; ++level)
    {
        auto p = Parse(std::to_string(level), 80);
        Require(p && p->maximum == level && p->minimum <= p->maximum, "Exact numeric cap");
    }
    Require(Parse("T10", 80)->maximum == 277, "Case insensitive tier");
    Require(Parse("blue", 80)->kind == Kind::Blue, "Dungeon blue");
    Require(Parse("", 80)->kind == Kind::Blue && Parse("auto", 40)->kind == Kind::Level, "Level appropriate defaults");
    Require(Parse("wrathful", 80)->IsPvP() && Parse("pvp", 80)->maximum == 284, "Wrathful includes elite weapons");
    Require(MatchesSet(*Parse("t7", 80), 787) && !MatchesSet(*Parse("t7", 80), 765), "No PvP set in T7");
    Require(MatchesSet(*Parse("t8", 80), 838) && !MatchesSet(*Parse("t8", 80), 843), "T8 set identities");
    Require(MatchesSet(*Parse("t9", 80), 880) && !MatchesSet(*Parse("t9", 80), 881), "T9 excludes trinket sets");
    Require(MatchesSet(*Parse("t10", 80), 901) && !MatchesSet(*Parse("t10", 80), 902), "T10 set identities");

    Require(FitsClassSet(ClassSetProfile(5, 14, false), 885) && !FitsClassSet(ClassSetProfile(5, 14, false), 886), "Priest healer set bonuses differ from shadow");
    Require(FitsClassSet(ClassSetProfile(7, 21, false), 892) && !FitsClassSet(ClassSetProfile(7, 21, false), 893), "Shaman restoration differs from elemental");
    Require(FitsClassSet(ClassSetProfile(11, 30, false), 887) && !FitsClassSet(ClassSetProfile(11, 30, false), 888), "Druid restoration differs from balance");
    for (size_t row = 0; row < ClassSets.size(); ++row)
        for (uint32_t set : ClassSets[row]) Require(FitsClassSet(SetProfile(row), set), "Every class/spec tier and PvP identity");
    Require(!FitsClassSet(SetProfile::None, 883) && FitsClassSet(SetProfile::None, 0), "Custom classes do not receive another class's tier bonuses");
    auto preset = *Parse("t10", 80);
    Candidate old{1, 200, 0, 0, 0, 9999};
    Candidate good{2, 264, 0, 0, 0, 100};
    Candidate over{3, 284, 0, 0, 0, 10000};
    Require(Select({old, good, over}, preset, {}).id == 2, "Prioritize target band and enforce cap");
    Candidate unique{4, 264, 0, 0, 0, 1000, true};
    Require(!Select({unique}, preset, {unique}).id, "No duplicate unique-equipped item");
    Candidate group1{5, 264, 0, 22, 1, 1000}, group2{6, 277, 0, 22, 1, 2000};
    Require(!Select({group2}, preset, {group1}).id, "No duplicate unique category (normal/heroic ring)");
    group2.categoryLimit = 2;
    Require(Select({group2}, preset, {group1}).id == 6, "Honor category quantity greater than one");
    Candidate nonunique{7, 264, 0, 0, 0, 100};
    Require(Select({nonunique}, preset, {nonunique}).id == 7, "Nonunique weapon pairs remain legal");

    std::array<std::vector<Candidate>, 5> slots;
    std::array<uint8_t, 5> tierSlots{0, 1, 2, 3, 4};
    for (unsigned i = 0; i < 5; ++i)
    {
        slots[i].push_back({100 + i, 264, 883, 0, 0, 100});
        slots[i].push_back({200 + i, 277, 884, 0, 0, 150});
        if (i != 4) slots[i].push_back({300 + i, 277, 885, 0, 0, 9999});
    }
    Require(SelectSet(slots, tierSlots, preset) == 884, "Choose one complete coherent five-piece set");
    slots.back().clear();
    Require(SelectSet(slots, tierSlots, preset) == 0, "Incomplete set fails before replacing items");
    std::cout << "PASS: strict preset parser, tier identity, cap/fallback, unique items/categories and complete sets\n";
}


static void TestOrganicEquipment()
{
    using namespace NPCBots::AutoEquip;
    auto preset = *Parse("organic t10", 80);
    std::array<std::vector<Candidate>, 18> candidates;
    std::array<uint8_t, 5> tierSlots{3, 5, 7, 12, 9};
    for (uint8_t slot = 0; slot < 18; ++slot)
    {
        if (slot == BodySlot) continue;
        bool tier = std::find(tierSlots.begin(), tierSlots.end(), slot) != tierSlots.end();
        if (tier) candidates[slot].push_back({1000u + slot, 264, 884, 0, 0, 150});
        candidates[slot].push_back({2000u + slot, 264, 0, 0, 0, 140});
        candidates[slot].push_back({3000u + slot, 251, 0, 0, 0, 120});
        candidates[slot].push_back({4000u + slot, 245, 0, 0, 0, 110});
        if (OrganicPvPSlot(slot)) candidates[slot].push_back({5000u + slot, 264, 0, 0, 0, 100, false, true});
    }
    std::array<unsigned, 6> pieceCounts{};
    unsigned pvp = 0, secondarySlots = 0;
    std::set<std::vector<uint32_t>> distinct;
    for (uint32_t identity = 1; identity <= 10000; ++identity)
    {
        uint32_t seed = OrganicSeed(identity, 7, 8);
        auto plan = SelectOrganicPlan(candidates, tierSlots, preset, seed);
        std::vector<uint32_t> ids;
        unsigned pieces = 0;
        for (uint8_t slot = 0; slot < 18; ++slot)
        {
            if (slot == BodySlot) { Require(!plan[slot].id, "Shirt is preserved"); continue; }
            Require(plan[slot].id != 0 && plan[slot].level <= preset.maximum, "All real equipment slots including head populated");
            pieces += plan[slot].set == 884;
            if (OrganicPvPSlot(slot)) { ++secondarySlots; pvp += plan[slot].pvp; }
            else Require(!plan[slot].pvp, "PvP appears only on intended secondary slots");
            ids.push_back(plan[slot].id);
        }
        Require(pieces >= 2 && pieces <= 5, "Exact selected bonus count without accidental extra set pieces");
        ++pieceCounts[pieces];
        distinct.insert(ids);
        // Unordered catalog iteration must not change the same bot's identity.
        auto shuffled = candidates;
        for (auto& pool : shuffled) std::reverse(pool.begin(), pool.end());
        auto repeat = SelectOrganicPlan(shuffled, tierSlots, preset, seed);
        for (size_t slot = 0; slot < plan.size(); ++slot) Require(plan[slot].id == repeat[slot].id, "Stable across catalog order/restart");
    }
    Require(pieceCounts[2] > 3100 && pieceCounts[2] < 3900 && pieceCounts[4] > 3100 && pieceCounts[4] < 3900, "35 percent 2p/4p");
    Require(pieceCounts[3] > 1200 && pieceCounts[3] < 1800 && pieceCounts[5] > 1200 && pieceCounts[5] < 1800, "15 percent 3p/5p");
    Require(pvp > secondarySlots * .08 && pvp < secondarySlots * .12 && distinct.size() > 100, "Ten percent PvP and varied plans");
    auto uniquePool = candidates;
    uniquePool[13] = {{8000, 264, 0, 10, 1, 150, true}, {8001, 264, 0, 11, 1, 140, true}};
    uniquePool[14] = uniquePool[13];
    for (uint32_t seed = 1; seed <= 100; ++seed)
    {
        auto ringPlan = SelectOrganicPlan(uniquePool, tierSlots, preset, seed);
        Require(ringPlan[13].id != ringPlan[14].id && ringPlan[13].uniqueCategory != ringPlan[14].uniqueCategory, "Organic plans enforce paired uniqueness");
    }
    for (auto& item : candidates[MainHandSlot]) item.blocksOffhand = true;
    auto twoHand = SelectOrganicPlan(candidates, tierSlots, preset, 1);
    Require(twoHand[MainHandSlot].id && !twoHand[OffHandSlot].id, "Two-hand weapons clear offhand in the planning phase");
    std::cout << "PASS: 10000 organic identities, exact 35/35/15/15 bonuses, 10 percent PvP, stable order, head/shirt and two-hand constraints\n";
}

int main()
{
    try { TestHazards(); TestEquipment();
        TestOrganicEquipment(); }
    catch (std::exception const& e) { std::cerr << "FAIL: " << e.what() << '\n'; return 1; }
}
