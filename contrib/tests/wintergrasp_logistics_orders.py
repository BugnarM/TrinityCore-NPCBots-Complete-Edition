#!/usr/bin/env python3
"""Factory/driver orders suppress hunting while preserving defensive native attacks."""
from pathlib import Path
import subprocess,tempfile
repo=Path(__file__).resolve().parents[2]
s=(repo/'src/server/scripts/Northrend/zone_wintergrasp.cpp').read_text()
def function(signature):
 start=s.index(signature);end=s.index('{',start)+1;depth=1
 while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[start:end].replace(' override','')
code=r'''
#include <cassert>
#include <iostream>
#include <map>
using uint32=unsigned;constexpr unsigned SquadSize=5;
struct Guid{unsigned id=0;bool IsEmpty()const{return id==0;}};
struct Creature;
struct Unit {
 bool alive=true,engaged=false;Unit* victim=nullptr;float x=0,y=0;
 bool IsAlive()const{return alive;}bool IsInCombatWith(Unit const*)const{return engaged;}
 Unit* GetVictim()const{return victim;}float GetPositionX()const{return x;}float GetPositionY()const{return y;}
};
struct Kit{Unit* driver=nullptr;Unit* GetPassenger(unsigned)const{return driver;}};
struct Map{std::map<unsigned,Creature*> units;Creature* GetCreature(Guid g){auto i=units.find(g.id);return i==units.end()?nullptr:i->second;}};
struct Creature:Unit{Map* map;Kit* kit=nullptr;explicit Creature(Map* m):map(m){}Map* GetMap()const{return map;}Kit* GetVehicleKit()const{return kit;}};
using GameObject=Unit;
struct BattlefieldWG{Unit* GetBotWarBuilding(unsigned){return nullptr;}};
BattlefieldWG* GetBotWarBattlefield(){return nullptr;}
struct Route{unsigned Slot=0;bool Blocked(BattlefieldWG*)const{return false;}unsigned BarrierEntry()const{return 0;}};
struct Soldier{Creature* me;Guid _buildSite,_vehicle;Route _route;explicit Soldier(Creature* c):me(c){}
ORDERS
ATTACK
};
int main(){Map map;Creature bot(&map),vehicle(&map);Kit kit;vehicle.kit=&kit;map.units[1]=&vehicle;
 Soldier controller(&bot);Unit hostile;
 assert(controller.CanAIAttack(&hostile));
 controller._buildSite={8};assert(controller.HasLogisticsOrder()&&!controller.CanAIAttack(&hostile));
 hostile.engaged=true;assert(controller.CanAIAttack(&hostile));
 hostile.engaged=false;hostile.victim=&bot;assert(controller.CanAIAttack(&hostile)); // ranged attacks also count
 hostile.victim=nullptr;controller._buildSite={};controller._vehicle={1};
 assert(controller.HasLogisticsOrder()&&!controller.CanAIAttack(&hostile)); // board before hunting
 controller._route.Slot=1;assert(!controller.HasLogisticsOrder()&&controller.CanAIAttack(&hostile)); // escort keeps fighting
 controller._route.Slot=0;kit.driver=&bot;assert(!controller.HasLogisticsOrder());
 kit.driver=nullptr;vehicle.alive=false;assert(!controller.HasLogisticsOrder());
 vehicle.alive=true;controller._vehicle={99};assert(!controller.HasLogisticsOrder());
 hostile.alive=false;assert(!controller.CanAIAttack(&hostile));
 std::cout<<"PASS: production native logistics orders prevent proactive hunting while walking/building/boarding; retaliation and infantry escorts remain active\n";
}
'''.replace('ORDERS',function('    bool HasLogisticsOrder(')).replace('ATTACK',function('    bool CanAIAttack('))
with tempfile.TemporaryDirectory(prefix='wg-logistics-orders-') as tmp:
 cpp=Path(tmp)/'test.cpp';exe=Path(tmp)/'test';cpp.write_text(code)
 subprocess.run(['c++','-std=c++20','-O2','-Wall','-Wextra','-Werror',str(cpp),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
