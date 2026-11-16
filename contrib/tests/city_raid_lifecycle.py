#!/usr/bin/env python3
"""Execute actual coordinator update, claim and token-budget code without a running server."""
from pathlib import Path
import subprocess,tempfile
repo=Path(__file__).resolve().parents[2];s=(repo/'src/server/scripts/Events/city_raids.cpp').read_text()
def block(signature,start=0):
 a=s.index(signature,start);b=s.index('{',a)+1;n=1
 while n:n+=(s[b]=='{')-(s[b]=='}');b+=1
 return s[a:b]
update=block('    void Update(uint32 diff)',s.index('class CityRaidInstance'))
claim=block('    void Claim(Player* player, ObjectGuid chest)',s.index('class CityRaidInstance'))
runtime=block('struct RaidRuntime')+';'
code=r'''
#include "src/server/game/AI/NpcBots/botcityraid.h"
#include <atomic>
#include <array>
#include <cassert>
#include <iostream>
#include <memory>
#include <set>
#include <thread>
#include <vector>
using uint32=std::uint32_t;using uint64=std::uint64_t;using int32=std::int32_t;using ObjectGuid=unsigned;
namespace Tactics=NPCBots::CityRaid;using State=Tactics::State;
struct dtNavMeshQuery{};
RUNTIME
struct Position{};
struct GameObject{};
struct Player;
struct Map{GameObject chest;GameObject* GetGameObject(unsigned id){return id==7?&chest:nullptr;}};
struct Bot{bool IsAlive()const{return false;}};
using Creature=Bot;
struct AIType{bool Retreated()const{return true;}};
struct Member{};
struct Config{
 Position GraveyardPos,DefenderGraveyardPos;unsigned MapId=0;
 int GetIntDefault(char const* key,int value){return std::string(key)=="CityRaid.Reward.Gold"?10:value;}
}cfg;
auto* sConfigMgr=&cfg;
using ItemPosCountVec=std::vector<int>;using InventoryResult=int;
enum{EQUIP_ERR_OK,NULL_BAG=255,NULL_SLOT=255};constexpr unsigned MAX_MONEY_AMOUNT=2147483647;
struct Session{unsigned notices=0;template<class... Args>void SendNotification(Args...){++notices;}};
struct Player{
 unsigned guid=1,money=0,items=0,saves=0,errors=0;bool online=true,bagFull=false,storeFail=false,near=true,session=true;Session sess;
 bool IsInWorld()const{return online;}Session* GetSession(){return session?&sess:nullptr;}
 unsigned GetGUID()const{return guid;}unsigned GetMapId()const{return 0;}bool IsWithinDistInMap(GameObject*,float)const{return near;}
 unsigned GetMoney()const{return money;}
 int CanStoreNewItem(int,int,ItemPosCountVec&,unsigned,unsigned)const{return bagFull?1:0;}
 void SendEquipError(int,void*,void*,unsigned){++errors;}
 bool StoreNewItem(ItemPosCountVec&,unsigned,unsigned count,bool){if(storeFail)return false;items+=count;return true;}
 void ModifyMoney(int32 n){money+=n;}void SaveToDB(){++saves;}
};
struct Instance{
 State _state=State::Idle;uint32 _cooldown=1000,_elapsed=0,_sinceDecision=0,_decision=0,_duration=180000,_finish=90000;
 uint32 _attackWave=20000,_defendWave=30000,_sampleAt=1000;
 unsigned stops=0,attackWaves=0,defendWaves=0,bossCalls=0;
 std::shared_ptr<RaidRuntime> _runtime=std::make_shared<RaidRuntime>();
 std::vector<Member> _attackers,_defenders;std::array<Position,2> _graveyards;Config _cfg;std::set<unsigned> _eligible{1},_claimed;unsigned _chest=7;Map map;bool mapExists=true;
 bool Active()const{return _state!=State::Idle;}Map* GetMap(){return mapExists?&map:nullptr;}
 void Stop(){++stops;_state=State::Idle;}void BeginRetreat(){_state=State::Retreating;_finish=90000;}
 void SpawnBatch(Map*){_state=State::Marching;}void SampleParticipants(Map*){}
 void MarchOrders(Map*){}void DefenderOrders(Map*){}void BossOrders(Map*){++bossCalls;}
 Bot* GetBot(Map*,Member const&){return nullptr;}AIType* AI(Bot*){return nullptr;}
 void Resurrect(Map*,std::vector<Member>& members,bool wave,Position const&){if(wave)(&members==&_attackers?attackWaves:defendWaves)++;}
UPDATE
CLAIM
};
int main(){
 Instance idle;idle.Update(1100);assert(!idle._cooldown); // actual idle cooldown path
 Instance raid;raid._state=State::Marching;
 while(raid._elapsed<65000)raid.Update(1+(raid._elapsed*7)%901);
 assert(raid.attackWaves==3&&raid.defendWaves==2); // waves reflect elapsed time, including lag spikes
 Instance missing;missing._state=State::Marching;missing.mapExists=false;missing.Update(1000);assert(missing.stops==1);
 Instance expired;expired._state=State::Marching;expired._duration=1;expired.Update(500);assert(expired._state==State::Retreating||expired.stops==1);
 Instance rewards;rewards._state=State::Celebrating;Player p;
 rewards.Claim(&p,99);assert(!p.items&&!p.money);
 p.guid=2;rewards.Claim(&p,7);assert(!p.items);p.guid=1;
 p.bagFull=true;rewards.Claim(&p,7);assert(!p.items&&!p.money&&!rewards._claimed.count(1)&&p.errors==1);
 p.bagFull=false;p.storeFail=true;rewards.Claim(&p,7);assert(!p.items&&!rewards._claimed.count(1));
 p.storeFail=false;p.money=MAX_MONEY_AMOUNT-50000;rewards.Claim(&p,7);assert(!p.items&&!rewards._claimed.count(1));
 p.money=0;rewards.Claim(&p,7);rewards.Claim(&p,7);assert(p.items==2&&p.money==100000&&p.saves==1);
 rewards.Claim(nullptr,7);
 RaidRuntime budget;budget.PathTokens=8;std::atomic<unsigned> permitted{0};std::vector<std::thread> threads;
 for(unsigned i=0;i<64;++i)threads.emplace_back([&]{if(budget.TakePathToken())++permitted;});
 for(auto& t:threads) { t.join(); }
 assert(permitted==8&&budget.PathTokens==0);
 std::cout<<"PASS: actual idle cooldown, irregular-tick 20/30s waves, null map cleanup, retreat timeout, eligible one-time reward/bag/cap checks and atomic concurrent movement budget\n";
}
'''.replace('RUNTIME',runtime).replace('UPDATE',update).replace('CLAIM',claim)
with tempfile.TemporaryDirectory(prefix='city-raid-lifecycle-') as tmp:
 cpp=Path(tmp)/'test.cpp';binary=Path(tmp)/'test';cpp.write_text(code)
 subprocess.run(['c++','-std=c++20','-O2','-Wall','-Wextra','-Werror','-pthread','-I',str(repo),str(cpp),'-o',str(binary)],check=True)
 subprocess.run([str(binary)],check=True)
