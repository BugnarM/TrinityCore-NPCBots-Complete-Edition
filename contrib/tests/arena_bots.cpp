// Standalone regression tests; run from any directory, optionally passing the mesh SQL path.
// g++ -std=c++20 -O2 contrib/tests/arena_bots.cpp -o /tmp/arena-test
#include "../../src/server/game/AI/NpcBots/botarena.h"
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
constexpr unsigned Spawn = 1, Alliance = 2, Horde = 4, Cover = 0x100000;
struct Node
{
    unsigned id, map, flags;
    float x, y, z;
    std::vector<unsigned> links;
};
using Graph = std::map<unsigned, Node>;

void Require(bool condition, std::string const& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

Graph ReadMesh(std::filesystem::path const& path)
{
    std::ifstream input(path);
    Require(input.is_open(), "Cannot open mesh: " + path.string());
    Graph graph;
    std::string line;
    while (std::getline(input, line))
    {
        if (line.size() < 2 || line.front() != '(' || line[1] < '0' || line[1] > '9')
            continue;
        // The migration uses quoted names and links, with no embedded commas.
        std::vector<std::string> fields;
        std::istringstream row(line.substr(1));
        std::string field;
        while (std::getline(row, field, ','))
            fields.push_back(field);
        Require(fields.size() == 16, "Invalid mesh row: " + line);
        Node node{unsigned(std::stoul(fields[0])), unsigned(std::stoul(fields[2])),
            unsigned(std::stoul(fields[10])), std::stof(fields[11]), std::stof(fields[12]), std::stof(fields[13]), {}};
        Require(std::isfinite(node.x) && std::isfinite(node.y) && std::isfinite(node.z), "Invalid coordinates");
        Require(std::stoul(fields[5]) >= 1 && std::stoul(fields[6]) <= 80, "Invalid levels");
        auto first = fields[15].find('\'');
        auto last = fields[15].rfind('\'');
        Require(first != std::string::npos && last > first, "Invalid links column");
        std::istringstream links(fields[15].substr(first + 1, last - first - 1));
        std::string link;
        while (links >> link)
        {
            auto colon = link.find(':');
            Require(colon != std::string::npos, "Invalid link: " + link);
            node.links.push_back(unsigned(std::stoul(link.substr(0, colon))));
            (void)std::stoul(link.substr(colon + 1));
        }
        Require(graph.emplace(node.id, node).second, "Duplicate node ID");
    }
    Require(graph.size() == 55, "Expected exactly 55 arena nodes");
    return graph;
}

void ValidateGraph(Graph const& graph)
{
    for (auto const& [id, node] : graph)
    {
        Require(!node.links.empty(), "Isolated node: " + std::to_string(id));
        std::set<unsigned> unique;
        for (unsigned target : node.links)
        {
            Require(target != id && unique.insert(target).second, "Self or duplicate edge");
            auto next = graph.find(target);
            Require(next != graph.end(), "Dangling edge");
            Require(next->second.map == node.map, "Cross-map edge");
            auto const& back = next->second.links;
            Require(std::find(back.begin(), back.end(), id) != back.end(), "Missing reverse edge");
        }
    }
    for (unsigned map : {559, 562, 572, 617, 618})
    {
        std::vector<unsigned> ids;
        std::vector<unsigned> spawns;
        unsigned covers = 0;
        for (auto const& [id, node] : graph)
            if (node.map == map)
            {
                ids.push_back(id);
                if (node.flags & Spawn) spawns.push_back(id);
                if (node.flags & Cover) ++covers;
                Require(!(node.flags & (64 | 128)), "Battleground objective flags in arena");
            }
        Require(ids.size() == 11 && spawns.size() == 2 && covers >= 2, "Map lacks nodes, spawns or cover");
        Require((graph.at(spawns[0]).flags & (Spawn | Alliance | Horde)) == (Spawn | Alliance), "Invalid Gold spawn");
        Require((graph.at(spawns[1]).flags & (Spawn | Alliance | Horde)) == (Spawn | Horde), "Invalid Green spawn");
        // Validate reachability from BOTH spawns, then traversal with faction restrictions.
        for (unsigned start : spawns)
            for (bool factionRestricted : {false, true})
            {
                unsigned forbidden = graph.at(start).flags & Alliance ? Horde : Alliance;
                std::set<unsigned> visited{start};
                std::queue<unsigned> pending;
                pending.push(start);
                while (!pending.empty())
                {
                    unsigned current = pending.front(); pending.pop();
                    for (unsigned target : graph.at(current).links)
                        if ((!factionRestricted || !(graph.at(target).flags & forbidden)) && visited.insert(target).second)
                            pending.push(target);
                }
                Require(visited.size() == (factionRestricted ? 10u : 11u), "Unreachable arena nodes");
            }
    }
}

void TestCorruptMeshes(Graph const& original)
{
    auto reject = [](Graph const& graph) {
        bool rejected = false;
        try { ValidateGraph(graph); } catch (std::runtime_error const&) { rejected = true; }
        Require(rejected, "Corrupt graph was accepted");
    };
    Graph changed = original;
    changed.at(5400).links.push_back(999999); reject(changed);
    changed = original; changed.at(5400).links.push_back(5420); reject(changed);
    changed = original; changed.at(5400).links.clear(); reject(changed);
    changed = original; changed.at(5402).links.erase(changed.at(5402).links.begin()); reject(changed);
}

void TestMatchmaking()
{
    for (unsigned size : {2, 3, 5})
    {
        Require(ArenaBots::IsTeamSize(size), "Valid bracket rejected");
        for (unsigned members = 0; members <= size + 1; ++members)
        {
            Require(ArenaBots::CanQueueGroup(members, size, true) == (members > 0 && members <= size), "Partial group validation");
            Require(ArenaBots::CanQueueGroup(members, size, false) == (members == size), "Disabled fill changed group validation");
        }
        for (int configured : {-100, -1, 1, 2, 3, 5, 100})
            Require(ArenaBots::NormalizeTarget(configured, size) == size, "Bracket normalization failed");
        Require(ArenaBots::NormalizeTarget(0, size) == 0, "Disabled bracket enabled");
        for (unsigned gold = 0; gold <= size + 2; ++gold)
            for (unsigned green = 0; green <= size + 2; ++green)
            {
                auto a = ArenaBots::MissingPlayers(size, gold);
                auto h = ArenaBots::MissingPlayers(size, green);
                Require(gold + a == std::max(gold, size) && green + h == std::max(green, size), "Incorrect team fill");
            }
        Require(ArenaBots::MissingPlayers(size, 1) == size - 1 && ArenaBots::MissingPlayers(size, 0) == size, "Solo fill failed");
        Require(ArenaBots::MissingPlayers(size, size) == 0, "Full team received allies");
        Require(ArenaBots::MissingPlayers(size, std::numeric_limits<unsigned>::max()) == 0, "Queue overflow");
    }
    for (unsigned invalid : {0, 1, 4, 6, 255})
        Require(!ArenaBots::IsTeamSize(invalid) && ArenaBots::NormalizeTarget(2, invalid) == 0, "Invalid bracket accepted");
}

void TestFlagsAndVictory()
{
    Require(ArenaBots::FlagSpell(true, false) == 32724, "Alliance Gold flag");
    Require(ArenaBots::FlagSpell(false, false) == 32725, "Alliance Green flag");
    Require(ArenaBots::FlagSpell(true, true) == 35774, "Horde Gold flag");
    Require(ArenaBots::FlagSpell(false, true) == 35775, "Horde Green flag");
    using ArenaBots::Result;
    for (unsigned size : {2, 3, 5})
    {
        Require(ArenaBots::Winner(1, 1, size, size) == Result::Pending, "Premature victory");
        Require(ArenaBots::Winner(1, 0, size, size) == Result::Gold, "Gold bot-only survivor");
        Require(ArenaBots::Winner(0, 1, size, size) == Result::Green, "Green bot-only survivor");
        Require(ArenaBots::Winner(0, 0, size, size) == Result::Draw, "Simultaneous elimination");
        Require(ArenaBots::Winner(1, 0, size, 0) == Result::Gold, "Opponent disconnected");
        Require(ArenaBots::Winner(0, 1, 0, size) == Result::Green, "Opponent disconnected");
    }
    Require(ArenaBots::Winner(0, 0, 0, 0) == Result::Pending, "Empty arena ended");
}
}

int main(int argc, char** argv)
{
    try
    {
        auto sql = argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::path(__FILE__).parent_path().parent_path().parent_path() /
            "sql/custom/world/2026_10_24_00_arena_bot_mesh.sql";
        Graph graph = ReadMesh(sql);
        ValidateGraph(graph);
        TestCorruptMeshes(graph);
        TestMatchmaking();
        TestFlagsAndVictory();
        std::cout << "PASS: 5 arena meshes, faction routes, corrupt graphs, 2v2/3v3/5v5 filling, flags and victory conditions\n";
        return 0;
    }
    catch (std::exception const& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
