#!/usr/bin/env python3
"""Run the real living-target guard with invalid maps/phases, duels and ambient stealth."""
from pathlib import Path
import subprocess
import sys
import os
import tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'src/server/game/AI/NpcBots/bot_ai.cpp').read_text()
a=s.index('bool bot_ai::CanEvaluateHealTarget(');b=s.index('bool bot_ai::TryHealCandidates',a)
method=s[a:b]
code=r'''
#include <cassert>
#include <iostream>
struct Player{bool duel=false;};
constexpr unsigned UNIT_STATE_ISOLATED=1,FORM_SPIRITOFREDEMPTION=32;
struct Unit{
 bool world=true,alive=true,phase=true,isolated=false,visible=true,assist=true,stealth=false,invisible=false;unsigned map=1,form=0;float distance=10;Player* player=nullptr;
 bool IsPlayer()const{return player!=nullptr;}bool IsInWorld()const{return world;}bool IsAlive()const{return alive;}unsigned FindMap()const{return map;}
 bool InSamePhase(Unit const*)const{return phase;}bool HasUnitState(unsigned)const{return isolated;}unsigned GetShapeshiftForm()const{return form;}
 bool IsWithinDistInMap(Unit const* u,float range)const{return map==u->map&&u->distance<=range;}
 bool CanSeeOrDetect(Unit const* u)const{return u->visible;}bool IsValidAssistTarget(Unit const* u)const{return u->assist;}
 bool HasStealthAura()const{return stealth;}bool HasInvisibilityAura()const{return invisible;}Player const* GetAffectingPlayer()const{return player;}
};
struct BotCfg{struct Settings{bool playerAid=true,allowStealthedAid=false,allowDuelAid=false;float aidRange=40;};static Settings& GetHealingSettings(){static Settings s;return s;}};
struct bot_ai{Unit self;Unit* me=&self;bool free=true;bool IAmFree()const{return free;}bool CanEvaluateHealTarget(Unit const*)const;};
METHOD
int main(){
 bot_ai ai;Unit target;Player player;target.player=&player;
 assert(ai.CanEvaluateHealTarget(&target));assert(!ai.CanEvaluateHealTarget(nullptr));
 target.world=false;assert(!ai.CanEvaluateHealTarget(&target));target.world=true;
 target.map=2;assert(!ai.CanEvaluateHealTarget(&target));target.map=1;
 target.phase=false;assert(!ai.CanEvaluateHealTarget(&target));target.phase=true;
 target.alive=false;assert(!ai.CanEvaluateHealTarget(&target));target.alive=true;
 target.isolated=true;assert(!ai.CanEvaluateHealTarget(&target));target.isolated=false;
 target.form=FORM_SPIRITOFREDEMPTION;assert(!ai.CanEvaluateHealTarget(&target));target.form=0;
 target.assist=false;assert(!ai.CanEvaluateHealTarget(&target));target.assist=true;
 target.distance=41;assert(!ai.CanEvaluateHealTarget(&target));target.distance=10;
 player.duel=true;assert(!ai.CanEvaluateHealTarget(&target));player.duel=false;
 ai.self.stealth=true;assert(!ai.CanEvaluateHealTarget(&target));assert(ai.CanEvaluateHealTarget(ai.me));ai.self.stealth=false;
 BotCfg::GetHealingSettings().playerAid=false;assert(!ai.CanEvaluateHealTarget(&target));target.player=nullptr;assert(ai.CanEvaluateHealTarget(&target));target.player=&player;ai.free=false;assert(ai.CanEvaluateHealTarget(&target));
 std::cout<<"PASS: production target guard rejects cross-map/phase/isolated/dead/dueling/invisible ambient targets\n";
}
'''.replace('METHOD',method)
with tempfile.TemporaryDirectory(prefix='healer-targets-') as temp:
 cpp=Path(temp)/'test.cpp';exe=Path(temp)/'test';cpp.write_text(code)
 subprocess.run(['c++','-std=c++20','-O2','-Wall','-Wextra','-Werror',*(['-fsanitize=address,undefined','-fno-omit-frame-pointer'] if '--sanitize' in sys.argv else []),str(cpp),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env=os.environ | ({'ASAN_OPTIONS':'detect_leaks=0'} if '--sanitize' in sys.argv else {}))
