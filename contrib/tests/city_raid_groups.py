#!/usr/bin/env python3
"""Exercise the actual native subgroup move used to distribute city-raid buffs."""
from pathlib import Path
import subprocess,tempfile
repo=Path(__file__).resolve().parents[2];s=(repo/'src/server/game/Groups/Group.cpp').read_text()
a=s.index('void Group::ChangeMembersGroup(');b=s.index('{',a)+1;n=1
while n:n+=(s[b]=='{')-(s[b]=='}');b+=1
method=s[a:b]
code=r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <vector>
using uint8=std::uint8_t;using uint32=std::uint32_t;
struct ObjectGuid{uint32 id;bool creature=true;bool IsCreature()const{return creature;}uint32 GetEntry()const{return id;}uint32 GetCounter()const{return id;}bool operator==(ObjectGuid const&)const=default;};
struct Group;
struct Ref{uint8 sub=0;void setSubGroup(uint8 v){sub=v;}};
struct Creature{
 Group* active=nullptr;uint8 subgroup=0,original=7;
 Group* GetBotGroup(){return active;}void SetSubGroup(uint8 i){subgroup=i;}void SetOriginalSubGroup(uint8 i){original=i;}uint8 GetOriginalSubGroup(){return original;}
};
namespace BotDataMgr{std::map<uint32,Creature*> bots;Creature const* FindBot(uint32 i){return bots.at(i);}}
struct Player{Group* GetGroup(){return nullptr;}Ref ref;Ref& GetGroupRef(){return ref;}Ref& GetOriginalGroupRef(){return ref;}uint8 GetOriginalSubGroup(){return 0;}};
namespace ObjectAccessor{Player* FindConnectedPlayer(ObjectGuid){return nullptr;}}
struct CharacterDatabasePreparedStatement{void setUInt8(unsigned,uint8){}void setUInt32(unsigned,uint32){}};
constexpr unsigned CHAR_UPD_GROUP_MEMBER_SUBGROUP=1;
struct DB{unsigned writes=0;CharacterDatabasePreparedStatement stmt;auto* GetPreparedStatement(unsigned){return &stmt;}void Execute(CharacterDatabasePreparedStatement*){++writes;}}CharacterDatabase;
struct Group{
 struct Member{ObjectGuid guid;uint8 group;};std::vector<Member> m_memberSlots;using member_witerator=std::vector<Member>::iterator;
 std::array<unsigned,8> counts{};bool persistent=false;
 bool isRaidGroup()const{return true;}bool IsPersistentGroup()const{return persistent;}
 auto _getMemberWSlot(ObjectGuid guid){return std::find_if(m_memberSlots.begin(),m_memberSlots.end(),[&](Member const& m){return m.guid==guid;});}
 uint8 GetMemberGroup(ObjectGuid g){return _getMemberWSlot(g)->group;}
 void SubGroupCounterIncrease(uint8 g){++counts[g];}void SubGroupCounterDecrease(uint8 g){--counts[g];}void SendUpdate(){}
 void ChangeMembersGroup(ObjectGuid,uint8);
};
METHOD
int main(){
 Group raid;Creature bot;bot.active=&raid;BotDataMgr::bots.emplace(1,&bot);raid.m_memberSlots.push_back({{1},0});raid.counts[0]=1;
 raid.ChangeMembersGroup({1},4);assert(bot.subgroup==4&&bot.original==7&&raid.counts[0]==0&&raid.counts[4]==1&&raid.GetMemberGroup({1})==4);
 raid.ChangeMembersGroup({1},4);assert(raid.counts[4]==1);assert(CharacterDatabase.writes==0);
 Group background;bot.active=&background;raid.ChangeMembersGroup({1},2);assert(bot.subgroup==4&&bot.original==2&&raid.counts[2]==1&&raid.counts[4]==0);
 raid.ChangeMembersGroup({99},1);assert(CharacterDatabase.writes==0);
 std::cout<<"PASS: actual native subgroup changes update active NPCBot references, preserve background raid references and avoid database writes for temporary raids\n";
}
'''.replace('METHOD',method)
with tempfile.TemporaryDirectory(prefix='city-groups-') as tmp:
 src=Path(tmp)/'test.cpp';exe=Path(tmp)/'test';src.write_text(code)
 subprocess.run(['c++','-std=c++20','-O2','-Wall','-Wextra','-Werror',str(src),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
