#!/usr/bin/env python3
"""Execute the actual pre-march roster check against incomplete/duplicate/group failures."""
from pathlib import Path
import subprocess,tempfile
repo=Path(__file__).resolve().parents[2];s=(repo/'src/server/scripts/Events/city_raids.cpp').read_text()
a=s.index('    bool VerifyRoster(Map* map) const');b=s.index('{',a)+1;n=1
while n:n+=(s[b]=='{')-(s[b]=='}');b+=1
method=s[a:b]
code=r'''
#include "src/server/game/AI/NpcBots/botcityraid.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <set>
#include <vector>
using uint32=std::uint32_t;namespace Tactics=NPCBots::CityRaid;
struct ObjectGuid{unsigned value=0;bool IsEmpty()const{return !value;}auto operator<=>(ObjectGuid const&)const=default;};
struct CityRaidBotMember{ObjectGuid Guid;};
struct Group{std::set<ObjectGuid> ids;unsigned GetMembersCount()const{return ids.size();}bool IsMember(ObjectGuid g)const{return ids.count(g);}};
struct Creature{Group* group=nullptr;bool world=true,controller=true,alive=true;Group* GetBotGroup(){return group;}};
struct Map{std::map<ObjectGuid,Creature> bots;};
struct GroupMgr{std::map<ObjectGuid,Group*> groups;Group* GetGroupByGUID(ObjectGuid g){auto i=groups.find(g);return i==groups.end()?nullptr:i->second;}}mgr;
auto* sGroupMgr=&mgr;
struct Instance{
 std::vector<CityRaidBotMember> _attackers,_defenders;unsigned _defendCount=15;std::array<ObjectGuid,2> _groups{{{1},{2}}};
 Creature* GetBot(Map* m,CityRaidBotMember const& member)const{if(!m)return nullptr;auto i=m->bots.find(member.Guid);return i==m->bots.end()||!i->second.world?nullptr:&i->second;}
 static void* AI(Creature* b){return b&&b->controller?b:nullptr;}
METHOD
};
struct Fixture{
 Instance raid;Map map;Group attackers,defenders;
 Fixture(unsigned defenses=15){
  raid._defendCount=defenses;mgr.groups={{{1},&attackers}};
  if(defenses)mgr.groups.emplace(ObjectGuid{2},&defenders);else raid._groups[1]={0};
  for(unsigned i=0;i<40+defenses;++i){ObjectGuid id{i+1};Group* group=i<40?&attackers:&defenders;group->ids.insert(id);map.bots.emplace(id,Creature{group});(i<40?raid._attackers:raid._defenders).push_back({id});}
 }
};
int main(){
 for(unsigned defenders:{0,5,15}){Fixture f(defenders);assert(f.raid.VerifyRoster(&f.map));assert(f.raid._attackers.size()==40&&f.map.bots.size()==40+defenders);}
 {Fixture f;f.raid._attackers.pop_back();assert(!f.raid.VerifyRoster(&f.map));}
 {Fixture f;f.map.bots.erase(ObjectGuid{40});assert(!f.raid.VerifyRoster(&f.map));}
 {Fixture f;f.map.bots.at(ObjectGuid{1}).world=false;assert(!f.raid.VerifyRoster(&f.map));}
 {Fixture f;f.map.bots.at(ObjectGuid{1}).controller=false;assert(!f.raid.VerifyRoster(&f.map));}
 {Fixture f;f.map.bots.at(ObjectGuid{1}).group=&f.defenders;assert(!f.raid.VerifyRoster(&f.map));}
 {Fixture f;f.attackers.ids.erase(ObjectGuid{40});f.attackers.ids.insert(ObjectGuid{999});assert(!f.raid.VerifyRoster(&f.map));}
 {Fixture f;f.raid._attackers.back()=f.raid._attackers.front();assert(!f.raid.VerifyRoster(&f.map));}
 {Fixture f;mgr.groups.erase(ObjectGuid{1});assert(!f.raid.VerifyRoster(&f.map));}
 {Fixture f;f.raid._defenders.pop_back();assert(!f.raid.VerifyRoster(&f.map));}
 {Fixture f;f.map.bots.at(ObjectGuid{1}).alive=false;assert(f.raid.VerifyRoster(&f.map));} // death uses the same roster slot and normal resurrection waves
 std::cout<<"PASS: 40 attackers plus 0/5/15 additional defenders; missing, duplicate, absent-controller and wrong-group members prevent marching\n";
}
'''.replace('METHOD',method)
with tempfile.TemporaryDirectory(prefix='city-roster-') as tmp:
 src=Path(tmp)/'test.cpp';exe=Path(tmp)/'test';src.write_text(code)
 subprocess.run(['c++','-std=c++20','-O2','-Wall','-Wextra','-Werror','-I',str(repo),str(src),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
