// Run from the repository root:
// c++ -std=c++20 -O2 contrib/tests/siege_battlegrounds.cpp -o /tmp/siege-bg-test && /tmp/siege-bg-test
#include "../../src/server/game/AI/NpcBots/botbgtactics.h"
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <queue>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace NPCBots::SiegeBG;
void Check(bool value, char const* message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
struct Node { unsigned map, flags; float x; std::vector<unsigned> links; };
using Mesh = std::unordered_map<unsigned, Node>;

bool Reachable(Mesh const& mesh, unsigned start, unsigned target, unsigned mask, unsigned map, unsigned team = 0)
{
    std::unordered_set<unsigned> seen{ start };
    std::queue<unsigned> queue;
    queue.push(start);
    while (!queue.empty())
    {
        unsigned current = queue.front(); queue.pop();
        if (current == target) return true;
        for (unsigned next : mesh.at(current).links)
        {
            bool open = map == 607 ? SANodeAccessible(next, mask) && SAEdgeOpen(current, next, mask)
                : ICEdgeOpen(current, next, uint8_t(team), mask);
            if (open && seen.insert(next).second) queue.push(next);
        }
    }
    return false;
}

int main(int argc, char** argv)
{
    std::ifstream input(argc > 1 ? argv[1] : "sql/custom/world/2026_10_23_01_battleground_bots_sa_ic.sql");
    Check(input.good(), "cannot read the production waypoint migration");
    Mesh mesh;
    std::string line;
    while (std::getline(input, line))
    {
        if (line.empty() || line.front() != '(' || line[1] < '0' || line[1] > '9') continue;
        line = line.substr(0, line.find_last_of(')'));
        std::vector<std::string> fields;
        bool quoted = false;
        std::string field;
        for (char character : line.substr(1))
        {
            if (character == '\'') quoted = !quoted;
            if (character == ',' && !quoted) { fields.push_back(field); field.clear(); }
            else field += character;
        }
        fields.push_back(field);
        Check(fields.size() == 16, "waypoint SQL column count changed");
        unsigned id = unsigned(std::stoul(fields[0]));
        Node node{unsigned(std::stoul(fields[2])), unsigned(std::stoul(fields[10])), std::stof(fields[11]), {}};
        auto text = fields[15];
        text = text.substr(text.find('\'') + 1);
        text = text.substr(0, text.find('\''));
        std::istringstream links(text);
        while (links >> text) node.links.push_back(unsigned(std::stoul(text.substr(0, text.find(':')))));
        Check(mesh.emplace(id, node).second, "duplicate waypoint ID");
        Check(node.flags < (1u << 20), "invalid waypoint flag bit");
    }
    Check(mesh.size() == 50, "expected 27 SA and 23 IC nodes");
    for (auto const& [id, node] : mesh)
        for (unsigned next : node.links)
        {
            Check(mesh.contains(next), "dangling waypoint link");
            Check(mesh.at(next).map == node.map, "link crosses battleground maps");
            bool reverse = false;
            for (unsigned back : mesh.at(next).links) reverse |= back == id;
            Check(reverse, "missing reverse link");
        }
    std::array<std::pair<unsigned, unsigned>, 6> gateEdges{{{5205,5221},{5217,5225},{5207,5222},{5213,5224},{5211,5223},{5219,5226}}};
    for (unsigned gate = 0; gate < gateEdges.size(); ++gate)
    {
        auto [before, after] = gateEdges[gate];
        Check(mesh.at(before).x > mesh.at(after).x + 40.f, "SA gate checkpoints are on the wrong side or column");
        Check(SAGateDefenseNode(Gate(gate)) == after, "SA defenders form on the attacking side of the wall");
        Check(!SAEdgeOpen(before, after, 0), "intact SA gate permits crossing");
        Check(SAEdgeOpen(before, after, 1u << gate), "destroyed SA gate still blocks crossing");
    }
    for (unsigned mask = 0; mask < 64; ++mask)
    {
        for (bool west : {false, true})
        {
            Gate gate = AssaultGate(mask, west);
            Check(gate == NoGate ? RelicOpen(mask) : !Broken(mask, uint8_t(gate)), "invalid SA siege objective");
        }
        Check(Reachable(mesh, 5200, 5220, mask, 607) == RelicOpen(mask), "SA mesh bypasses an intact gate or strands the relic");
        for (unsigned team = 0; team < 2; ++team)
        {
            unsigned boss = team == 0 ? 5309 : 5304;
            bool breach = (mask & (team == 0 ? 7u : 56u)) != 0;
            Check(Reachable(mesh, 5310, boss, mask, 628, team) == breach, "IC path bypasses an enemy keep gate");
            bool ownBreach = (mask & (team == 0 ? 56u : 7u)) != 0;
            Check(Reachable(mesh, 5310, team == 0 ? 5304 : 5309, mask, 628, team) == ownBreach,
                "an intact friendly gate permits ordinary walking instead of using a portal");
        }
    }
    for (uint8_t team = 0; team < 2; ++team)
    {
        std::array<unsigned,5> counts{};
        for (unsigned id = 0; id < 40; ++id)
        {
            uint8_t squad = ICSquad(id);
            Check(squad == ICSquad(id - id % 5), "IC squad members have different orders");
            ++counts[ICPrimaryNode(squad, team)];
        }
        for (unsigned count : counts) Check(count >= 5, "IC does not cover all five strategic nodes");
    }
    for (unsigned id = 0; id < 40; ++id)
        Check(!SACaptureScout(id) || !SiegeEscortMember(id), "a capture runner was assigned to a convoy");
    Check(!SAReinforceOtherLane(1.f, 1.f), "an idle lane pulled the defender reserves");
    Check(!SAReinforceOtherLane(0.8f, 0.74f), "minor splash damage moved defender reserves");
    Check(SAReinforceOtherLane(0.95f, 0.55f), "a threatened gate did not receive reserve defenders");
    Check(SiegeGateHealthFraction(50, 200) == 0.25f && SiegeGateHealthFraction(0, 0) == 1.f,
        "gate pressure ignores current health or divides by zero");
    Check(!SANodeAccessible(999, 63), "unrelated custom waypoint bypasses SA stage checks");
    for (bool aboard : {false, true})
        for (bool freeBot : {false, true})
            Check(UseOwnerVehicleAI(freeBot, aboard) == (aboard && !freeBot), "autonomous bot reached owner-only vehicle AI/assert");
    Check(!SiegeControlBlocked(true, false, true), "boarding root hands siege driver back to class/owner vehicle AI");
    Check(SiegeControlBlocked(false, false, true), "on-foot crowd control ignored");
    Check(SiegeControlBlocked(true, true, true), "casting driver loses vehicle control ownership");
    for (unsigned type = 0; type < 256; ++type)
        for (unsigned count = 0; count < 5; ++count)
            Check(VerifiedSiegePath(type, count) == ((type == 1 || type == 4) && count >= 2), "valid prefix rejected or shortcut path accepted");
    struct GateCrossing { float x1,y1,x2,y2; };
    GateCrossing crossings[] = {
        {1140.9f,-762.61f,1160.9f,-762.61f}, {1218.74f,-861.15f,1218.74f,-841.15f},
        {1217.9f,-686.95f,1217.9f,-666.95f}, {403.48f,-833.95f,423.48f,-833.95f},
        {353.44f,-774.58f,353.44f,-754.58f}, {351.02f,-913.33f,351.02f,-893.33f}
    };
    for (unsigned mask = 0; mask < 64; ++mask)
        for (unsigned gate = 0; gate < 6; ++gate)
        {
            auto const& p = crossings[gate];
            Check(ICSegmentOpen(p.x1,p.y1,p.x2,p.y2,mask) == Broken(mask, gate), "physical path crosses intact keep gate");
            Check(ICSegmentOpen(p.x2,p.y2,p.x1,p.y1,mask) == Broken(mask, gate), "reverse path crosses intact keep gate");
        }
    Check(!ICSegmentOpen(403.48f,-800.f,423.48f,-800.f,63), "path crosses solid wall away from destroyed gate opening");
    Check(ICSegmentOpen(600.f,-820.f,776.f,-804.f,0), "open road blocked by unrelated closed gates");
    Check(ICPositionKeep(299.15f,-784.59f) == 0 && ICPositionKeep(1284.76f,-705.67f) == 1, "spawn not attached to actual courtyard");
    Check(ICPositionKeep(776.23f,-804.28f) == 2, "workshop mistaken for keep interior");
    Check(ICNodeKeep(5304) == 0 && ICNodeKeep(5309) == 1 && ICNodeKeep(5306) == 2, "gate checkpoint mistaken for breached interior");
    std::cout << "PASS: SQL graph, 64 gate states for both teams, 768 physical gate crossings, vehicle root/owner regression and strict path flags\n";
}
