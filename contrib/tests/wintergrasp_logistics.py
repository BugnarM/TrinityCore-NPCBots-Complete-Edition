#!/usr/bin/env python3
"""Execute production build, rank, cast and commander code with simulated map state."""
from pathlib import Path
import subprocess
import tempfile
repo = Path(__file__).resolve().parents[2]
source = (repo / 'src/server/scripts/Northrend/zone_wintergrasp.cpp').read_text()
def function(text, signature):
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end].replace(' override', '')
code = r'''
#include "src/server/scripts/Northrend/wintergrasp_tactics.h"
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <list>
#include <map>
#include <vector>
using uint32=std::uint32_t;using uint8=std::uint8_t;
using namespace WintergraspBotWar;
enum TeamId { TEAM_ALLIANCE,TEAM_HORDE,TEAM_NEUTRAL };
enum { TYPEID_UNIT,TYPEID_PLAYER,BATTLEFIELD_WG_NPC_GUARD_A=30740,BATTLEFIELD_WG_NPC_GUARD_H=30739 };
enum { BATTLEFIELD_WG_WORKSHOP_SE,BATTLEFIELD_WG_WORKSHOP_SW,BATTLEFIELD_WG_WORKSHOP_NE,
 BATTLEFIELD_WG_WORKSHOP_NW,BATTLEFIELD_WG_WORKSHOP_KEEP_WEST,BATTLEFIELD_WG_WORKSHOP_KEEP_EAST };
enum { AREA_THE_BROKEN_TEMPLATE=1000,AREA_THE_SUNKEN_RING,AREA_WESTPARK_WORKSHOP,AREA_EASTPARK_WORKSHOP };
enum { NPC_GNOMISH_ENGINEER=100,NPC_GOBLIN_MECHANIC,NPC_WINTERGRASP_CONTROL_ARMS,
 NPC_WINTERGRASP_CATAPULT,NPC_WINTERGRASP_DEMOLISHER,NPC_WINTERGRASP_SIEGE_ENGINE_ALLIANCE,NPC_WINTERGRASP_SIEGE_ENGINE_HORDE };
enum { BATTLEFIELD_WG_DATA_VEHICLE_A,BATTLEFIELD_WG_DATA_VEHICLE_H,
 BATTLEFIELD_WG_DATA_MAX_VEHICLE_A,BATTLEFIELD_WG_DATA_MAX_VEHICLE_H };
enum { UNIT_STATE_CONTROLLED=1,UNIT_STATE_CASTING=2,GO_DESTRUCTIBLE_DESTROYED=1,
 TEMPSUMMON_MANUAL_DESPAWN,SPELL_ACTIVATE_CONTROL_ARMS,SPELL_RECRUIT,SPELL_CORPORAL,SPELL_LIEUTENANT };
enum { SPELL_CAST_OK,SPELL_FAILED_NO_POWER,SPELL_FAILED_OUT_OF_RANGE };
enum { POWER_MANA=0,POWER_RAGE=1,POWER_ENERGY=3,UNIT_FLAG2_REGENERATE_POWER=2048 };
using SpellCastResult=int;
#define TC_LOG_INFO(...) ((void)0)
int WGFactionTable[]={1732,1735};
struct ObjectGuid
{
 int value=0; ObjectGuid(int v=0):value(v){} void Clear(){value=0;} bool IsEmpty() const{return value==0;}
 static ObjectGuid const Empty;
 bool operator==(ObjectGuid const& other) const{return value==other.value;}
};
ObjectGuid const ObjectGuid::Empty{};
struct Position
{
 float m_positionX=0,m_positionY=0,m_positionZ=0,o=0;
 Position(float x=0,float y=0,float z=0,float angle=0):m_positionX(x),m_positionY(y),m_positionZ(z),o(angle){}
 float GetPositionX() const{return m_positionX;} float GetPositionY() const{return m_positionY;}
 float GetOrientation() const{return o;}
 float GetExactDist2d(Position const* p) const{return std::hypot(m_positionX-p->m_positionX,m_positionY-p->m_positionY);}
 Position const& GetPosition() const{return *this;}
};
struct Creature;struct Player;using Unit=Creature;
struct VehicleKit { Creature* driver=nullptr;Creature* GetPassenger(int){return driver;} };
struct AIBase { bool battlefield=false;AIBase* controller=nullptr;bool IsBattlefieldBot()const{return battlefield;}AIBase* GetBattlefieldAI()const{return controller;} virtual ~AIBase()=default; virtual uint32 GetData(uint32) const{return 0;} virtual void SetData(uint32,uint32){} };
struct Map
{
 std::map<int,Creature*> creatures;unsigned loads=0;
 Creature* GetCreature(ObjectGuid guid){auto i=creatures.find(guid.value);return i==creatures.end()?nullptr:i->second;}
 void LoadGrid(float,float){++loads;}
};
std::vector<Creature*> world;
struct Creature : Position
{
 struct Threat {unsigned clears=0;void ClearAllThreat(){++clears;}} threat;
 std::vector<Creature*> incoming;float health=100;
 auto const& getAttackers()const{return incoming;}bool HealthBelowPct(unsigned pct)const{return health<pct;}
 void CombatStop(bool){combat=false;}Threat& GetThreatManager(){return threat;}
 bool native=false;bool IsNPCBot()const{return native;}bool IsCreature()const{return GetTypeId()==TYPEID_UNIT;}Creature const* ToCreature()const{return this;}AIBase* GetBotAI()const{return ai;}
 uint32 phaseMask=1;
 uint32 entry=80501,mapId=571,zone=4197;int faction=1732;bool alive=true,combat=false,controlled=false,aboard=false,active=false,humanPassenger=false;
 ObjectGuid guid;AIBase* ai=nullptr;Map* map=nullptr;
 VehicleKit* kit=nullptr;VehicleKit* GetVehicleKit(){return kit;}
 unsigned powerType=POWER_MANA,flags2=0;bool dual=false;
 std::map<unsigned,unsigned> power,maxPower,items;
 void SetUnitFlag2(unsigned flag){flags2|=flag;}
 void SetPowerType(unsigned type){powerType=type;}
 void SetMaxPower(unsigned type,unsigned maximum){maxPower[type]=maximum;}
 void SetPower(unsigned type,unsigned value){power[type]=value;}
 void SetVirtualItem(unsigned slot,unsigned id){items[slot]=id;}
 void SetCanDualWield(bool enabled){dual=enabled;}
 std::map<uint32,uint32> auras;unsigned summons=0,casts=0;int castResult=SPELL_CAST_OK;
 Creature(Map& m,uint32 e=80501,Position p={}):Position(p),entry(e),map(&m)
 {if(e==NPC_GNOMISH_ENGINEER)phaseMask=32;if(e==NPC_GOBLIN_MECHANIC)phaseMask=16;static int next=1;guid=next++;m.creatures[guid.value]=this;world.push_back(this);}
 virtual Player* ToPlayer(){return nullptr;}
 virtual int GetTypeId() const{return TYPEID_UNIT;}
 uint32 GetMapId() const{return mapId;}uint32 GetZoneId() const{return zone;}
 bool IsAlive() const{return alive;}bool IsInCombat() const{return combat;}
 bool IsFriendlyTo(Creature const* c) const{return faction==c->faction;}
 int GetFaction() const{return faction;}uint32 GetEntry() const{return entry;}
 bool GetVehicle() const{return aboard;}bool HasUnitState(int) const{return controlled;}
 Creature* ToTempSummon(){return this;}AIBase* AI(){return ai;} ObjectGuid GetGUID() const{return guid;}
 void DespawnOrUnsummon(){alive=false;}void setActive(bool v){active=v;}
 void UpdateAllowedPositionZ(float,float,float&){}
 Creature* FindNearestCreature(uint32,float){return nullptr;}
 int CastSpell(Unit*,uint32,bool){++casts;return castResult;}
 void RemoveAurasDueToSpell(uint32 spell){auras.erase(spell);}
 void SetAuraStack(uint32 spell,Creature*,uint32 count){auras[spell]=count;}
 Creature* SummonCreature(uint32,Position const&,int);
};
struct Player : Creature
{
 using Creature::Creature;
 Player* ToPlayer() override{return this;}
 int GetTypeId() const override{return TYPEID_PLAYER;}
 TeamId GetTeamId() const{return faction==1732?TEAM_ALLIANCE:TEAM_HORDE;}
 bool IsPvP() const{return true;}
};
struct GameObject : Position
{
 uint32 phaseMask=1;
 using Position::Position;
 int state=0;
 struct Value { struct {uint32 Health=100,MaxHealth=100;} Building; } value;
 Value const* GetGOValue() const{return &value;}
 int GetDestructibleState() const{return state;}
};
struct BattlefieldWG
{
 TeamId attacker=TEAM_ALLIANCE;
 std::array<TeamId,6> owners{};
 std::array<uint32,2> current{},maximum{16,16},humans{};
 std::map<uint32,GameObject*> flags,buildings;
 BattlefieldWG(){owners.fill(TEAM_NEUTRAL);}
 void HandleBotWarPromotion(TeamId,Unit*){}
 uint32 GetZoneId() const{return 4197;}
 TeamId GetAttackerTeam() const{return attacker;}
 TeamId GetBotWarWorkshopControl(uint8 id) const{return id<owners.size()?owners[id]:TEAM_NEUTRAL;}
 GameObject* GetBotWarWorkshopFlagForArea(uint32 area){auto i=flags.find(area);return i==flags.end()?nullptr:i->second;}
 GameObject* GetBotWarBuilding(uint32 entry){auto i=buildings.find(entry);return i==buildings.end()?nullptr:i->second;}
 uint32 GetWarPlayerCount(TeamId team) const{return humans[team];}
 uint32 GetData(int id) const
 {
  if(id==AREA_THE_BROKEN_TEMPLATE)return owners[BATTLEFIELD_WG_WORKSHOP_NW];
  if(id==AREA_THE_SUNKEN_RING)return owners[BATTLEFIELD_WG_WORKSHOP_NE];
  return id==BATTLEFIELD_WG_DATA_VEHICLE_A?current[0]:id==BATTLEFIELD_WG_DATA_VEHICLE_H?current[1]:id==BATTLEFIELD_WG_DATA_MAX_VEHICLE_A?maximum[0]:maximum[1];}
};
template<class Source> void GetCreatureListWithEntryInGrid(std::list<Creature*>& out,Source* source,uint32 entry,float radius)
{for(Creature* c:world)if((!entry||c->entry==entry)&&(c->phaseMask&source->phaseMask)&&source->GetExactDist2d(c)<=radius)out.push_back(c);}
using WorldObject=Position;
namespace Trinity {
 struct AllCreaturesOfEntryInRange {WorldObject* center;uint32 entry;float radius;
  AllCreaturesOfEntryInRange(WorldObject* c,uint32 e,float r):center(c),entry(e),radius(r){}
  bool operator()(Creature* c)const{return c->entry==entry&&center->GetExactDist2d(c)<=radius;}};
 template<class T>struct SearcherContainerResult{};
 template<class Check,class Result>struct CreatureSearcherBase {uint32 phase;std::list<Creature*>& out;Check& check;
  CreatureSearcherBase(uint32 p,std::list<Creature*>& o,Check& c):phase(p),out(o),check(c){}};
}
namespace Cell {template<class Searcher>void VisitGridObjects(WorldObject*,Searcher& s,float)
 {for(Creature* c:world)if((c->phaseMask&s.phase)&&s.check(c))s.out.push_back(c);}}
ENGINEERS
void GetPlayerListInGrid(std::list<Player*>&,GameObject*,float){}
SITES
RESOLVE
bool BotAbilityAvailable(Creature*,Creature*,uint32){return true;}
void RecordBotAbility(Creature*,Creature*,uint32){}
struct npc_wg_bot_soldier : AIBase
{
 Creature* me;Lane lane=Center,requested=LaneCount;ObjectGuid build,escort;uint32 _killCredits=0,_profile=Warrior;
 explicit npc_wg_bot_soldier(Creature* c):me(c){c->ai=this;}
 Lane GetLane() const{return lane;}void SetTacticalLane(Lane l){requested=l;}
 bool HasWorkshopOrder() const{return IsWorkshop(lane)||IsWorkshop(requested);}
 bool HasBuildSite() const{return !build.IsEmpty();}void SetBuildSite(ObjectGuid g){build=g;}
 void SetEscortVehicle(ObjectGuid g){escort=g;}
 uint32 GetData(uint32 id) const override{return id==RankCreditData?_killCredits:0;}
SET_DATA
CONFIGURE
CAST
};
struct npc_wg_bot_vehicle : AIBase
{
 Lane lane=Center;unsigned redirects=0;
 void SetRoute(Lane l,uint32){lane=l;}
 void SetAssaultLane(BattlefieldWG*,Lane l){lane=l;++redirects;}
};
Creature* Creature::SummonCreature(uint32 entry,Position const& pos,int)
{++summons;auto* c=new Creature(*map,entry,pos);c->faction=faction;c->ai=new npc_wg_bot_vehicle;return c;}
#define CAST_AI(type,value) dynamic_cast<type*>(value)
BattlefieldWG* activeBattle=nullptr;
BattlefieldWG* GetBotWarBattlefield(){return activeBattle;}
bool HasBotWarHumanPassenger(Creature* vehicle){return vehicle->humanPassenger;}
npc_wg_bot_soldier* GetBotWarSoldierAI(Creature* c)
{return c?dynamic_cast<npc_wg_bot_soldier*>(c->native?c->ai->GetBattlefieldAI():c->ai):nullptr;}
void RetireBotWarSoldier(Creature* c){c->DespawnOrUnsummon();}
uint32 urand(uint32 low,uint32 high){static uint32 n=0;return low+(n++%(high-low+1));}
struct Waypoint {Position Pos;};
std::array<std::vector<Waypoint>,LaneCount> BotRoutes=[](){std::array<std::vector<Waypoint>,LaneCount> routes;for(auto& r:routes)r.push_back({Position{4800,3340,380,0}});return routes;}();
IS_SOLDIER
AWARD
struct Controller
{
SLOT
 uint32 _elapsed=1000;
 Lane _siegeLane=LaneCount;
 struct PatrolOrder {Lane Target=LaneCount;uint32 Until=0;};
 std::array<std::array<PatrolOrder,MaxParticipants/SquadSize>,2> _patrols{};
 std::array<std::array<RosterSlot,MaxParticipants>,2> _roster{};
 std::vector<ObjectGuid> _claimedVehicles;
REMOVE
RETIRE
FIND
BUILD
TACTICS
SIEGE
};
void Construction()
{
 for(TeamId team:{TEAM_ALLIANCE,TEAM_HORDE})
 {
  world.clear();Map map;BattlefieldWG wg;Controller controller;
  wg.attacker=TeamId(1-team); // defenders may still build a corporal's anti-infantry catapult
  Creature soldier(map,80501,{4550,3300,376,0});soldier.faction=WGFactionTable[team];npc_wg_bot_soldier ai(&soldier);
  Creature engineer(map,team==TEAM_ALLIANCE?NPC_GNOMISH_ENGINEER:NPC_GOBLIN_MECHANIC,{4390.78f,3304.09f,372.43f,0});
  engineer.faction=soldier.faction;
  GameObject southernFlag;static_cast<Position&>(southernFlag)=engineer.GetPosition();
  wg.flags[AREA_WESTPARK_WORKSHOP]=&southernFlag;
  wg.owners[BATTLEFIELD_WG_WORKSHOP_SW]=team;
  std::list<Creature*> legacyLookup;GetCreatureListWithEntryInGrid(legacyLookup,&southernFlag,engineer.entry,100.f);
  assert(legacyLookup.empty()); // reproduce phase-1 flag missing the SQL engineer in phase 16/32
  controller._roster[team][0].Soldier=soldier.guid;
  Controller::RosterSlot slot;
  slot.KillCredits=0;
  controller.EnsureVehicle(&map,&wg,team,0,slot,&soldier);
  assert(slot.Engineer==engineer.guid&&ai.build==engineer.guid&&soldier.summons==0);
  controller._elapsed=21000;
  controller.EnsureVehicle(&map,&wg,team,0,slot,&soldier);
  assert(slot.BuildStartedAt==0&&soldier.GetPositionX()==4550); // no teleport and no remote build
  static_cast<Position&>(soldier)=engineer.GetPosition();
  controller.EnsureVehicle(&map,&wg,team,0,slot,&soldier);
  assert(slot.BuildStartedAt==21000);
  wg.owners[BATTLEFIELD_WG_WORKSHOP_SW]=TEAM_NEUTRAL;controller._elapsed=26000;
  controller.EnsureVehicle(&map,&wg,team,0,slot,&soldier);
  assert(slot.Engineer.IsEmpty()&&ai.build.IsEmpty()&&soldier.summons==0); // lost before completion
  wg.owners[BATTLEFIELD_WG_WORKSHOP_SW]=team;controller._elapsed=32000;
  controller.EnsureVehicle(&map,&wg,team,0,slot,&soldier);
  assert(slot.BuildStartedAt==0); // bounded retry interval after factory loss
  controller._elapsed=37000;controller.EnsureVehicle(&map,&wg,team,0,slot,&soldier);
  assert(slot.BuildStartedAt==37000);
  wg.maximum[team]=0;controller._elapsed=42000;
  controller.EnsureVehicle(&map,&wg,team,0,slot,&soldier);
  assert(slot.BuildStartedAt==0&&soldier.summons==0); // capacity revoked while building
  wg.maximum[team]=16;controller._elapsed=48000;
  controller.EnsureVehicle(&map,&wg,team,0,slot,&soldier);
  controller._elapsed=52000;controller.EnsureVehicle(&map,&wg,team,0,slot,&soldier);
  assert(soldier.summons==0);
  controller._elapsed=53000;controller.EnsureVehicle(&map,&wg,team,0,slot,&soldier);
  assert(soldier.summons==1&&!slot.Vehicle.IsEmpty()&&ai.escort==slot.Vehicle);
  uint32 expectedVeh = (team == TEAM_ALLIANCE ? NPC_WINTERGRASP_SIEGE_ENGINE_ALLIANCE : NPC_WINTERGRASP_SIEGE_ENGINE_HORDE);
  assert(map.GetCreature(slot.Vehicle)->entry==expectedVeh&&ai.build.IsEmpty());
  assert(soldier.GetExactDist2d(&engineer)==0); // builder walked; construction never moves them
  ai.SetData(RankCreditData,5);
  slot.Soldier=soldier.guid;
  ObjectGuid retained=slot.Vehicle;
  soldier.alive=false;controller.RetireSoldier(&map,slot);
  assert(slot.Soldier.IsEmpty()&&slot.Vehicle==retained&&map.GetCreature(retained)->alive&&slot.KillCredits==5);
  soldier.alive=true;
  Controller::RosterSlot heavy;heavy.KillCredits=10;controller._elapsed=60000;
  controller.EnsureVehicle(&map,&wg,team,0,heavy,&soldier);
  controller._elapsed=65000;controller.EnsureVehicle(&map,&wg,team,0,heavy,&soldier);
  assert(map.GetCreature(heavy.Vehicle)->entry==(team==TEAM_ALLIANCE?NPC_WINTERGRASP_SIEGE_ENGINE_ALLIANCE:NPC_WINTERGRASP_SIEGE_ENGINE_HORDE));
  Controller::RosterSlot fighting;fighting.KillCredits=10;soldier.combat=true;soldier.incoming.push_back(&engineer);
  controller.EnsureVehicle(&map,&wg,team,0,fighting,&soldier);assert(fighting.Engineer.IsEmpty());
  soldier.combat=false;soldier.incoming.clear();
  wg.humans[team]=1;wg.current[team]=15;wg.maximum[team]=16;
  controller.EnsureVehicle(&map,&wg,team,0,fighting,&soldier);assert(fighting.Engineer.IsEmpty());
  // An engineer in an enemy factory never qualifies, even if it is friendly/neutral.
  wg.current[team]=0;wg.owners.fill(TEAM_NEUTRAL);uint8 factory=255;
  assert(controller.FindBuildEngineer(&map,&wg,team,Center,&soldier,factory)==nullptr);
  // The only owned factory is inside the keep: offensive columns cannot build there.
  static_cast<Position&>(engineer)={5392.91f,2975.26f,415.223f,0};engineer.phaseMask=FactoryPhaseMask(team==TEAM_ALLIANCE,true);wg.owners[BATTLEFIELD_WG_WORKSHOP_KEEP_WEST]=team;
  assert(controller.FindBuildEngineer(&map,&wg,team,Center,&soldier,factory)==nullptr);
  assert(controller.FindBuildEngineer(&map,&wg,team,KeepWest,&soldier,factory)==nullptr);
  static_cast<Position&>(soldier)=engineer.GetPosition();
  assert(controller.FindBuildEngineer(&map,&wg,team,KeepWest,&soldier,factory)==&engineer);
 }
}
void SiegeDispatch()
{
 for(TeamId team:{TEAM_ALLIANCE,TEAM_HORDE}) {
  world.clear();Map map;BattlefieldWG wg;Controller controller;wg.attacker=team;
  Creature engineer(map,team==TEAM_ALLIANCE?NPC_GNOMISH_ENGINEER:NPC_GOBLIN_MECHANIC,{4390.78f,3304.09f,372.43f,0});engineer.faction=WGFactionTable[team];
  GameObject flag;static_cast<Position&>(flag)=engineer;wg.flags[AREA_WESTPARK_WORKSHOP]=&flag;wg.owners[BATTLEFIELD_WG_WORKSHOP_SW]=team;
  Creature first(map,80501,engineer),second(map,80501,engineer),third(map,80501,engineer);
  first.faction=second.faction=third.faction=WGFactionTable[team];
  npc_wg_bot_soldier a(&first),b(&second),c(&third);
  auto& one=controller._roster[team][0];auto& two=controller._roster[team][5];auto& three=controller._roster[team][10];
  one.Soldier=first.guid;two.Soldier=second.guid;three.Soldier=third.guid;
  one.KillCredits=0;two.KillCredits=three.KillCredits=0;first.combat=true;
  controller.EnsureVehicle(&map,&wg,team,0,one,&first);
  assert(!one.Engineer.IsEmpty()&&!first.combat&&first.threat.clears==1); // safely leaves a finished engagement and builds siege without rank
  controller.EnsureVehicle(&map,&wg,team,5,two,&second);assert(!two.Engineer.IsEmpty());
  controller.EnsureVehicle(&map,&wg,team,10,three,&third);assert(three.Engineer.IsEmpty());
  controller._elapsed=6000;controller.EnsureVehicle(&map,&wg,team,0,one,&first);
  auto* vehicle=map.GetCreature(one.Vehicle);assert(vehicle&&vehicle->entry!=(uint32)NPC_WINTERGRASP_CATAPULT);
  VehicleKit seat;vehicle->kit=&seat;
  controller.EnsureVehicle(&map,&wg,team,10,three,&third);assert(three.Engineer.IsEmpty()); // driver still walking to vehicle
  seat.driver=&first;controller.EnsureVehicle(&map,&wg,team,10,three,&third);assert(!three.Engineer.IsEmpty());
 }
}
void Resources()
{
 world.clear();Map map;
 for(unsigned slot=0;slot<120;++slot)
 {
  Creature body(map);npc_wg_bot_soldier ai(&body);ai.ConfigureProfile(slot);
  auto p=ProfileForSlot(slot);assert(ai._profile==p&&(body.flags2&UNIT_FLAG2_REGENERATE_POWER));
  if(p==Rogue)assert(body.powerType==POWER_ENERGY&&body.maxPower[POWER_ENERGY]==100&&body.power[POWER_ENERGY]==100&&body.dual&&body.items[0]==40386);
  else if(p==Warrior)assert(body.powerType==POWER_RAGE&&body.maxPower[POWER_RAGE]==1000&&body.power[POWER_RAGE]==0);
  else assert(body.powerType==POWER_MANA);
 }
}
void RanksAndCasts()
{
 world.clear();Map map;Creature soldier(map);npc_wg_bot_soldier ai(&soldier);
 for(uint32 credit=0;credit<25;++credit)
 {
  ai.SetData(RankCreditData,credit);
  uint32 rank=RankForCredits(credit),spell=rank==2?SPELL_LIEUTENANT:rank==1?SPELL_CORPORAL:SPELL_RECRUIT;
  assert(ai.GetData(RankCreditData)==std::min(credit,10u));
  assert(soldier.auras.size()==1&&soldier.auras[spell]==(rank==2?1:1+credit%5));
 }
 uint32 timer=0;
 for(int failure:{SPELL_FAILED_NO_POWER,SPELL_FAILED_OUT_OF_RANGE})
 {
  soldier.castResult=failure;assert(!ai.CastAbility(&soldier,123,timer,120000));assert(timer==500);
 }
 soldier.castResult=SPELL_CAST_OK;assert(ai.CastAbility(&soldier,123,timer,120000));assert(timer==120000);
 assert(WorkshopResponseSquads(false,0,0)==1);
 assert(WorkshopResponseSquads(false,0,12)==2);
 assert(WorkshopResponseSquads(false,20,12)==0);
 assert(WorkshopResponseSquads(true,5,0)==0);
 assert(WorkshopResponseSquads(true,0,12)==2);
}
void KillCredits()
{
 world.clear();Map map;BattlefieldWG wg;activeBattle=&wg;Controller controller;
 Creature ally(map,80501),far(map,80502,{100,0,0,0}),enemy(map,80505);
 enemy.faction=1735;npc_wg_bot_soldier a(&ally),f(&far),e(&enemy);
 Player killer(map,999),victim(map,999,{10,0,0,0});victim.faction=1735;
 AwardBotWarKill(&killer,&victim);
 assert(a.GetData(RankCreditData)==1&&f.GetData(RankCreditData)==0&&e.GetData(RankCreditData)==0);
 victim.zone=0;AwardBotWarKill(&killer,&victim);victim.zone=4197;
 victim.faction=1732;AwardBotWarKill(&killer,&victim);victim.faction=1735;
 AwardBotWarKill(&ally,&ally);assert(a.GetData(RankCreditData)==1);
 Creature mob(map,999,{10,0,0,0});mob.faction=1735;AwardBotWarKill(&ally,&mob);
 assert(a.GetData(RankCreditData)==1); // ordinary mobs do not grant battle rank
 Creature guard(map,BATTLEFIELD_WG_NPC_GUARD_H,{10,0,0,0});guard.faction=1735;
 AwardBotWarKill(&ally,&guard);assert(a.GetData(RankCreditData)==2);
 for(unsigned i=0;i<3;++i)AwardBotWarKill(&ally,&victim);
 assert(a.GetData(RankCreditData)==5&&ally.auras.count(SPELL_CORPORAL));
 for(unsigned i=0;i<10;++i)AwardBotWarKill(&ally,&victim);
 assert(a.GetData(RankCreditData)==10&&ally.auras.count(SPELL_LIEUTENANT));
 Controller::RosterSlot slot;slot.Soldier=ally.guid;ally.alive=false;
 controller.RemoveSlot(&map,slot);
 assert(slot.KillCredits==10&&slot.Soldier.IsEmpty());
 Creature replacement(map);npc_wg_bot_soldier replacementAI(&replacement);
 replacementAI.SetData(RankCreditData,slot.KillCredits);
 assert(replacement.auras.count(SPELL_LIEUTENANT));
 activeBattle=nullptr;
}
void NativeIdentity()
{
 world.clear();Map map;BattlefieldWG wg;activeBattle=&wg;
 Creature named(map,900001),victim(map,900002);victim.faction=1735;
 npc_wg_bot_soldier tactical(&named),enemyTactical(&victim);AIBase nativeAI,enemyAI;
 nativeAI.battlefield=enemyAI.battlefield=true;nativeAI.controller=&tactical;enemyAI.controller=&enemyTactical;
 named.native=victim.native=true;named.ai=&nativeAI;victim.ai=&enemyAI;
 assert(IsBotWarSoldier(&named)&&GetBotWarSoldierAI(&named)==&tactical);
 named.powerType=POWER_ENERGY;named.power[POWER_ENERGY]=73;tactical.ConfigureProfile(0);
 assert(named.powerType==POWER_ENERGY&&named.power[POWER_ENERGY]==73);
 nativeAI.battlefield=false;assert(!IsBotWarSoldier(&named));
 activeBattle=nullptr;
}
void Commander()
{
 for(TeamId team:{TEAM_ALLIANCE,TEAM_HORDE})
  for(bool attacker:{false,true})
  {
   world.clear();Map map;BattlefieldWG wg;Controller controller;
   wg.attacker=attacker?team:TeamId(1-team);
   std::array<GameObject,4> flags;
   unsigned n=0;
   for(auto const& site:BotWorkshopSites)
    if(site.Area){static_cast<Position&>(flags[n])=site.Fallback;wg.flags[site.Area]=&flags[n++];wg.owners[site.Id]=team;}
   wg.owners[BATTLEFIELD_WG_WORKSHOP_NW]=TeamId(1-team);
   for(uint32 i=0;i<120;++i)
   {
    auto* c=new Creature(map,team==TEAM_ALLIANCE?80501:80505,{4800,3340,380,0});c->faction=WGFactionTable[team];
    auto* ai=new npc_wg_bot_soldier(c);ai->lane=SquadLane(attacker,i);
    controller._roster[team][i].Soldier=c->guid;
   }
   controller.UpdateTactics(&map,&wg,team);
   uint32 sent=0;
   for(uint32 i=0;i<120;++i)
   {
    auto* c=map.GetCreature(controller._roster[team][i].Soldier);auto* ai=dynamic_cast<npc_wg_bot_soldier*>(c->ai);
    if(ai->requested==WorkshopWest){++sent;ai->lane=WorkshopWest;static_cast<Position&>(*c)=flags[0];}
    if(IsTowerDefense(SquadLane(attacker,i))||IsCounterAttack(SquadLane(attacker,i)))assert(ai->requested==LaneCount);
   }
   assert(sent>=5&&sent<=15);uint32 initial=sent; // response plus at most two scouting squads
   controller.UpdateTactics(&map,&wg,team);sent=0;
   for(uint32 i=0;i<120;++i)
   {
    auto* ai=dynamic_cast<npc_wg_bot_soldier*>(map.GetCreature(controller._roster[team][i].Soldier)->ai);
    if(ai->requested==WorkshopWest)++sent;
   }
   assert(sent==initial); // enough troops present must not cancel before actual capture
   wg.owners[BATTLEFIELD_WG_WORKSHOP_NW]=team;controller.UpdateTactics(&map,&wg,team);
   for(uint32 i=0;i<120;++i)
   {
    auto* ai=dynamic_cast<npc_wg_bot_soldier*>(map.GetCreature(controller._roster[team][i].Soldier)->ai);
    assert(ai->requested!=WorkshopWest || controller._patrols[team][i/SquadSize].Target==WorkshopWest); // urgent response released; deliberate scouts may revisit
   }
  }
}
void Siege()
{
 for(TeamId team:{TEAM_ALLIANCE,TEAM_HORDE})
 {
  world.clear();Map map;BattlefieldWG wg;wg.attacker=team;Controller controller;
  wg.owners.fill(team);
  GameObject center(5140,2840,405,0),west(5180,2934,409,0),east(5180,2747,409,0);
  wg.buildings[190375]=&center;wg.buildings[190371]=&west;wg.buildings[190372]=&east;
  west.state=GO_DESTRUCTIBLE_DESTROYED;
  Creature leader(map,80501,{5000,2840,380,0});leader.faction=WGFactionTable[team];
  npc_wg_bot_soldier ai(&leader);
  Creature engine(map,NPC_WINTERGRASP_DEMOLISHER,{5000,2840,380,0});engine.faction=leader.faction;
  npc_wg_bot_vehicle siege;engine.ai=&siege;
  controller._roster[team][0].Soldier=leader.guid;controller._roster[team][0].Vehicle=engine.guid;
  controller.UpdateSiegeTactics(&map,&wg,team);
  assert(siege.lane==West&&ai.requested==West&&controller._siegeLane==West);
  center.state=GO_DESTRUCTIBLE_DESTROYED;
  controller.UpdateSiegeTactics(&map,&wg,team);assert(controller._siegeLane==West); // tie: keep convoy stable
  center.state=0;west.state=0;east.state=GO_DESTRUCTIBLE_DESTROYED;
  engine.humanPassenger=true;unsigned before=siege.redirects;
  controller.UpdateSiegeTactics(&map,&wg,team);assert(siege.redirects==before); // human control wins
  engine.humanPassenger=false;engine.m_positionX=5200;
  controller.UpdateSiegeTactics(&map,&wg,team);assert(siege.redirects==before); // no retreat from inside the keep
  engine.m_positionX=5000;ai.requested=WorkshopWest;
  controller.UpdateSiegeTactics(&map,&wg,team);assert(siege.redirects==before); // capturers retain their orders
 }
}
int main()
{
 Construction();SiegeDispatch();Resources();RanksAndCasts();KillCredits();NativeIdentity();Commander();Siege();
 std::cout<<"PASS: production construction for both factions, ownership/capacity revocation, walking, real nearby kill credit and death persistence, rank-gated vehicles, human reserve, spell failure retries, four-side commander recovery/hold/release and siege convoy coordination\n";
}
'''
site_start=source.index('struct BotWorkshopSite')
site_end=source.index('// Engineers use factory/keep control phases',site_start)
# WorkshopArea is included and used by the production code, even in this harness.
code=code.replace('SITES',source[site_start:site_end])
for marker,signature in [('ENGINEERS','void GetBotWarEngineers('),('IS_SOLDIER','bool IsBotWarSoldier('),('AWARD','void AwardBotWarKill('),('REMOVE','    void RemoveSlot('),('RETIRE','    void RetireSoldier('),('RESOLVE','WintergraspBotWar::Lane ResolveSquadLane('),('SET_DATA','    void SetData(uint32 id, uint32 value)'),('CONFIGURE','    void ConfigureProfile('),('CAST','    bool CastAbility('),('SLOT','    struct RosterSlot'),('FIND','    Creature* FindBuildEngineer('),('BUILD','    void EnsureVehicle('),('TACTICS','    void UpdateTactics('),('SIEGE','    void UpdateSiegeTactics(')]:
    text=function(source,signature)
    if marker=='SLOT':text+=';'
    code=code.replace('\n'+marker+'\n','\n'+text+'\n')
with tempfile.TemporaryDirectory(prefix='wg-logistics-') as tmp:
    cpp,binary=Path(tmp)/'test.cpp',Path(tmp)/'test'
    cpp.write_text(code)
    subprocess.run(['c++','-std=c++20','-O2','-Wall','-Wextra','-Werror','-I',str(repo),str(cpp),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
