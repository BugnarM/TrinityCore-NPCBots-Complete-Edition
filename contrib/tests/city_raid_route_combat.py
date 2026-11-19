#!/usr/bin/env python3
"""Run the actual event combat filter: city corridor, defenders and reinforcements."""
from pathlib import Path
import subprocess,tempfile
repo=Path(__file__).resolve().parents[2];s=(repo/'src/server/scripts/Events/city_raids.cpp').read_text()
a=s.index('    bool CanAIAttack(Unit const* target) const override');b=s.index('{',a)+1;n=1
while n:n+=(s[b]=='{')-(s[b]=='}');b+=1
method=s[a:b]
code=r'''
#include "src/server/game/AI/NpcBots/botcityraid.h"
#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>
namespace Tactics=NPCBots::CityRaid;using State=Tactics::State;
enum{ROLE_ATTACKER,ROLE_DEFENDER};
struct Position{float x=0,y=0,z=0;float GetPositionX()const{return x;}float GetPositionY()const{return y;}float GetExactDist2d(Position const* p)const{return std::hypot(x-p->x,y-p->y);}};
struct Guid{unsigned value=0;bool IsEmpty()const{return !value;}auto operator<=>(Guid const&)const=default;};
struct BotCfg{struct Settings{float defenderLeash=65.f;};static Settings GetHealingSettings(){return {};}};
struct Unit:Position{unsigned entry=0;Guid guid;unsigned GetEntry()const{return entry;}Guid GetGUID()const{return guid;}bool alive=true,world=true,critter=false,friendly=false;unsigned map=0;Unit* victim=nullptr;bool IsAlive()const{return alive;}bool IsInWorld()const{return world;}bool IsCritter()const{return critter;}unsigned GetMap()const{return map;}bool IsFriendlyTo(Unit const*)const{return friendly;}Unit* GetVictim()const{return victim;}};
struct Base{virtual ~Base()=default;virtual bool CanAIAttack(Unit const*)const{return true;}};
struct CityRaidBotAI:Base{
 Unit creature;Unit* me=&creature;struct Runtime{std::atomic<bool> Running{true};};std::shared_ptr<Runtime> _runtime=std::make_shared<Runtime>();
 struct Config{unsigned BossEntry=4949;std::array<Position,3> CheckpointSpawns{{{0,0,0},{0,0,0},{1922,-4204,42}}};std::vector<Position> Waypoints{{-9115,423,96},{-8953.95f,521.02f,96.54f},{-8831.8f,596.4f,95.8f}};}_cfg;
 struct Order{Guid Focus;State Mode=State::Marching;Position Destination{-9115,423,96};bool Engage=true;unsigned Frontline=0;}_order;
 unsigned _side=ROLE_ATTACKER;bool _reinforcing=false;
METHOD
};
int main(){
 CityRaidBotAI c;Unit guard;guard.x=-9100;guard.y=430;assert(c.CanAIAttack(&guard));
 Unit mine;mine.x=-8790;mine.y=-150;assert(!c.CanAIAttack(&mine)); // Echo Ridge / Northshire cannot divert a city raid
 Unit goldshire;goldshire.x=-9340;goldshire.y=171;assert(!c.CanAIAttack(&goldshire));
 c._order.Frontline=1;guard.x=-9025;guard.y=475;assert(c.CanAIAttack(&guard));
 guard.x=-9025;guard.y=800;assert(!c.CanAIAttack(&guard)); // PvP lure outside the finite route corridor
 c._reinforcing=true;guard.x=-9025;guard.y=475;guard.victim=c.me;assert(!c.CanAIAttack(&guard)); // no off-route graveyard/chase behavior
 c._reinforcing=false;c._order.Mode=State::Retreating;assert(!c.CanAIAttack(&guard));
 c._order.Mode=State::Boss;c._order.Destination={-8441,333,122};guard.x=-8440;guard.y=335;assert(c.CanAIAttack(&guard));assert(!c.CanAIAttack(&mine));
 c._side=ROLE_DEFENDER;c._order.Mode=State::Marching;c._order.Destination={1922,-4204,42};guard.x=1920;guard.y=-4200;assert(c.CanAIAttack(&guard));
 guard.x=1352;guard.y=-4395;assert(!c.CanAIAttack(&guard));
 Unit boss;boss.entry=4949;guard.victim=&boss;assert(!c.CanAIAttack(&guard));
 guard.victim=nullptr;c._order.Focus=guard.guid={7};assert(!c.CanAIAttack(&guard));
 c._order.Focus={};
 c._order.Mode=State::Boss;c._order.Destination={1352,-4395,42};assert(!c.CanAIAttack(&guard));
 c._order.Mode=State::Marching;c._order.Destination={1922,-4204,42}; // defenders cannot follow a lure out of Orgrimmar
 guard.x=1920;guard.y=-4200;guard.world=false;assert(!c.CanAIAttack(&guard));guard.world=true;guard.map=1;assert(!c.CanAIAttack(&guard));
 assert(!c.CanAIAttack(nullptr));
 std::cout<<"PASS: actual event AI rejects Echo Ridge/Goldshire detours, outside-city lures, reinforcement fights and invalid targets; city and boss combat remain enabled\n";
}
'''.replace('METHOD',method)
with tempfile.TemporaryDirectory(prefix='city-route-combat-') as tmp:
 src=Path(tmp)/'test.cpp';exe=Path(tmp)/'test';src.write_text(code)
 subprocess.run(['c++','-std=c++20','-O2','-Wall','-Wextra','-Werror','-I',str(repo),str(src),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
