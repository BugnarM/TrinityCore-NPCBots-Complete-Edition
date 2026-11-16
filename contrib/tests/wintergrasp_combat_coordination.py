#!/usr/bin/env python3
"""Execute production cast reservations across multiple bots and battle lifecycles."""
from pathlib import Path
import subprocess
import tempfile
repo=Path(__file__).resolve().parents[2]
source=(repo/'src/server/scripts/Northrend/zone_wintergrasp.cpp').read_text()
def function(signature):
    start=source.index(signature);end=source.index('{',start)+1;depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]
code=r'''
#include "src/server/scripts/Northrend/wintergrasp_tactics.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>
#include <map>
using uint32=std::uint32_t;using ObjectGuid=uint32;using SpellCastResult=int;
using namespace WintergraspBotWar;
enum{TEAM_ALLIANCE,TEAM_HORDE};
enum{UNIT_STATE_CONTROLLED=1,UNIT_STATE_FLEEING=2,UNIT_STATE_CONFUSED=4};
enum{SPELL_CAST_OK,SPELL_FAILED_NO_POWER};
int WGFactionTable[]={1732,1735};
struct Creature;using Unit=Creature;
struct Creature
{
 uint32 guid,state=0;int faction=1732,result=SPELL_CAST_OK;unsigned casts=0;Unit* victim=nullptr;
 explicit Creature(uint32 id):guid(id){}
 uint32 GetGUID() const{return guid;}int GetFaction() const{return faction;}
 Unit* GetVictim(){return victim;}bool HasUnitState(uint32 mask) const{return (state&mask)!=0;}
 int CastSpell(Unit*,uint32,bool){++casts;return result;}
};
struct BotCombatFocus{ObjectGuid Target=0;uint32 Until=0;};
std::array<std::array<BotCombatFocus,LaneCount>,2> BotFocus;
std::map<ObjectGuid,uint32> BotInterrupts,BotControls;
uint32 BotCombatSerial=0,BotCombatPrune=0;
bool nearbyBreakable=false;
bool HasBreakableControlNear(Unit*,Unit*,float){return nearbyBreakable;}
RESERVATIONS
INTERRUPT
CONTROL
AVAILABLE
RECORD
struct Bot
{
 Creature* me;explicit Bot(Creature* c):me(c){}
CAST
};
int main()
{
 BotBattleStats.Start(1,0);Creature a(1),b(2),enemy(3),other(4);a.victim=&enemy;b.victim=&enemy;
 Bot first(&a),second(&b);uint32 x=0,y=0;
 assert(first.CastAbility(&enemy,2139,x,24000)&&a.casts==1&&BotBattleStats.Teams[0].Interrupts==1);
 assert(!second.CastAbility(&enemy,57994,y,6000)&&b.casts==0&&y==500);
 BotBattleStats.Elapsed=1499;assert(!second.CastAbility(&enemy,57994,y,6000));
 BotBattleStats.Elapsed=1500;assert(second.CastAbility(&enemy,57994,y,6000)&&b.casts==1);
 a.result=SPELL_FAILED_NO_POWER;
 assert(!first.CastAbility(&other,2139,x,24000)&&!BotInterrupts.count(other.guid));
 assert(second.CastAbility(&other,57994,y,6000)); // failures do not reserve somebody else's interrupt
 a.result=SPELL_CAST_OK;enemy.state=UNIT_STATE_FLEEING;
 assert(!first.CastAbility(&enemy,6215,x,8000)&&!BotControls.count(enemy.guid));
 enemy.state=0;nearbyBreakable=true;
 assert(!first.CastAbility(&a,42917,x,15000)); // self-area CC respects other allies' breakable control
 nearbyBreakable=false;assert(first.CastAbility(&a,42917,x,15000));
 assert(!second.CastAbility(&enemy,10308,y,60000));
 BotBattleStats.Elapsed=20000;UpdateCombatReservations();assert(BotControls.empty()&&BotInterrupts.empty());
 BotBattleStats.Start(2,1);assert(second.CastAbility(&enemy,57994,y,6000));
 assert(BotControls.empty()&&BotCombatSerial==2&&BotBattleStats.Teams[0].Interrupts==1);
 std::cout<<"PASS: actual multi-bot interrupt/control reservations, failure release, area-CC protection, expiry and battle resets\n";
}
'''
for marker,sig in [('RESERVATIONS','void UpdateCombatReservations('),('INTERRUPT','bool IsInterruptAbility('),('CONTROL','bool IsControlAbility('),('AVAILABLE','bool BotAbilityAvailable('),('RECORD','void RecordBotAbility('),('CAST','    bool CastAbility(')]:
    code=code.replace('\n'+marker+'\n','\n'+function(sig)+'\n')
with tempfile.TemporaryDirectory(prefix='wg-combat-coordination-') as temp:
    cpp,binary=Path(temp)/'test.cpp',Path(temp)/'test';cpp.write_text(code)
    subprocess.run(['c++','-std=c++20','-O2','-Wall','-Wextra','-Werror','-I',str(repo),str(cpp),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
