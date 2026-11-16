#!/usr/bin/env python3
"""Exercise production SA path, spline, teleporter and final-defense decisions."""
from pathlib import Path
import subprocess
import tempfile

repo=Path(__file__).resolve().parents[2]
ai=(repo/'src/server/game/AI/NpcBots/bot_battlegrounds.cpp').read_text()
spline=(repo/'src/server/game/Movement/Spline/MoveSplineInit.cpp').read_text()
helpers=ai[ai.index('bool SAPathClear('):ai.index('uint32 VehicleSpell(')]
launch=spline[spline.index('        args.path[0] = real_position;'):spline.index('        args.initialOrientation = real_position.orientation;')]
posts_start=ai.index('        Gate gate = AssaultGate(',ai.index('bool bot_ai::UpdateSiegeBattleground('))
posts=ai[posts_start:ai.index('        if (attacker)\n            if (GameObject* selected',posts_start)]
code=r'''
#include "src/server/game/AI/NpcBots/botbgtactics.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <list>
#include <map>
#include <limits>
#include <vector>
using uint32=std::uint32_t;using uint8=std::uint8_t;
using namespace NPCBots::SiegeBG;
enum {BATTLEGROUND_SA=9,STATUS_IN_PROGRESS=1,UNIT_STATE_CASTING=1,UNIT_STATE_CONTROLLED=2,
BG_SA_PORTAL_DEFFENDER_BLUE=7,BG_SA_PORTAL_DEFFENDER_RED=11,NPC_WORLD_TRIGGER_LARGE_AOI_NOT_IMMUNE_PC_NPC=23472,
GO_FLAG_NOT_SELECTABLE=1,MOTION_PRIORITY_NORMAL=1,SPELL_AURA_MOUNTED=1,SPELL_TELEPORT_VISUAL_ONLY=51347,
BG_SA_ANCIENT_GATE=5,BG_SA_TITAN_RELIC=6,BOT_ROLE_HEAL=1};
float constexpr INTERACTION_DISTANCE=5.f,INVALID_HEIGHT=-100000.f;
enum {UNIT_STATE_JUMPING=4,PATHFIND_NORMAL=1,PATHFIND_INCOMPLETE=4,LINEOFSIGHT_ALL_CHECKS=3};
namespace Movement {float constexpr gravity=19.29110527f;}
namespace VMAP {enum class ModelIgnoreFlags{Nothing};}
struct Vec {
 float x=0,y=0,z=0;
 Vec operator+(Vec const& v)const{return {x+v.x,y+v.y,z+v.z};}
 Vec operator-(Vec const& v)const{return {x-v.x,y-v.y,z-v.z};}
 Vec operator*(float f)const{return {x*f,y*f,z*f};}
 float length()const{return std::sqrt(x*x+y*y+z*z);}
};
namespace G3D {using Vector3=Vec;}using Vector3=Vec;
struct Position {
 union {float x,m_positionX;};union {float y,m_positionY;};union {float z,m_positionZ;};
 float orientation=0;
 Position(float a=0,float b=0,float c=0,float d=0):x(a),y(b),z(c),orientation(d){}
 float GetPositionX()const{return x;}float GetPositionY()const{return y;}float GetPositionZ()const{return z;}
 float GetExactDist(Position const* p)const{return std::sqrt((x-p->x)*(x-p->x)+(y-p->y)*(y-p->y)+(z-p->z)*(z-p->z));}
 Position const& GetPosition()const{return *this;}
 float GetOrientation()const{return orientation;}
};
struct Map {
 float ground=-1000000.f;bool los=true;
 float GetHeight(unsigned,float,float,float,bool,float){return ground;}
 bool isInLineOfSight(float,float,float,float,float,float,unsigned,unsigned,VMAP::ModelIgnoreFlags){return los;}
} world;
struct Motion {
 unsigned moves=0,clears=0,jumps=0;Position goal;
 void Clear(unsigned){++clears;}void MovePoint(unsigned,Position const& p,bool){goal=p;++moves;}
 bool GetDestination(float& x,float& y,float& z){x=goal.x;y=goal.y;z=goal.z;return moves;}
 void MoveJump(Position const& p,float,float,unsigned){goal=p;++jumps;}
};
struct BattlegroundSA;
struct Kit {bool empty=true;bool HasEmptySeat(unsigned)const{return empty;}};
struct Unit:Position {
 Motion motion;BattlegroundSA* bg=nullptr;Unit* charmer=nullptr;
 bool alive=true,combat=false,vehicle=false,transport=false,npc=true,wanderer=true,mounted=true,lockout=false,moving=false;
 unsigned state=0,phase=1,teleports=0,auras=0,stops=0,visuals=0;
 Kit kit;Kit* GetVehicleKit(){return &kit;}Unit* GetVehicleBase(){return vehicle?this:nullptr;}
 float health=100.f;float GetHealthPct()const{return health;}
 using Position::Position;
 bool IsAlive()const{return alive;}bool IsInCombat()const{return combat;}bool HasUnitState(unsigned s)const{return state&s;}
 bool GetVehicle()const{return vehicle;}bool GetTransport()const{return transport;}
 bool IsNPCBot()const{return npc;}Unit* ToCreature(){return this;}Unit* GetCharmer(){return charmer;}
 BattlegroundSA* GetBotBG()const{return bg;}bool IsWandererBot()const{return wanderer;}
 unsigned GetGUID()const{return 1;}float GetBoundingRadius()const{return 0.5f;}unsigned GetPhaseMask()const{return phase;}
 Map* GetMap(){return &world;}
 bool InSamePhase(Unit const* u)const{return phase&u->phase;}bool IsWithinDistInMap(Unit const* u,float r)const{return GetExactDist(u)<=r;}
 bool isMoving()const{return moving;}Motion* GetMotionMaster(){return &motion;}void StopMoving(){moving=false;++stops;}
 void RemoveAurasByType(unsigned){mounted=false;}bool HasAura(unsigned id)const{return id==54643&&lockout;}
 void AddAura(unsigned id,Unit*){if(id!=54643)std::abort();lockout=true;++auras;}
 void CastSpell(Unit*,unsigned id,bool){if(id!=51347)std::abort();++visuals;}
 void NearTeleportTo(Position const& p){static_cast<Position&>(*this)=p;++teleports;}
 void UpdateAllowedPositionZ(float,float,float&){}
};
using Creature=Unit;
std::vector<Creature> triggers;
struct GameObject:Unit {
 bool spawned=true,disabled=false;
 bool isSpawned()const{return spawned;}bool HasFlag(unsigned)const{return disabled;}
 void GetCreatureListWithEntryInGrid(std::list<Creature*>& out,unsigned entry,float range){
  if(entry!=23472)std::abort();for(auto& t:triggers)if(GetExactDist(&t)<=range)out.push_back(&t);
 }
};
struct BattlegroundSA {
 unsigned status=1,attackers=0,team=1;bool solid=true,gunsEnabled=false;std::map<unsigned,GameObject> objects;Creature guns[10];
 unsigned GetTypeID()const{return BATTLEGROUND_SA;}unsigned GetStatus()const{return status;}
 unsigned GetAttackersTeam()const{return attackers;}unsigned GetBotTeamId(unsigned)const{return team;}
 GameObject* GetBGObject(unsigned id,bool){auto i=objects.find(id);return i==objects.end()?nullptr:&i->second;}
 Creature* GetBGCreature(unsigned i,bool){return gunsEnabled&&i<10?&guns[i]:nullptr;}
 bool IsBotGateSegmentClear(Vec const& a,Vec const& b,float,unsigned)const{return !solid||!((a.x<10&&b.x>=10)||(a.x>=10&&b.x<10));}
};
using Battleground=BattlegroundSA;
bool HumanNear(Unit*,float){return false;}
unsigned pathType=1;bool pathAvailable=true;
struct PathGenerator {
 Unit* actor;std::vector<Vec> points;
 PathGenerator(Unit* u):actor(u){}void SetUseStraightPath(bool b){if(!b)std::abort();}
 bool CalculatePath(float x,float y,float z,bool force){if(force)std::abort();points={{actor->x,actor->y,actor->z},{x,y,z}};return pathAvailable;}
 unsigned GetPathType()const{return pathType;}auto const& GetPath()const{return points;}
 bool CalculatePathFrom(Vec const& start,Vec const& end,bool force){if(force)std::abort();points={start,end};return pathAvailable;}
 Vec const& GetActualEndPosition()const{return points.back();}
};
Position const BG_SA_NpcSpawnlocs[10]={{0,0,10},{1000,0,10},{1000,0,10},{1000,0,10},{1000,0,10},{1000,0,10},{1000,0,10},{1000,0,10},{1000,0,10},{1000,0,10}};
HELPERS
struct Launcher {
 Unit* unit;unsigned stopped=0;
 struct {std::vector<Vec> path;struct {bool catmullrom=true,parabolic=false;}flags;float time_perc=0,parabolic_amplitude=0;}args;
 void Stop(){++stopped;}
 int Launch(std::vector<Vec> path,bool transport=false){
  args.path=path;Vec real_position{unit->x,unit->y,unit->z};
LAUNCH
  return 1;
 }
};
Position const BG_SA_ObjSpawnlocs[]={{1411.57f,108.163f,28.692f},{1055.452f,-108.1f,82.134f},
 {1431.3413f,-219.437f,30.893f},{1227.667f,-212.555f,55.372f},{1214.681f,81.21f,53.413f},
 {878.555f,-108.2f,117.845f}};
bool HasRole(unsigned){return false;}
void Check(bool b,char const* why){if(!b){std::cerr<<why<<'\n';std::exit(1);}}
Position CheckPost(BattlegroundSA* sa,bool attacker,uint32 gates,uint32 id){
 Creature storage;Creature* me=&storage;GameObject* objective=nullptr;GameObject* building=nullptr;Position destination;bool finalAssault=false;
POSTS
 if(!attacker){Check(!objective&&!finalAssault,"defender targets relic interaction");
  auto gate=AssaultGate(gates,(id/5)%2==0);if(gate==NoGate)gate=Ancient;
  Creature* gun=nullptr;
  if(sa->gunsEnabled&&SADefenderGunner(id)&&gate<Ancient){auto preferred=SACannonIndex(gate,id);for(unsigned index:{unsigned(preferred),unsigned(preferred^1)})
   if(sa->guns[index].alive&&sa->guns[index].health>=15.f&&sa->guns[index].kit.empty){gun=&sa->guns[index];break;}}
  if(gun)Check(destination.GetExactDist(gun)<0.01f,"gunner did not choose a free active-gate turret");
  else Check(destination.x>BG_SA_ObjSpawnlocs[gate].x+20.f,"defender sent behind active gate instead of in front");
 }else if(AssaultGate(gates,true)==NoGate)Check(objective==sa->GetBGObject(6,true)&&finalAssault,"attacker lost relic objective");
 return destination;
}
int main(){
 BattlegroundSA bg;for(unsigned i=0;i<7;++i){GameObject go;if(i<6)static_cast<Position&>(go)=BG_SA_ObjSpawnlocs[i];bg.objects.emplace(i,go);}
 GameObject platform;platform.x=0;platform.y=2;bg.objects.emplace(7,platform);triggers.emplace_back(20,2,0);
 Unit bot;bot.bg=&bg;Position goal(20,0,0);
 Check(!SAPathClear(&bot,goal,&bg),"path crosses solid gate");
 bg.solid=false;Check(SAPathClear(&bot,goal,&bg),"destroyed gate still blocks path");
 for(unsigned type=0;type<256;++type){pathType=type;Check(SAPathClear(&bot,goal,&bg)==(type==1||type==4),"bad path flags accepted");}pathType=1;
 bg.solid=true;
 Check(MoveSADefender(&bot,&bg,goal)&&bot.teleports==1&&bot.auras==1&&!bot.mounted,"defender did not use real platform landing");
 Check(!MoveSADefender(&bot,&bg,goal)&&bot.motion.goal.x==20&&bot.teleports==1,"defender failed to resume or bounced");
 for(unsigned team:{0u,1u}){
  bg.team=team;bg.attackers=1-team;
  Unit far(-20,0,0);far.bg=&bg;
  Check(!MoveSADefender(&far,&bg,goal)&&far.motion.moves==1&&far.motion.goal.x==0&&far.teleports==0,"remote defender teleported without approaching platform");
 }
 bg.team=1;bg.attackers=0;
 for(unsigned invalid=0;invalid<8;++invalid){
  Unit b;b.bg=&bg;
  if(invalid==0)b.alive=false;if(invalid==1)b.combat=true;if(invalid==2)b.vehicle=true;if(invalid==3)b.transport=true;
  if(invalid==4)b.state=UNIT_STATE_CASTING;if(invalid==5)b.state=UNIT_STATE_CONTROLLED;if(invalid==6)bg.team=bg.attackers;if(invalid==7)bg.status=0;
  Check(!MoveSADefender(&b,&bg,goal)&&b.teleports==0&&b.motion.moves==0,"invalid defender moved/teleported");bg.status=1;bg.team=1;
 }
 Unit locked;locked.bg=&bg;locked.lockout=true;Check(!MoveSADefender(&locked,&bg,goal)&&locked.teleports==0,"native lockout ignored");
 auto saved=triggers;triggers.clear();Unit missing;missing.bg=&bg;Check(!MoveSADefender(&missing,&bg,goal)&&!missing.teleports,"missing landing allowed transfer");triggers=saved;
 for(unsigned mask=0;mask<64;++mask)for(bool attacker:{false,true})for(unsigned id:{0u,1u,6u,11u})CheckPost(&bg,attacker,mask,id);
 bg.gunsEnabled=true;for(unsigned i=0;i<10;++i)bg.guns[i].x=100.f+float(i)*10.f;
 for(unsigned mask:{0u,5u,29u,31u})for(unsigned id:{1u,2u,6u,7u}){
  for(auto& gun:bg.guns){gun.kit.empty=true;gun.health=100.f;}
  CheckPost(&bg,false,mask,id);
  auto gate=AssaultGate(mask,(id/5)%2==0);auto preferred=SACannonIndex(gate,id);
  if(preferred<10){bg.guns[preferred].kit.empty=false;auto point=CheckPost(&bg,false,mask,id);
   Check(point.GetExactDist(&bg.guns[preferred^1])<0.01f,"second turret is not used when first is occupied");
   bg.guns[preferred^1].kit.empty=false;CheckPost(&bg,false,mask,id);
   bg.guns[preferred].kit.empty=true;bg.guns[preferred].health=14.f;CheckPost(&bg,false,mask,id);}
 }
 bg.gunsEnabled=false;
 Unit mover;mover.bg=&bg;Launcher init{&mover};
 for(auto path:{std::vector<Vec>{{0,0,0},{20,0,0}},std::vector<Vec>{{0,0,0},{5,5,0},{20,0,0}}}){
  Check(init.Launch(path)==0&&init.args.path.empty()&&init.stopped,"direct/chase spline crosses gate");
 }
 Check(init.Launch({{999,0,0},{20,0,0}})==0,"unchecked stale spline origin used");
 Check(init.Launch({{0,0,0},{5,0,0}})==1&&!init.args.flags.catmullrom,"safe movement blocked or unchecked curve allowed");
 bg.solid=false;Check(init.Launch({{0,0,0},{20,0,0}})==1,"breach fails to reopen movement");bg.solid=true;
 Unit driver;driver.bg=&bg;Unit vehicle;vehicle.npc=false;vehicle.charmer=&driver;Launcher tank{&vehicle};
 Check(tank.Launch({{0,0,0},{20,0,0}})==0,"bot-driven vehicle crosses gate");
 mover.npc=false;Check(init.Launch({{0,0,0},{20,0,0}})==1,"human movement changed");
 bg.solid=false;world.ground=0;Unit top(0,0,10);top.bg=&bg;
 Check(TrySADefenderDescent(&top,&bg,goal)&&top.motion.jumps==1,"native platform descent not used");
 Unit below;below.bg=&bg;Check(TrySADefenderDescent(&below,&bg,goal)&&below.motion.jumps==0&&below.motion.moves==1,"bot jumped without reaching wall platform");
 world.los=false;Unit blocked(0,0,10);blocked.bg=&bg;Check(!TrySADefenderDescent(&blocked,&bg,goal)&&blocked.motion.jumps==0,"descent through static obstacle allowed");
 std::cout<<"PASS: production SA path/spline guards, chase/direct/vehicle movement, defender platform approach/transfer/lockout, both teams and 64 final-defense states\n";
}
'''
code=code.replace('HELPERS',helpers).replace('LAUNCH',launch).replace('POSTS',posts)
with tempfile.TemporaryDirectory(prefix='sa-gate-movement-') as directory:
    cpp,exe=Path(directory)/'test.cpp',Path(directory)/'test'
    cpp.write_text(code)
    subprocess.run(['c++','-std=c++20','-O2','-Wall','-Wextra','-Wno-misleading-indentation','-Wno-missing-field-initializers','-I',str(repo),str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
