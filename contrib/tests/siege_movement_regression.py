#!/usr/bin/env python3
"""Run production checkpoint planning and preparation code with simulated movers."""
from pathlib import Path
import csv
import subprocess
import sys
import tempfile
import textwrap

repo = Path(__file__).resolve().parents[2]
source = (repo / "src/server/game/AI/NpcBots/bot_battlegrounds.cpp").read_text()
movement = Path(sys.argv[1]).read_text() if len(sys.argv) > 1 else source
planner_start = movement.index("    auto allowed =", movement.index("bool bot_ai::MoveToSiegeObjective"))
planner_end = movement.index("    if (mover == me && !me->GetTransport())", planner_start)
planner = textwrap.indent(movement[planner_start:planner_end], "    ")
prep_start = source.index("    if (bg->GetStatus() != STATUS_IN_PROGRESS", source.index("bool bot_ai::UpdateSiegeBattleground"))
prep_end = source.index("    RemoveBotCommandState(BOT_COMMAND_STAY)", prep_start)
preparation = textwrap.indent(source[prep_start:prep_end], "    ")

nodes, links = [], []
for row in (repo / "sql/custom/world/2026_10_23_01_battleground_bots_sa_ic.sql").read_text().splitlines():
    if not row.startswith(("(52", "(53")):
        continue
    fields = next(csv.reader([row[1:row.rfind(")")]], quotechar="'", skipinitialspace=True))
    node_id, map_id = int(fields[0]), int(fields[2])
    xyz = ",".join(str(float(fields[i])) + "f" for i in (11, 12, 13))
    nodes.append(f"nodes.emplace({node_id}, WanderNode({node_id},{map_id},{xyz}));")
    for token in fields[15].split():
        target = int(token.split(":")[0])
        links.append(f"nodes.at({node_id}).links.push_back({{&nodes.at({target})}});")

