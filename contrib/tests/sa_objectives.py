#!/usr/bin/env python3
"""Run production selection, cast ordering and SA capture completion handlers."""
from pathlib import Path
import subprocess
import tempfile
repo=Path(__file__).resolve().parents[2]
ai=(repo/'src/server/game/AI/NpcBots/bot_battlegrounds.cpp').read_text()
bg=(repo/'src/server/game/Battlegrounds/Zones/BattlegroundSA.cpp').read_text()
effects=(repo/'src/server/game/Spells/SpellEffects.cpp').read_text()
def function(text,signature):
    start=text.index(signature);brace=text.index('{',start);depth=1;end=brace+1
    while depth:
        depth+=(text[end]=='{')-(text[end]=='}');end+=1
    return text[start:end]
selector=function(ai,'GameObject* SelectSAAttackObjective(')
handler=function(bg,'void BattlegroundSA::EventBotClickedOnFlag(')
access=function(bg,'bool BattlegroundSA::CanInteractWithObject(')
dispatch=ai[ai.index('    if (!finalAssault && !(sa && objective)'):ai.index('    // Carry a real bomb payload')]
exit_start=ai.index('    if (finalAssault && me->GetVehicle()')
exit_vehicle=ai[exit_start:ai.index('    if (me->GetVehicle())',exit_start)]
eligibility=ai[ai.index('    if (_siegeBoardTimer ||',ai.index('bool bot_ai::TrySiegeVehicle(')):ai.index('    std::vector<uint32> entries;',ai.index('bool bot_ai::TrySiegeVehicle('))]
completion=effects[effects.index('        GameObjectTemplate const* botGoInfo',effects.index('void Spell::EffectOpenLock()')):effects.index('        // Arathi Basin banner opening.',effects.index('void Spell::EffectOpenLock()'))]
code=r'''
#include "src/server/game/AI/NpcBots/botbgtactics.h"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <map>
#include <vector>
using uint32=std::uint32_t;using uint8=std::uint8_t;using TeamId=unsigned;
using namespace NPCBots::SiegeBG;
enum {BATTLEGROUND_SA=9,BATTLEGROUND_IC=30,STATUS_IN_PROGRESS=1,BG_SA_ROUND_ONE=2,BG_SA_ROUND_TWO=4,
 BG_SA_LEFT_FLAG=23,BG_SA_RIGHT_FLAG=24,BG_SA_CENTRAL_FLAG=25,BG_SA_TITAN_RELIC=6,
 BG_SA_LEFT_CAPTURABLE_GY=0,BG_SA_RIGHT_CAPTURABLE_GY=1,BG_SA_CENTRAL_CAPTURABLE_GY=2,
 BG_SA_GREEN_GATE=0,BG_SA_YELLOW_GATE=1,BG_SA_BLUE_GATE=2,BG_SA_RED_GATE=3,BG_SA_PURPLE_GATE=4,BG_SA_ANCIENT_GATE=5,
 BG_SA_GATE_OK=1,BG_SA_GATE_DESTROYED=3,GO_CHAMBER_OF_ANCIENT_RELICS=192549,
 GO_FLAG_NOT_SELECTABLE=1,CURRENT_GENERIC_SPELL=1,OPEN_FLAG_BG=21651,SPELL_CAST_OK=255,UNIT_FLAG_IMMUNE=1,BOT_ROLE_HEAL=1};
#define ABORT() std::abort()
float constexpr INTERACTION_DISTANCE=5.f;
struct Guid {unsigned value;unsigned GetCounter()const{return value;}operator unsigned()const{return value;}};
struct Position {float x=0,y=0,z=0;Position const& GetPosition()const{return *this;}};
struct Targets {Guid guid{0};Guid GetGOTargetGUID()const{return guid;}};
struct Info {unsigned Id=21651;};
struct Spell {Info info;Info* m_spellInfo=&info;Targets m_targets;};
struct BattlegroundSA;struct GameObject;
struct Unit:Position {
 BattlegroundSA* bg=nullptr;Guid guid{4};bool alive=true,registered=true,combat=false,mounted=true,form=true,immune=false,npc=true,los=true,vehicle=false,transport=false,success=true;
 unsigned phase=1,casts=0,attackStops=0,movementStops=0,exits=0;GameObject* pending=nullptr;Spell* casting=nullptr;
 Guid GetGUID()const{return guid;}bool IsAlive()const{return alive;}bool IsInCombat()const{return combat;}
 bool GetVehicle()const{return vehicle;}Unit* GetVehicleBase(){return this;}bool GetTransport()const{return transport;}
 bool IsNPCBot()const{return npc;}Unit* ToCreature(){return this;}BattlegroundSA* GetBotBG(){return bg;}
 bool HasUnitFlag(unsigned)const{return immune;}void* GetMap()const{return (void*)1;}
 bool InSamePhase(Unit const* o)const{return phase&o->phase;}
 float GetExactDist(Unit const* o)const{return std::sqrt((x-o->x)*(x-o->x)+(y-o->y)*(y-o->y)+(z-o->z)*(z-o->z));}
 bool IsWithinDistInMap(Unit const* o,float distance)const{return GetExactDist(o)<=distance;}
 bool IsWithinLOSInMap(Unit const*)const{return los;}
 Spell* GetCurrentSpell(unsigned)const{return casting;}
 void StopMoving(){++movementStops;}void AttackStop(){++attackStops;}
 unsigned CastSpell(GameObject* object,unsigned spell,bool triggered){if(spell!=21651||triggered)std::abort();++casts;if(success)pending=object;return success?255:1;}
 void ExitVehicle(){vehicle=false;++exits;}
};
using Creature=Unit;
struct GameObjectTemplate {bool CannotBeUsedUnderImmunity()const{return true;}};
struct GameObject:Unit {
 unsigned entry=0;bool spawned=true,disabled=false;GameObjectTemplate info;
 bool isSpawned()const{return spawned;}bool HasFlag(unsigned)const{return disabled;}
 unsigned GetEntry()const{return entry;}GameObjectTemplate const* GetGOInfo()const{return &info;}
};
struct BattlegroundSA {
 unsigned status=1,Status=2,Attackers=0,team=0,captures=0,victories=0,type=BATTLEGROUND_SA;
 unsigned GraveyardStatus[3]={1,1,1},GateStatus[6]={1,1,1,1,1,1};
 std::map<unsigned,GameObject> objects;std::map<unsigned,Guid> BgObjects;
 unsigned GetTypeID()const{return type;}unsigned GetStatus()const{return status;}
 unsigned GetAttackersTeam()const{return Attackers;}unsigned GetBotTeamId(Guid)const{return team;}
 unsigned GetGraveyardStatus(unsigned gy)const{return GraveyardStatus[gy];}
 bool IsPlayerInBattleground(Guid)const{return registered;}bool registered=true;
 void* GetBgMap()const{return (void*)1;}
 GameObject* GetBGObject(unsigned type,bool){auto i=objects.find(type);return i==objects.end()?nullptr:&i->second;}
 bool CanInteractWithObject(unsigned);
 void EventBotClickedOnFlag(Creature*,GameObject*);
 void CaptureGraveyard(unsigned gy,Creature*){GraveyardStatus[gy]=Attackers;++captures;}
 void TitanRelicActivated(Creature*){if(CanInteractWithObject(6))++victories;}
};
using Battleground=BattlegroundSA;
ACCESS
SELECTOR
HANDLER
namespace BotMgr {std::vector<Unit*> allies;auto const& GetAllGroupMembers(Unit*){return allies;}}
struct Controller {
 Creature* me;unsigned boards=0,_siegeBoardTimer=0,_siegeTravelNode=99;bool vehicleAvailable=true,_siegeBombCarrier=false;
 BattlegroundSA* GetBG(){return me->bg;}bool IsCasting(){return me->casting;}bool HasRole(unsigned){return false;}
 bool Eligible(bool defender,uint8 squad){(void)squad;
ELIGIBILITY
  return true;
 }
 bool TrySiegeVehicle(bool,unsigned){++boards;return vehicleAvailable;}
 void DismountBot(){me->mounted=false;}void removeShapeshiftForm(){me->form=false;}
 bool Act(BattlegroundSA* sa,GameObject* objective,bool finalAssault=false){
  bool defend=false;unsigned squad=0;
EXIT
DISPATCH
  return false;
 }
};
void Complete(Unit* m_caster,GameObject* gameObjTarget){
COMPLETION
}
void Check(bool ok,char const* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}}
void SetMask(BattlegroundSA& bg,unsigned mask){for(unsigned i=0;i<6;++i)bg.GateStatus[i]=(mask&(1u<<i))?3:1;}
void Setup(BattlegroundSA& bg){
 for(auto [type,entry]:{std::pair{23u,191307u},{24u,191305u},{25u,191310u},{6u,192834u}}){
  GameObject go;go.entry=entry;go.guid={type+100};go.x=float(type);bg.objects.emplace(type,go);bg.BgObjects[type]=go.guid;
 }
}
int main(){
 BattlegroundSA bg;Setup(bg);Creature bot;bot.bg=&bg;bot.guid={4};Controller ai{&bot};
 for(unsigned mask=0;mask<64;++mask)for(unsigned team:{0u,1u}){
  SetMask(bg,mask);bg.team=bg.Attackers=team;for(auto& gy:bg.GraveyardStatus)gy=1-team;
  auto* chosen=SelectSAAttackObjective(&bot,&bg);
  if(RelicOpen(mask))Check(chosen==bg.GetBGObject(6,true),"uncaptured flags override vulnerable relic");
  else if(OuterOpen(mask))Check(chosen&&chosen!=bg.GetBGObject(6,true),"scout ignores available graveyards");
  else Check(!chosen,"capture objective selected through intact outer gates");
 }
 bg.Attackers=bg.team=0;for(auto& gy:bg.GraveyardStatus)gy=1;SetMask(bg,5);bot.x=24;
 Check(SelectSAAttackObjective(&bot,&bg)==bg.GetBGObject(24,true),"scout takes fixed left flag instead of nearby right flag");
 bg.GraveyardStatus[1]=0;Check(SelectSAAttackObjective(&bot,&bg)==bg.GetBGObject(23,true),"owned graveyard selected again");
 bot.vehicle=true;Check(!SelectSAAttackObjective(&bot,&bg),"attacking scout's vehicle sent to a flag");bot.vehicle=false;
 for(unsigned id=0;id<15;++id){bot.guid={id};bot.vehicle=true;Check(!SelectSAAttackObjective(&bot,&bg),"an attacking vehicle receives a graveyard objective");}
 bot.guid={4};bot.vehicle=false;
 bg.team=1;Check(!SelectSAAttackObjective(&bot,&bg),"defender attempts attacker objectives");bg.team=0;
 bot.guid={0};bot.x=500;Check(!SelectSAAttackObjective(&bot,&bg),"distant assault troop abandons assault for flag");
 bot.guid={4};bot.x=24;bot.combat=true;
 auto* flag=bg.GetBGObject(24,true);bg.GraveyardStatus[1]=1;
 Check(ai.Act(&bg,flag)&&bot.casts==1&&ai.boards==0&&!bot.mounted&&!bot.form&&bot.attackStops==1,"combat or boarding prevents real flag cast");
 Check(bg.captures==0&&bot.pending==flag,"capture completed without interruptible cast");
 Complete(&bot,flag);Check(bg.captures==1&&bg.GraveyardStatus[1]==0,"real spell completion does not capture graveyard");
 SetMask(bg,63);auto* relic=bg.GetBGObject(6,true);bot.x=relic->x;bot.vehicle=true;
 Check(ai.Act(&bg,relic,true)&&bot.exits==1&&!bot.vehicle&&ai._siegeBoardTimer==30000&&ai._siegeTravelNode==0,"driver fails to dismount and click relic");
 Check(bg.victories==0,"victory before cast completion");Complete(&bot,relic);Check(bg.victories==1,"vulnerable relic not activated at completion");
 for(unsigned invalid=0;invalid<9;++invalid){
  Creature b;b.bg=&bg;b.x=relic->x;auto copy=*relic;auto* target=relic;
  if(invalid==0)b.alive=false;if(invalid==1)bg.status=0;if(invalid==2)bg.Status=1;
  if(invalid==3)bg.team=1;if(invalid==4)b.vehicle=true;if(invalid==5)b.phase=2;
  if(invalid==6)b.x+=100;if(invalid==7){copy.guid={999};target=&copy;}if(invalid==8)b.immune=true;
  unsigned before=bg.victories;Complete(&b,target);Check(bg.victories==before,"invalid capture passed completion validation");
  bg.status=1;bg.Status=2;bg.team=0;
 }
 bot.vehicle=false;bot.combat=false;bot.success=false;unsigned before=bot.casts;
 Check(!ai.Act(&bg,relic,true)&&bot.casts==before+1,"failed cast treated as active capture");
 for(unsigned first=0;first<100;++first){unsigned scouts=0;for(unsigned id=first;id<first+15;++id)scouts+=SACaptureScout(id);Check(scouts==3,"15-bot roster loses capture scouts");}
 bot.success=true;bot.combat=false;ai._siegeBoardTimer=0;bot.guid={4};
 Check(!ai.Eligible(false,0),"capture scout boards a beach demolisher");
 bot.guid={0};Check(ai.Eligible(false,0),"ordinary assault driver cannot board");
 bot.guid={4};bg.type=BATTLEGROUND_IC;Check(ai.Eligible(false,0),"SA scout rule changes IC drivers");bg.type=BATTLEGROUND_SA;
 bot.guid={1};Check(ai.Eligible(true,0),"first defender gunner cannot board");
 bot.guid={2};Check(ai.Eligible(true,0),"second defender gunner cannot board");
 bot.guid={3};Check(!ai.Eligible(true,0),"all defenders leave infantry line for cannons");
 for(unsigned team:{0u,1u})for(unsigned type:{23u,24u,25u}){
  SetMask(bg,31);bg.team=bg.Attackers=team;for(auto& gy:bg.GraveyardStatus)gy=1-team;
  auto* flag=bg.GetBGObject(type,true);bot.x=flag->x;unsigned before=bg.captures;Complete(&bot,flag);
  Check(bg.captures==before+1,"native graveyard completion missing for a faction/flag");
 }
 std::cout<<"PASS: actual SA objective selection (64 gate states, both teams), combat cast before boarding, scout roles, vehicle exit, deferred native capture/victory and invalid completion guards\n";
}
'''
for marker,value in [('ACCESS',access),('SELECTOR',selector),('HANDLER',handler),('EXIT',exit_vehicle),('DISPATCH',dispatch),('COMPLETION',completion),('ELIGIBILITY',eligibility)]:code=code.replace(marker,value)
with tempfile.TemporaryDirectory(prefix='sa-objectives-') as directory:
    cpp,exe=Path(directory)/'test.cpp',Path(directory)/'test';cpp.write_text(code)
    subprocess.run(['c++','-std=c++20','-O2','-Wall','-Wextra','-Wno-misleading-indentation','-Wno-missing-field-initializers','-I',str(repo),str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
