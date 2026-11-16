#!/usr/bin/env python3
"""A bot attacked before AttackStart must not evade relative to world origin."""
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
#include <cassert>
#include <cmath>
#include <iostream>
struct Position
{
 float x=4948.5f,y=3342.3f,z=377;
 float GetExactDist2d(Position const* other)const{return std::hypot(x-other->x,y-other->y);}
};
struct Creature:Position
{
 bool combat=false;Position home;
 bool IsInCombat()const{return combat;}
 Position const& GetPosition()const{return *this;}
 void SetHomePosition(Position const& pos){home=pos;}
};
struct Bot
{
 Creature* me;Position _combatOrigin{0,0,0};bool _combatOriginSet=false;
 explicit Bot(Creature* c):me(c){}
ORIGIN
 bool BeyondLeash()const{return me->IsInCombat()&&me->GetExactDist2d(&_combatOrigin)>100.f;}
};
int main()
{
 Creature defender;Bot ai(&defender);
 defender.combat=true;ai.UpdateCombatOrigin(); // enemy engaged us first; no AttackStart by us yet
 assert(ai._combatOriginSet&&!ai.BeyondLeash()&&defender.GetExactDist2d(&ai._combatOrigin)==0);
 defender.x+=80;ai.UpdateCombatOrigin();assert(!ai.BeyondLeash());
 defender.x+=30;ai.UpdateCombatOrigin();assert(ai.BeyondLeash()); // origin never drifts during combat
 defender.combat=false;ai.UpdateCombatOrigin();assert(!ai._combatOriginSet);
 defender.x+=400;ai.UpdateCombatOrigin();assert(defender.GetExactDist2d(&defender.home)==0);
 defender.combat=true;ai.UpdateCombatOrigin();assert(!ai.BeyondLeash());
 std::cout<<"PASS: defender engaged before AttackStart captures its real combat origin; fixed leash and new engagements remain valid\n";
}
'''.replace('ORIGIN',function('    void UpdateCombatOrigin('))
with tempfile.TemporaryDirectory(prefix='wg-combat-origin-') as temp:
    cpp,binary=Path(temp)/'test.cpp',Path(temp)/'test';cpp.write_text(code)
    subprocess.run(['c++','-std=c++20','-O2','-Wall','-Wextra','-Werror',str(cpp),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
