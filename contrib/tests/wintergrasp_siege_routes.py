#!/usr/bin/env python3
"""Execute production assault redirects: never skip closed or unavailable walls."""
from pathlib import Path
import subprocess
import tempfile
repo=Path(__file__).resolve().parents[2]
s=(repo/'src/server/scripts/Northrend/zone_wintergrasp.cpp').read_text()
start=s.index('    void RedirectAssault(');end=s.index('{',start)+1;depth=1
while depth:
    depth+=(s[end]=='{')-(s[end]=='}');end+=1
redirect=s[start:end]
code=r'''
#include "src/server/scripts/Northrend/wintergrasp_tactics.h"
#include <array>
#include <cassert>
#include <iostream>
#include <map>
#include <vector>
using uint32=std::uint32_t;
using namespace WintergraspBotWar;
enum TeamId{TEAM_ALLIANCE,TEAM_HORDE};
enum{AREA_THE_BROKEN_TEMPLATE,AREA_THE_SUNKEN_RING};
int WGFactionTable[]={1732,1735};
struct Position{float x=0;float GetPositionX() const{return x;}};
struct Creature:Position{int faction=1732;int GetFaction() const{return faction;}};
struct Waypoint{Position Pos;uint32 Barrier=0;};
std::array<std::vector<Waypoint>,LaneCount> BotRoutes;
struct BattlefieldWG
{
 uint32 owner=TEAM_ALLIANCE;
 std::map<uint32,bool> destroyed;
 uint32 GetData(int) const{return owner;}
};
struct RouteController
{
 Lane Route=Center;uint32 Point=0,Slot=0,moves=0;bool Arrived=true;
 Position Destination(Creature*) const{return BotRoutes[Route][Point].Pos;}
 bool Blocked(BattlefieldWG* wg) const
 {
  uint32 barrier=BotRoutes[Route][Point].Barrier;
  if(!barrier)return false;
  auto it=wg->destroyed.find(barrier);
  return it==wg->destroyed.end()||!it->second;
 }
 void Move(Creature*){++moves;}
REDIRECT
};
int main()
{
 BotRoutes[Center]={{{4550},0},{{5135},190375},{{5250},191805},{{5370},191810}};
 BotRoutes[West]={{{4948},0},{{5100},0},{{5180},190371},{{5250},191805},{{5370},191810}};
 for(TeamId team:{TEAM_ALLIANCE,TEAM_HORDE})
 {
  Creature bot;bot.x=5350;bot.faction=WGFactionTable[team];BattlefieldWG wg;wg.owner=team;
  RouteController route;route.Point=2;
  route.RedirectAssault(&bot,&wg,West);
  assert(route.Route==West&&route.Point==2&&!route.Arrived&&route.moves==1); // unknown outer wall remains closed
  wg.destroyed[190371]=true;route.Route=Center;
  route.RedirectAssault(&bot,&wg,West);assert(route.Point==3); // stop at intact/missing inner wall
  wg.destroyed[191805]=true;route.Route=Center;
  route.RedirectAssault(&bot,&wg,West);assert(route.Point==4); // never skip the vault gate
  wg.owner=TeamId(1-team);route.Route=Center;
  route.RedirectAssault(&bot,&wg,West);assert(route.Point==0); // lost workshop has to be secured again
  route.Route=GuardSouth;auto before=route.moves;
  route.RedirectAssault(&bot,&wg,West);assert(route.Route==GuardSouth&&route.moves==before);
  route.Route=West;route.Point=3;
  route.RedirectAssault(&bot,&wg,West);assert(route.Point==3&&route.moves==before); // no repeated rewind
 }
 std::cout<<"PASS: actual siege redirect preserves workshop ownership and every intact/unloaded barrier for both factions\n";
}
'''.replace('REDIRECT',redirect)
with tempfile.TemporaryDirectory(prefix='wg-siege-routes-') as tmp:
    cpp,binary=Path(tmp)/'test.cpp',Path(tmp)/'test';cpp.write_text(code)
    subprocess.run(['c++','-std=c++20','-O2','-Wall','-Wextra','-Werror','-I',str(repo),str(cpp),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
