#!/usr/bin/env python3
"""Compile and exercise the actual World defense-queue methods under concurrent producers."""
from pathlib import Path
import subprocess,tempfile
repo=Path(__file__).resolve().parents[2]
s=(repo/'src/server/game/World/World.cpp').read_text()
def block(signature):
 a=s.index(signature);b=s.index('{',a)+1;n=1
 while n:n+=(s[b]=='{')-(s[b]=='}');b+=1
 return s[a:b]
code=r'''
#include <cassert>
#include <cstdint>
#include <ctime>
#include <map>
#include <mutex>
#include <set>
#include <thread>
#include <vector>
#include <iostream>
using uint32=std::uint32_t;
constexpr uint32 ALLIANCE=469,HORDE=67,SMSG_ZONE_UNDER_ATTACK=1;
namespace GameTime{time_t now=100;time_t GetGameTime(){return now;}}
struct WorldPacket {uint32 area=0;WorldPacket(uint32,uint32){}WorldPacket& operator<<(uint32 a){area=a;return *this;}};
struct World {
 std::mutex _zoneAttackLock;
 std::set<std::pair<uint32,uint32>> _zoneAttackQueue;
 std::map<std::pair<uint32,uint32>,time_t> _zoneAttackCooldowns;
 std::vector<std::pair<uint32,uint32>> sent;
 void QueueZoneUnderAttack(uint32,uint32);void SendQueuedZoneUnderAttack();
 void SendGlobalMessage(WorldPacket const* p,void*,uint32 team){sent.emplace_back(p->area,team);}
};
QUEUE
DRAIN
int main(){
 World w;w.QueueZoneUnderAttack(0,HORDE);w.QueueZoneUnderAttack(1519,0);assert(w._zoneAttackQueue.empty());
 std::vector<std::thread> workers;
 for(unsigned i=0;i<32;++i)workers.emplace_back([&]{for(unsigned j=0;j<1000;++j){w.QueueZoneUnderAttack(1519,HORDE);w.QueueZoneUnderAttack(1519,ALLIANCE);}});
 for(auto& t:workers){t.join();}assert(w.sent.empty());assert(w._zoneAttackQueue.size()==2);
 w.SendQueuedZoneUnderAttack();assert(w.sent.size()==2&&w._zoneAttackCooldowns.size()==2);
 w.QueueZoneUnderAttack(1519,HORDE);w.SendQueuedZoneUnderAttack();assert(w.sent.size()==2);
 GameTime::now=159;w.QueueZoneUnderAttack(1519,HORDE);w.SendQueuedZoneUnderAttack();assert(w.sent.size()==2);
 GameTime::now=160;w.QueueZoneUnderAttack(1519,HORDE);w.SendQueuedZoneUnderAttack();assert(w.sent.size()==3);
 for(unsigned area=1;area<1000;++area)w.QueueZoneUnderAttack(area,HORDE);
 assert(w._zoneAttackQueue.size()==256);w.SendQueuedZoneUnderAttack();assert(w._zoneAttackQueue.empty());
 std::cout<<"PASS: 32 concurrent defense-alert producers, world-thread-only broadcasts, per-area/per-faction 60s throttle, deduplication and bounded queue\n";
}
'''.replace('QUEUE',block('void World::QueueZoneUnderAttack')).replace('DRAIN',block('void World::SendQueuedZoneUnderAttack'))
with tempfile.TemporaryDirectory(prefix='city-defense-') as tmp:
 src=Path(tmp)/'test.cpp';exe=Path(tmp)/'test';src.write_text(code)
 subprocess.run(['c++','-std=c++20','-O2','-Wall','-Wextra','-Werror','-pthread',str(src),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
