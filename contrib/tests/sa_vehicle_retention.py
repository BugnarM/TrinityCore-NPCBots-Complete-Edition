#!/usr/bin/env python3
"""Execute production driver retention, cannon filtering and mount cooldown code."""
from pathlib import Path
import subprocess
import tempfile
repo=Path(__file__).resolve().parents[2]
source=(repo/'src/server/game/AI/NpcBots/bot_battlegrounds.cpp').read_text()
common=(repo/'src/server/game/AI/NpcBots/bot_ai.cpp').read_text()
def function(text,signature):
    start=text.index(signature);end=text.index('{',start)+1;depth=1
    while depth:depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]
humans=function(source,'bool HumanNear(')+'\n'+function(source,'bool HumanRequestsVehicle(')
boarding=function(source,'bool bot_ai::TrySiegeVehicle(').replace('bot_ai::','Controller::')
start=source.index('void bot_ai::UpdateSiegeVehicle(')
retention=source[start:source.index('    if (!CanBotMoveVehicle())',start)].replace('bot_ai::','Controller::')+'    ++drives;\n}\n'
start=source.index('            if (!vehicle->IsAlive() || vehicle->GetHealthPct()')
candidate=source[start:source.index('            if (kit->HasEmptySeat(0))',start)]
start=common.index('uint32 bot_ai::_selectMountSpell() const')
mount=common[start:common.index('    uint8 minLevel60',start)].replace('bot_ai::_selectMountSpell','Controller::SelectMount')+'    return 1;\n}\n'
start=source.index('    Unit* mover =',source.index('bool bot_ai::MoveToSiegeObjective('))
near=source[start:source.index('    if (mover->GetExactDist(&destination) <= radius)',start)]
start=source.index('    auto tick =',source.index('bool bot_ai::UpdateSiegeBattleground('))
tick=source[start:source.index('    bool saMap =',start)]
reset=source[source.index('    _siegeThinkTimer =',source.index('void bot_ai::ResetSiegeBattlegroundState(')):source.index('    _siegeSARole =',source.index('void bot_ai::ResetSiegeBattlegroundState('))]
code=r'''
#include "src/server/game/AI/NpcBots/botbgtactics.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>
#include <list>
#include <map>
using uint32=std::uint32_t;using uint8=std::uint8_t;using int8=std::int8_t;
using namespace NPCBots::SiegeBG;
enum {UNIT_FLAG_UNINTERACTIBLE=1,BATTLEGROUND_SA=9,BOT_ROLE_HEAL=1,
 NPC_ANTI_PERSONNAL_CANNON=27894,NPC_DEMOLISHER_SA=28781,NPC_KEEP_CANNON=34944,
 NPC_GLAIVE_THROWER_A=34802,NPC_GLAIVE_THROWER_H=35273,NPC_CATAPULT=34793,NPC_DEMOLISHER=34775,NPC_SIEGE_ENGINE_A=34776,NPC_SIEGE_ENGINE_H=35069};float constexpr INTERACTION_DISTANCE=5.f;
struct Guid {unsigned value=0;unsigned GetCounter()const{return value;}operator unsigned()const{return value;}};
struct Position {
 union{float x,m_positionX;};union{float y,m_positionY;};union{float z,m_positionZ;};
 Position(float a=0,float b=0,float c=0):x(a),y(b),z(c){}
 float GetExactDist(Position const* p)const{return std::sqrt((x-p->x)*(x-p->x)+(y-p->y)*(y-p->y)+(z-p->z)*(z-p->z));}
 Position const& GetPosition()const{return *this;}float GetPositionZ()const{return z;}
};
struct Player;struct Map {struct Ref{Player* player;Player* GetSource()const{return player;}};std::vector<Ref> players;auto const& GetPlayers()const{return players;}} world;
struct Unit;struct SeatInfoData{bool CanEnterOrExit()const{return true;}};
struct SeatData{SeatInfoData* SeatInfo=nullptr;};
struct Kit {bool empty=true;std::map<int,SeatData> Seats;bool HasEmptySeat(unsigned)const{return empty;}Unit* GetPassenger(int){return nullptr;}};
using Vehicle=Kit;
struct Motion {unsigned moves=0;Position goal;void MovePoint(unsigned,Position const& p,bool){goal=p;++moves;}
 bool GetDestination(float& x,float& y,float& z){x=goal.x;y=goal.y;z=goal.z;return moves;}};
struct BattlegroundSA;
struct Unit:Position {
 Guid guid{200};float health=100;bool alive=true,friendly=true,unselectable=false;unsigned phase=1,stops=0,exits=0;Unit* vehicle=nullptr;BattlegroundSA* bg=nullptr;Kit kit;
 bool reachable=true,combat=false,casting=false,mounted=false,moving=false;unsigned entry=28781,m_spells[4]={};Motion motion;Unit* pending=nullptr;
 Map* GetMap(){return &world;}Guid GetGUID()const{return guid;}bool IsAlive()const{return alive;}
 bool IsFriendlyTo(Unit const*)const{return friendly;}bool IsHostileTo(Unit const*)const{return !friendly;}
 bool InSamePhase(Unit const* other)const{return phase&other->phase;}
 bool IsWithinDistInMap(Unit const* p,float distance)const{return GetExactDist(p)<=distance;}
 bool HasUnitFlag(unsigned)const{return unselectable;}float GetHealthPct()const{return health;}
 Kit* GetVehicleKit(){return &kit;}bool GetVehicle()const{return vehicle;}
 Unit* GetVehicleBase(){return vehicle;}Unit* GetVehicleCreatureBase(){return vehicle;}
 bool IsInCombat()const{return combat;}Unit* ToCreature(){return this;}
 Motion* GetMotionMaster(){return &motion;}bool isMoving()const{return moving;}
 void CombatStop(bool){combat=false;}void EnterVehicle(Unit* v,int8){pending=v;}
 void FinishJoin(){vehicle=pending;pending=nullptr;if(vehicle)vehicle->kit.empty=false;}
 void GetCreatureListWithEntryInGrid(std::list<Unit*>&,unsigned,float);
 void ExitVehicle(){++exits;vehicle=nullptr;}void StopMoving(){++stops;}
};
using Creature=Unit;struct GameObject{};
std::vector<Creature*> available;
void Unit::GetCreatureListWithEntryInGrid(std::list<Unit*>& result,unsigned id,float radius){for(auto* v:available)if(v->entry==id&&GetExactDist(v)<=radius)result.push_back(v);}
struct Player:Unit {Guid target;Guid GetTarget()const{return target;}};
struct BattlegroundSA {unsigned mask=0;Creature guns[10];unsigned GetTypeID()const{return 9;}Creature* GetBGCreature(unsigned i,bool){return i<10?&guns[i]:nullptr;}};
unsigned GateMask(BattlegroundSA* bg){return bg->mask;}
bool SAPathFromClear(Unit*,BattlegroundSA*,Position const&,Position const& to){for(auto* vehicle:available)if(&to==static_cast<Position const*>(vehicle))return vehicle->reachable;return true;}
HUMANS
struct Controller {
 Creature* me;unsigned drives=0,_siegeBoardTimer=0,_siegeMountTimer=0,_siegeThinkTimer=0,_siegeWeaponTimer=0,_siegeBombTimer=0,_siegeTravelNode=0;bool siege=true;
 bool _siegeBombCarrier=false,healer=false;
 bool IsCasting(){return me->casting;}bool HasRole(unsigned){return healer;}
 void DismountBot(){me->mounted=false;}void removeShapeshiftForm(){}
 bool TrySiegeVehicle(bool,uint8);
 BattlegroundSA* GetBG(){return me->bg;}bool CanBotMoveVehicle(){return true;}bool IsSiegeBattlegroundBot()const{return siege;}
 void UpdateSiegeVehicle(uint32,GameObject*,Position const&,uint32,bool);
 uint32 SelectMount()const;
 void HoldNear(Position const& destination){
NEAR
 }
 void Tick(uint32 diff){
TICK
 }
 void Reset(){
RESET
 }
 bool Candidate(Creature* candidate,bool sa,bool defender){
  for(Creature* vehicle:{candidate}){Kit* kit=vehicle->GetVehicleKit();
CANDIDATE
   return kit->HasEmptySeat(0);
  }return false;
 }
};
RETENTION
MOUNT
BOARDING
void Check(bool b,char const* why){if(!b){std::cerr<<why<<'\n';std::exit(1);}}
int main(){
 BattlegroundSA bg;Creature tank;Creature bot;bot.bg=&bg;bot.vehicle=&tank;Controller ai{&bot};Position goal{100,0,0};
 Player human;human.x=2;world.players.push_back({&human});
 for(unsigned tick=0;tick<120;++tick)ai.UpdateSiegeVehicle(1000,nullptr,goal,0,true);
 Check(bot.exits==0&&ai.drives==120,"healthy driver abandons vehicle for a passing human");
 human.target=tank.guid;human.x=6;Check(!HumanRequestsVehicle(&tank),"selected vehicle yielded outside interaction range");
 human.x=2;human.phase=2;Check(!HumanRequestsVehicle(&tank),"wrong-phase player requests vehicle");human.phase=1;
 human.alive=false;Check(!HumanRequestsVehicle(&tank),"dead player requests vehicle");human.alive=true;
 tank.friendly=false;Check(!HumanRequestsVehicle(&tank),"hostile player requests vehicle");tank.friendly=true;
 Check(HumanRequestsVehicle(&tank),"nearby player selecting vehicle cannot take priority");
 ai.UpdateSiegeVehicle(1000,nullptr,goal,0,true);Check(bot.exits==1&&ai._siegeBoardTimer==20000,"intentional human takeover not honored");
 world.players.clear();bot.vehicle=&tank;tank.health=14.9f;
 ai.UpdateSiegeVehicle(1000,nullptr,goal,0,true);Check(bot.exits==2,"critical vehicle not abandoned");
 ai.Tick(21000);Check(!ai.Candidate(&tank,true,false),"same critical vehicle reclaimed after cooldown");
 tank.health=15.f;Check(ai.Candidate(&tank,true,false),"usable repaired vehicle rejected");
 for(unsigned mask=0;mask<64;++mask)for(unsigned id:{1u,2u,6u,7u}){
  bg.mask=mask;bot.guid={id};auto gate=AssaultGate(mask,(id/5)%2==0);auto preferred=SACannonIndex(gate,id);
  for(unsigned gun=0;gun<10;++gun)Check(ai.Candidate(&bg.guns[gun],true,true)==(preferred<10&&(gun==preferred||gun==(preferred^1u))),"defender boards cannon of wrong/obsolete gate");
 }
 for(auto gate:{Green,Yellow,Blue,Red,Purple})Check(SACannonIndex(gate,1)<10&&SACannonIndex(gate,2)==(SACannonIndex(gate,1)^1u),"two gunners assigned the same turret");
 bot.vehicle=nullptr;bot.x=0;ai.Reset();
 for(unsigned tick=0;tick<120;++tick){ai.Tick(1000);ai.HoldNear({10,0,0});Check(ai.SelectMount()==0,"mount/dismount loop near siege objective");}
 ai.Tick(3000);Check(ai.SelectMount()==1,"mount remains disabled after leaving objective");
 ai._siegeBoardTimer=20000;for(unsigned tick=0;tick<19;++tick){ai.Tick(1000);Check(ai.SelectMount()==0,"mount started during vehicle handover cooldown");}
 ai.Tick(1000);Check(ai.SelectMount()==1,"vehicle cooldown never releases mounts");
 ai._siegeMountTimer=2500;ai.siege=false;Check(ai.SelectMount()==1,"siege mount rule affects normal companions");
 ai.siege=true;ai.Reset();Check(ai._siegeMountTimer==0&&ai._siegeBoardTimer==0,"round reset retains mount cooldown");
 bot.guid={0};bot.x=0;bot.vehicle=nullptr;available.clear();
 Check(!ai.TrySiegeVehicle(false,0)&&bot.motion.moves==0,"no vehicle leaves bot waiting for boarding");
 Creature remote;remote.x=200;remote.guid={999};available={&remote};
 Check(ai.TrySiegeVehicle(false,0)&&bot.motion.moves==1&&bot.motion.goal.x==200,"available vehicle at other beach is ignored");
 remote.reachable=false;ai.Reset();bot.motion.moves=0;
 Check(!ai.TrySiegeVehicle(false,0)&&bot.motion.moves==0,"unreachable vehicle monopolizes infantry orders");
 remote.reachable=true;remote.x=2;ai.Reset();
 Check(ai.TrySiegeVehicle(false,0)&&bot.vehicle==nullptr&&bot.pending==&remote&&ai._siegeBoardTimer==2000,"asynchronous VehicleJoinEvent is mistaken for failed boarding");
 Check(ai.SelectMount()==0,"mount interrupts pending vehicle join");bot.FinishJoin();
 Check(bot.vehicle==&remote,"scheduled join failed to attach");bot.vehicle=nullptr;remote.kit.empty=false;ai.Tick(3000);
 Check(!ai.TrySiegeVehicle(false,0),"taken vehicle keeps another bot waiting at beach");
 std::cout<<"PASS: production driver retention, explicit human takeover, critical-vehicle filter, both active-gate turrets, mount cooldowns, opposite-harbor search, unreachable/taken vehicle fallback and asynchronous boarding\n";
}
'''
for key,value in [('HUMANS',humans),('NEAR',near),('TICK',tick),('RESET',reset),('CANDIDATE',candidate),('RETENTION',retention),('MOUNT',mount),('BOARDING',boarding)]:code=code.replace(key,value)
with tempfile.TemporaryDirectory(prefix='sa-vehicle-retention-') as directory:
    cpp,exe=Path(directory)/'test.cpp',Path(directory)/'test';cpp.write_text(code)
    subprocess.run(['c++','-std=c++20','-O2','-Wall','-Wextra','-Wno-unused-parameter','-I',str(repo),str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
