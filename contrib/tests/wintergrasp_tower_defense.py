#!/usr/bin/env python3
"""Test production tower routing, including live reassignment and later spawns."""
from pathlib import Path
import subprocess
import tempfile
import textwrap

repo = Path(__file__).resolve().parents[2]
source = (repo / "src/server/scripts/Northrend/zone_wintergrasp.cpp").read_text()

def function(text, signature):
    start = text.index(signature)
    end = text.index("{", start) + 1
    depth = 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]

resolve = function(source, "WintergraspBotWar::Lane ResolveSquadLane(")
route_source = source[source.index("struct BotRouteMovement"):]
assign = textwrap.indent(function(route_source, "    void Assign(Creature* bot, Lane lane, uint32 slot)"), "    ")
update = textwrap.indent(function(route_source, "    void Update(Creature* bot, BattlefieldWG* wg, uint32 diff)"), "    ")
assert "&& !IsTowerDefense(_route.Route)" in source, "garrisons can leave their tower to board spare vehicles"
assert source.count("Lane lane = ResolveSquadLane(wg, team == wg->GetAttackerTeam(), index);") == 2, "spawn/vehicle replacement ignores tower state"

code = r'''
#include "src/server/scripts/Northrend/wintergrasp_bot_war.h"
#include <array>
#include <cassert>
#include <iostream>
#include <map>
#include <vector>
using uint32=std::uint32_t;
using namespace WintergraspBotWar;
enum TeamId { TEAM_ALLIANCE,TEAM_HORDE };
enum { GO_DESTRUCTIBLE_INTACT,GO_DESTRUCTIBLE_DESTROYED,AREA_THE_BROKEN_TEMPLATE,AREA_THE_SUNKEN_RING };
int WGFactionTable[]={1732,1735};
struct Position { };
struct Creature
{
    bool vehicle=false;
    float distance=0;
    bool IsVehicle() const { return vehicle; }
    int GetFaction() const { return WGFactionTable[TEAM_ALLIANCE]; }
    float GetExactDist2d(Position const*) const { return distance; }
    bool isMoving() const { return false; }
};
struct GameObject { int state=GO_DESTRUCTIBLE_INTACT; int GetDestructibleState() const { return state; } };
struct BattlefieldWG
{
    std::map<uint32,GameObject> towers;
    uint32 owner=TEAM_ALLIANCE;
    GameObject* GetBotWarBuilding(uint32 entry)
    {
        auto it=towers.find(entry);
        return it==towers.end()?nullptr:&it->second;
    }
    uint32 GetData(int) const { return owner; }
};
std::array<std::vector<int>,LaneCount> BotRoutes;
RESOLVE
struct Route
{
    Lane Route=Center;
    uint32 Point=0,Slot=0,RetryTimer=0,moves=0,WanderTimer=0;
    bool Assigned=false,Arrived=false;
    Position Destination(Creature*) const { return {}; }
    bool Blocked(BattlefieldWG*) const { return false; }
    void Move(Creature*) { ++moves; }
    void UpdateNavigation(Creature*,uint32) { }
    void UpdateWander(Creature*, Position const&, uint32) { }
    float PatrolRadius(BattlefieldWG*,Creature*) const{return 12.f;}
ASSIGN
UPDATE
};
int main()
{
    for(auto& route:BotRoutes) route.resize(1);
    // Both factions receive the same duties when they take the attacking role.
    for(TeamId defender:{TEAM_ALLIANCE,TEAM_HORDE})
        for(uint32 mask=0;mask<8;++mask)
        {
            BattlefieldWG wg;
            wg.towers[190357].state=(mask&1)?GO_DESTRUCTIBLE_DESTROYED:GO_DESTRUCTIBLE_INTACT;
            wg.towers[190356].state=(mask&2)?GO_DESTRUCTIBLE_DESTROYED:GO_DESTRUCTIBLE_INTACT;
            wg.towers[190358].state=(mask&4)?GO_DESTRUCTIBLE_DESTROYED:GO_DESTRUCTIBLE_INTACT;
            for(TeamId team:{TEAM_ALLIANCE,TEAM_HORDE})
            {
                bool attacker=team!=defender;
                std::array<uint32,LaneCount> counts{};
                for(uint32 slot=0;slot<120;++slot)
                    ++counts[ResolveSquadLane(&wg,attacker,slot)];
                if(attacker)
                {
                    assert(counts[Center]==30u+((mask&1)?10u:0u));
                    assert(counts[West]==30u+((mask&2)?10u:0u));
                    assert(counts[East]==30u+((mask&4)?10u:0u));
                    assert(counts[GuardSouth]==((mask&1)?0u:10u));
                    assert(counts[GuardSouthWest]==((mask&2)?0u:10u));
                    assert(counts[GuardSouthEast]==((mask&4)?0u:10u));
                }
                else
                    assert(counts[South]==15 && counts[SouthWest]==15 && counts[SouthEast]==15);
            }
        }
    for(Lane lane:{GuardSouth,GuardSouthWest,GuardSouthEast})
    {
        Creature bot; BattlefieldWG wg; struct Route route;
        route.Route=lane;route.Slot=47;route.Assigned=true;route.Arrived=true;
        // An unavailable grid must not be interpreted as a destroyed tower.
        route.Update(&bot,&wg,1000);
        assert(route.Route==lane && route.moves==0);
        wg.towers[GuardedTowerEntry(lane)].state=GO_DESTRUCTIBLE_INTACT;
        route.Update(&bot,&wg,1000);
        assert(route.Route==lane && route.moves==0);
        wg.towers[GuardedTowerEntry(lane)].state=GO_DESTRUCTIBLE_DESTROYED;
        route.Update(&bot,&wg,1000);
        assert(route.Route==ResolveTowerDefense(lane,true) && route.Slot==47);
        assert(route.Point==0 && !route.Arrived && route.moves==1);
    }
    // A completed safe prefix is not a completed tactical waypoint.
    BotRoutes[Center].resize(2);
    Creature vehicle;vehicle.vehicle=true;vehicle.distance=100;
    BattlefieldWG wg;struct Route prefix;
    prefix.Route=Center;prefix.Assigned=true;prefix.RetryTimer=5000;
    prefix.Update(&vehicle,&wg,1000);
    assert(prefix.Point==0 && !prefix.Arrived && prefix.moves==0);
    vehicle.distance=8;
    prefix.Update(&vehicle,&wg,1000);
    assert(prefix.Point==1 && prefix.moves==1);
    // Combat can leave a capturer outside the flag after a prior arrival.
    BotRoutes[West].resize(2);
    Creature capturer;capturer.distance=40;
    BattlefieldWG workshop;workshop.owner=TEAM_HORDE;
    struct Route capture;capture.Route=West;capture.Assigned=true;capture.Arrived=true;
    capture.Update(&capturer,&workshop,1000);
    assert(!capture.Arrived&&capture.moves==1&&capture.Point==0);
    capturer.distance=0;capture.Update(&capturer,&workshop,1000);
    assert(capture.Arrived&&capture.Point==0); // hold until owned
    workshop.owner=TEAM_ALLIANCE;capture.Update(&capturer,&workshop,1000);
    assert(capture.Point==1&&capture.moves==2);
    std::cout<<"PASS: production tower resolver for both factions/all eight tower states; live reassignment; vehicle prefixes require actual waypoint proximity\n";
}
'''
for marker, value in [("RESOLVE", resolve), ("ASSIGN", assign), ("UPDATE", update)]:
    code = code.replace(marker, value)
with tempfile.TemporaryDirectory(prefix="wg-tower-defense-") as tmp:
    cpp, binary = Path(tmp) / "test.cpp", Path(tmp) / "test"
    cpp.write_text(code)
    subprocess.run(["c++", "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror", "-I", str(repo), str(cpp), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