code = r'''
#include "src/server/game/AI/NpcBots/botbgtactics.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <map>
#include <queue>
#include <unordered_map>
#include <vector>
using uint32 = std::uint32_t;
using uint8 = std::uint8_t;
using namespace NPCBots::SiegeBG;
enum { BATTLEGROUND_SA, BATTLEGROUND_IC, STATUS_WAIT_JOIN, STATUS_IN_PROGRESS,
       BG_SA_WARMUP, BG_SA_ROUND_ONE, BG_SA_SECOND_WARMUP, BG_SA_ROUND_TWO,
       BOT_COMMAND_STAY = 256 };
void Check(bool ok, char const* message) { if (!ok) { std::cerr << message << '\n'; std::exit(1); } }
struct Position
{
    float x=0, y=0, z=0;
    float GetPositionX() const { return x; }
    float GetPositionY() const { return y; }
    float GetExactDistSq(Position const* p) const { return (x-p->x)*(x-p->x)+(y-p->y)*(y-p->y)+(z-p->z)*(z-p->z); }
    float GetExactDist(Position const* p) const { return std::sqrt(GetExactDistSq(p)); }
};
struct WanderNode : Position
{
    struct Link { WanderNode const* wp; };
    unsigned id, map;
    std::vector<Link> links;
    WanderNode(unsigned i,unsigned m,float px,float py,float pz) : Position{px,py,pz},id(i),map(m) { }
    unsigned GetWPId() const { return id; }
    auto const& GetLinks() const { return links; }
    template<class F> static void DoForAllMapWPs(unsigned map, F f);
    template<class F> static WanderNode const* FindInMapWPs(unsigned map, F f);
};
std::map<unsigned, WanderNode> nodes;
template<class F> void WanderNode::DoForAllMapWPs(unsigned map,F f)
{ for (auto const& [_,node] : nodes) if (node.map==map) f(&node); }
template<class F> WanderNode const* WanderNode::FindInMapWPs(unsigned map,F f)
{ for (auto const& [_,node] : nodes) if (node.map==map && f(&node)) return &node; return nullptr; }
struct Actor : Position
{
    unsigned map=607, stops=0;
    bool moving=false;
    unsigned GetMapId() const { return map; }
    bool isMoving() const { return moving; }
    void BotStopMovement() { ++stops; moving=false; }
};
struct Battleground
{
    int status=STATUS_IN_PROGRESS;
    unsigned type=BATTLEGROUND_SA;
    int GetStatus() const { return status; }
    unsigned GetTypeID() const { return type; }
};
struct SA { int round=BG_SA_ROUND_ONE; int GetRoundStatus() const { return round; } };
struct Controller
{
    uint32 _siegeTravelNode=0, commands=0;
    bool staying=false;
    bool HasBotCommandState(unsigned) const { return staying; }
    void SetBotCommandState(unsigned) { ++commands; staying=true; }
    bool Prepare(Battleground* bg,SA* sa,Actor* me)
    {
PREPARATION
        return false;
    }
    bool Plan(Actor* mover,Position const& destination,uint32 gates,bool attacker,Position& result,uint8 team=0)
    {
        Actor* me=mover;
        Battleground storage;
        storage.type=mover->map==628 ? BATTLEGROUND_IC : BATTLEGROUND_SA;
        Battleground* bg=&storage;
        bool icMap=storage.type==BATTLEGROUND_IC;
        uint8 originKeep=ICPositionKeep(mover->x,mover->y),destinationKeep=ICPositionKeep(destination.x,destination.y);
PLANNER
        result=point;
        return true;
    }
};
int main()
{
NODES
LINKS
    // Physical movement is simulated; routing and production SQL are real.
    // Every long edge is sampled repeatedly, as on a live one-second AI tick.
    for (float speed : {3.f,7.f,18.f})
        for (unsigned spawn : {5200u,5201u,5215u})
            for (unsigned target : spawn==5215 ? std::vector<unsigned>{5221,5222} : std::vector<unsigned>{5204,5206})
            {
                Actor actor; static_cast<Position&>(actor)=nodes.at(spawn);
                Position destination=nodes.at(target),next;
                Controller controller;
                bool arrived=false;
                for (unsigned tick=0;tick<900;++tick)
                {
                    if (actor.GetExactDist(&destination)<7.f) { arrived=true; break; }
                    Check(controller.Plan(&actor,destination,spawn==5215?63u:0u,spawn!=5215,next),"no tactical graph route");
                    float distance=actor.GetExactDist(&next);
                    float fraction=distance ? std::min(1.f,speed/distance) : 0.f;
                    actor.x+=(next.x-actor.x)*fraction;
                    actor.y+=(next.y-actor.y)*fraction;
                    actor.z+=(next.z-actor.z)*fraction;
                }
                Check(arrived,"FAIL: bot oscillates near its spawn instead of reaching the next road checkpoint");
            }
    Check(KeepSiegeCheckpoint(18.f) && !KeepSiegeCheckpoint(17.99f),"checkpoint arrival boundary");
    // Native exterior landing points. Old nearest-node selection commanded a
    // closed gate after the centre portals, leaving the bot stuck after transfer.
    for (Position landing : {Position{1139.498f,-779.4739f,48.73496f},
        Position{1232.524f,-666.3246f,48.13402f},Position{1232.387f,-861.0243f,48.99959f},
        Position{313.2344f,-918.0452f,48.85597f},Position{430.6007f,-857.0052f,48.31177f},
        Position{326.2135f,-744.0243f,49.43576f}})
    {
        Actor actor;actor.map=628;static_cast<Position&>(actor)=landing;
        uint8 team=landing.x>1000.f?1:0;
        for (uint32 stale : {0u,team?5306u:5301u})
        {
            Controller controller;controller._siegeTravelNode=stale;Position next;
            Check(controller.Plan(&actor,nodes.at(5310),0,true,next,team),"no route after keep teleport");
            Check(ICSegmentOpen(landing.x,landing.y,next.x,next.y,0),"teleporter exit reattaches to an intact gate");
            Check(controller._siegeTravelNode!=5306&&controller._siegeTravelNode!=5301,"stale closed-gate checkpoint kept");
        }
    }
    for (int team=0;team<2;++team)
        for (int round : {BG_SA_WARMUP,BG_SA_SECOND_WARMUP})
        {
            Controller controller; Actor actor; actor.moving=true;
            Battleground bg; SA sa; sa.round=round;
            for (unsigned tick=0;tick<1200;++tick)
                Check(controller.Prepare(&bg,&sa,&actor),"preparation released the class AI");
            Check(controller.staying && controller.commands==1 && actor.stops==1 && !actor.moving,
                "preparation repeatedly moves/stops bots instead of keeping them idle");
            sa.round=BG_SA_ROUND_ONE;
            Check(!controller.Prepare(&bg,&sa,&actor),"active round still held in preparation");
            bg.status=STATUS_WAIT_JOIN;
            Check(controller.Prepare(&bg,&sa,&actor),"BG preparation status was ignored");
        }
    std::cout << "PASS: actual planner reaches SA fronts, IC native landings avoid closed gates/stale checkpoints; both preparation rounds remain idle\n";
}
'''
for marker, value in [("PLANNER", planner), ("PREPARATION", preparation), ("NODES", "\n".join(nodes)), ("LINKS", "\n".join(links))]:
    code = code.replace(marker, value)
with tempfile.TemporaryDirectory(prefix="siege-movement-") as tmp:
    cpp, binary = Path(tmp) / "test.cpp", Path(tmp) / "test"
    cpp.write_text(code)
    subprocess.run(["c++", "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror", "-I", str(repo), str(cpp), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
