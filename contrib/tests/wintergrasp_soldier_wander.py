#!/usr/bin/env python3
"""Run the production scouting radius and idle movement; combat/vehicles preempt patrols."""
from pathlib import Path
import subprocess
import tempfile
repo=Path(__file__).resolve().parents[2]
s=(repo/'src/server/scripts/Northrend/zone_wintergrasp.cpp').read_text()
def function(signature):
 start=s.index(signature);end=s.index('{',start)+1;depth=1
 while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[start:end]
code=r'''
#include "src/server/scripts/Northrend/wintergrasp_tactics.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <random>
using uint32=std::uint32_t;using namespace WintergraspBotWar;
enum TeamId {TEAM_ALLIANCE,TEAM_HORDE};
enum { UNIT_STATE_NOT_MOVE=1,AREA_THE_BROKEN_TEMPLATE=1000,AREA_THE_SUNKEN_RING };
int WGFactionTable[]={1732,1735};
struct Position { float m_positionX=100,m_positionY=100,m_positionZ=10;
 float GetExactDist2d(Position const* p)const{return std::hypot(m_positionX-p->m_positionX,m_positionY-p->m_positionY);} };
struct Map{};
struct Creature:Position {
 bool vehicle=false,aboard=false,combat=false,root=false,moving=false,mounted=true,walking=false;
 int faction=1732;Map map;
 bool IsVehicle()const{return vehicle;}bool GetVehicle()const{return aboard;}bool IsInCombat()const{return combat;}
 bool HasUnitState(int)const{return root;}bool isMoving()const{return moving;}bool IsMounted()const{return mounted;}
 void Dismount(){mounted=false;}void SetWalk(bool b){walking=b;}int GetFaction()const{return faction;}Map* GetMap(){return &map;}
};
struct BattlefieldWG { int owner=TEAM_ALLIANCE;uint32 GetData(uint32)const{return owner;} } wg;
BattlefieldWG* GetBotWarBattlefield(){return &wg;}
uint32 WorkshopArea(Lane){return AREA_THE_BROKEN_TEMPLATE;}
std::mt19937 randomEngine(71);
uint32 urand(uint32 low,uint32 high){return std::uniform_int_distribution<uint32>(low,high)(randomEngine);}
float frand(float low,float high){return std::uniform_real_distribution<float>(low,high)(randomEngine);}
void SetBotWarMovementSpeed(Creature*,bool){}
bool ProjectBotWarGround(Map*,Position&){return true;}
struct Navigation {uint32 calls=0;Position goal;void Move(Creature* c,Position const& p){++calls;goal=p;c->moving=true;}};
struct Route {
 Lane Route=WorkshopWest;uint32 Point=0,WanderTimer=0;bool blocked=false;
 Navigation VehicleNavigation;
 bool Blocked(BattlefieldWG*)const{return blocked;}
RADIUS
WANDER
};
int main(){
 for(int team:{TEAM_ALLIANCE,TEAM_HORDE}) {
  Creature bot;bot.faction=WGFactionTable[team];wg.owner=team;Position anchor=bot;
  Route patrol;assert(patrol.PatrolRadius(&wg,&bot)==38.f);
  patrol.blocked=true;assert(patrol.PatrolRadius(&wg,&bot)==12.f);patrol.blocked=false;
  wg.owner=team==TEAM_ALLIANCE?TEAM_HORDE:TEAM_ALLIANCE;assert(patrol.PatrolRadius(&wg,&bot)==12.f);
  wg.owner=team;patrol.Route=GuardSouth;assert(patrol.PatrolRadius(&wg,&bot)==30.f);patrol.Route=WorkshopWest;
  for(int restriction=0;restriction<4;++restriction){
   bot.vehicle=restriction==0;bot.aboard=restriction==1;bot.combat=restriction==2;bot.root=restriction==3;
   patrol.UpdateWander(&bot,anchor,9000);assert(patrol.VehicleNavigation.calls==0);
  }
  bot.vehicle=bot.aboard=bot.combat=bot.root=false;
  for(unsigned i=0;i<400;++i){
   bot.moving=false;patrol.WanderTimer=1;patrol.UpdateWander(&bot,anchor,2);
   assert(bot.walking&&!bot.mounted&&bot.GetExactDist2d(&patrol.VehicleNavigation.goal)<=38.01f);
   assert(patrol.WanderTimer>=5000&&patrol.WanderTimer<=11000);
  }
  unsigned before=patrol.VehicleNavigation.calls;bot.moving=true;patrol.WanderTimer=1;
  patrol.UpdateWander(&bot,anchor,2);assert(patrol.VehicleNavigation.calls==before);
 }
 std::cout<<"PASS: production patrols for both factions stay near objectives, hold capture/gate radius, pause naturally, and yield to combat/vehicles/roots\n";
}
'''.replace('RADIUS',function('    float PatrolRadius(')).replace('WANDER',function('    void UpdateWander('))
with tempfile.TemporaryDirectory(prefix='wg-wander-') as tmp:
 cpp=Path(tmp)/'test.cpp';binary=Path(tmp)/'test';cpp.write_text(code)
 subprocess.run(['c++','-std=c++20','-O2','-Wall','-Wextra','-Werror','-I',str(repo),str(cpp),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
