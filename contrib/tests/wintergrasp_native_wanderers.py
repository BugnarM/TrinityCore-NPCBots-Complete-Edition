#!/usr/bin/env python3
"""Exercise the actual native/controller bridge, explicit attacks, and SQL graph constraints."""
from pathlib import Path
import re, subprocess, tempfile
repo=Path(__file__).resolve().parents[2]
h=(repo/'src/server/game/AI/NpcBots/bot_ai.h').read_text()
c=(repo/'src/server/game/AI/NpcBots/bot_ai.cpp').read_text()
z=(repo/'src/server/scripts/Northrend/zone_wintergrasp.cpp').read_text()
def function(text,signature):
 start=text.index(signature);end=text.index('{',start)+1;depth=1
 while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
 return text[start:end].replace(' override','')
methods='\n'.join(function(h,sig) for sig in ['    void SetBattlefieldAI(','    CreatureAI* GetBattlefieldAI(','    bool IsBattlefieldBot(','    uint32 GetData(','    void SetData(','    void DoAction('])
attack=function(c,'void bot_ai::AttackStart(')
code=r'''
#include <cassert>
#include <cstdint>
#include <iostream>
#include <memory>
using uint32=std::uint32_t;using int32=std::int32_t;
enum {BOT_COMMAND_COMBATRESET=8};
struct Unit{};
struct CreatureAI {virtual ~CreatureAI()=default;virtual uint32 GetData(uint32)const{return 0;}
 virtual void SetData(uint32,uint32){}virtual void DoAction(int32){}virtual bool CanAIAttack(Unit const*)const{return true;}};
struct Creature {int attacks=0;bool melee=false;Unit* victim=nullptr;void Attack(Unit* u,bool m){++attacks;victim=u;melee=m;}};
struct bot_ai:CreatureAI {
 Creature* me;Unit *opponent=nullptr,*disttarget=nullptr;uint32 command=0;bool ranged=false;
 std::unique_ptr<CreatureAI> _battlefieldAI;
 std::unique_ptr<CreatureAI> _cityRaidAI;
 explicit bot_ai(Creature* c):me(c){}
 void AttackStart(Unit*);
 bool CanBotAttack(Unit const* u)const{return _battlefieldAI&&_battlefieldAI->CanAIAttack(u);}
 bool IsRanged()const{return ranged;}void SetBotCommandState(uint32 st){command=st;}
METHODS
};
ATTACK
struct Controller:CreatureAI {
 uint32 credits=0;int actions=0;bool gateOpen=true;static inline int retired=0;
 ~Controller()override{++retired;}
 uint32 GetData(uint32 id)const override{return id==80501?credits:0;}
 void SetData(uint32 id,uint32 value)override{if(id==80501)credits=value;}
 void DoAction(int32 a)override{actions+=a;}
 bool CanAIAttack(Unit const*)const override{return gateOpen;}
};
int main(){
 Creature body;bot_ai native(&body);Unit enemy;
 native.AttackStart(&enemy);assert(!body.attacks&&!native.IsBattlefieldBot());
 auto controller=std::make_unique<Controller>();auto* tactical=controller.get();
 native.SetBattlefieldAI(std::move(controller));assert(native.GetBattlefieldAI()==tactical);
 native.SetData(80501,5);native.DoAction(1);assert(native.GetData(80501)==5&&tactical->actions==1);
 tactical->gateOpen=false;native.AttackStart(&enemy);assert(!body.attacks);
 tactical->gateOpen=true;native.AttackStart(&enemy);
 assert(body.attacks==1&&body.victim==&enemy&&body.melee&&native.opponent==&enemy&&native.disttarget==&enemy);
 native.ranged=true;native.AttackStart(&enemy);assert(!body.melee&&native.command==BOT_COMMAND_COMBATRESET);
 native.SetBattlefieldAI(nullptr);assert(Controller::retired==1&&!native.IsBattlefieldBot()&&native.GetData(80501)==0);
 std::cout<<"PASS: native class AI owns the controller, forwards ranks/orders, starts explicit melee/ranged combat, respects barriers and releases controller safely\n";
}
'''.replace('METHODS',methods).replace('ATTACK',attack)
with tempfile.TemporaryDirectory(prefix='wg-native-bridge-') as temp:
 cpp=Path(temp)/'test.cpp';binary=Path(temp)/'test';cpp.write_text(code)
 subprocess.run(['c++','-std=c++20','-O2','-Wall','-Wextra','-Werror',str(cpp),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
# Identity must come from native SQL prototypes; do not hide a scripted fallback behind a new name.
spawn=function(z,'    void SpawnSoldier(')
assert 'SpawnBattlefieldBot' in spawn and 'SetBattlefieldAI' in spawn
assert 'SetName(' not in spawn and 'map->SummonCreature' not in spawn
assert 'DespawnWandererBot' in function(z,'void RetireBotWarSoldier(')
assert '_battlefieldAI->JustDied(u)' in function(c,'void bot_ai::JustDied(')
assert '_battlefieldAI->KilledUnit(u)' in function(c,'void bot_ai::KilledUnit(')
assert 'if (_battlefieldAI || _cityRaidAI) return;' in function(c,'void bot_ai::UpdateReviveTimer(')
sql=(repo/'sql/custom/world/npcbot_2026_11_09_01_wintergrasp_native_bots.sql').read_text()
rows=re.findall(r"\((600\d+), '[^']+', 571, 4197, 4197, 80, 80, 2000, 7000, 0, (\d+), [^\n]+, '([^']+)'\)",sql)
assert len(rows)==34
edges={int(i):[int(link.split(':')[0]) for link in links.split()] for i,flag,links in rows}
assert all(int(flag)&1==0 for _,flag,_ in rows) # no unattended army from global wanderer generator
assert all(len(links)>=2 and all(n in edges for n in links) for links in edges.values())
# The tactical controller uses live, gated route checkpoints; these links initialize native wanderers.
print('PASS: 34 linked level-80 Wintergrasp SQL nodes, native named prototype spawning, native death/kill forwarding, roster-owned resurrection and pool retirement')
