#!/usr/bin/env python3
"""Exercise actual event scheduler: script-owned internal events must not be auto-stopped."""
from pathlib import Path
import subprocess,tempfile
repo=Path(__file__).resolve().parents[2];s=(repo/'src/server/game/Events/GameEventMgr.cpp').read_text()
a=s.index('uint32 GameEventMgr::Update()');b=s.index('{',a)+1;n=1
while n:n+=(s[b]=='{')-(s[b]=='}');b+=1
method=s[a:b]
code=r'''
#include <cassert>
#include <cstdint>
#include <ctime>
#include <iostream>
#include <set>
#include <vector>
using uint32=std::uint32_t;using uint16=std::uint16_t;using int16=std::int16_t;
enum{GAMEEVENT_NORMAL,GAMEEVENT_WORLD_INACTIVE,GAMEEVENT_WORLD_CONDITIONS,GAMEEVENT_WORLD_NEXTPHASE,GAMEEVENT_WORLD_FINISHED,GAMEEVENT_INTERNAL};
constexpr uint32 max_ge_check_delay=86400,IN_MILLISECONDS=1000;
namespace GameTime{time_t GetGameTime(){return 1000;}}
#define TC_LOG_INFO(...) (void)0
struct GameEventMgr{
 struct Event{unsigned state=GAMEEVENT_NORMAL;time_t nextstart=0;bool scheduled=false;};
 std::vector<Event> mGameEvent=std::vector<Event>(88);std::set<unsigned> active;bool isSystemInit=true;
 unsigned stopped=0,started=0,saved=0;std::set<unsigned> checked;
 bool CheckOneGameEvent(unsigned id){return mGameEvent[id].state==GAMEEVENT_INTERNAL?false:mGameEvent[id].scheduled;}
 bool IsActiveEvent(unsigned id){return active.count(id);}
 bool CheckOneGameEventConditions(unsigned){return false;}void SaveWorldEventStateToDB(unsigned){++saved;}
 void GameEventSpawn(int16){}unsigned NextCheck(unsigned id){checked.insert(id);return 60;}
 bool StartEvent(unsigned id){active.insert(id);++started;return false;}void StopEvent(unsigned id){active.erase(id);++stopped;}
 uint32 Update();
};
METHOD
int main(){
 GameEventMgr g;g.mGameEvent[86].state=g.mGameEvent[87].state=GAMEEVENT_INTERNAL;g.active={86,1};
 g.mGameEvent[2].scheduled=true; // a normal scheduled event still starts
 g.Update();assert(g.active.count(86)&&!g.active.count(87));assert(!g.checked.count(86)&&!g.checked.count(87));assert(!g.active.count(1)&&g.active.count(2));
 for(unsigned i=0;i<100;++i)g.Update();assert(g.active.count(86));
 g.StopEvent(86);g.Update();assert(!g.active.count(86)); // manual stop remains authoritative
 g.mGameEvent[3]={GAMEEVENT_WORLD_NEXTPHASE,900,true};g.active.insert(3);g.Update();assert(!g.active.count(3)&&g.saved==1&&g.mGameEvent[3].state==GAMEEVENT_WORLD_FINISHED);
 std::cout<<"PASS: actual global scheduler preserves active internal raids across 101 updates, leaves inactive internal raids alone, and keeps normal/world/manual-stop behavior\n";
}
'''.replace('METHOD',method).replace('for(unsigned i=0;i<100;++i)g.Update();','for(unsigned i=0;i<100;++i){g.Update();}')
with tempfile.TemporaryDirectory(prefix='city-internal-events-') as tmp:
 src=Path(tmp)/'test.cpp';exe=Path(tmp)/'test';src.write_text(code)
 subprocess.run(['c++','-std=c++20','-O2','-Wall','-Wextra','-Werror',str(src),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
