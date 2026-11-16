#!/usr/bin/env python3
"""Execute production IC teleporter handling with native GO/trigger fixtures."""
from pathlib import Path
import re
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
source = (repo / "src/server/game/AI/NpcBots/bot_battlegrounds.cpp").read_text()
header = (repo / "src/server/game/Battlegrounds/Zones/BattlegroundIC.h").read_text()
helper = source[source.index("bool UseICKeepTeleporter("):source.index("bool SAPathClear(")]
prefix = source[source.index("bool bot_ai::MoveToSiegeObjective("):source.index("    auto allowed =", source.index("bool bot_ai::MoveToSiegeObjective("))]
prefix = prefix.replace("bool bot_ai::", "bool Controller::") + "    return true;\n}\n"
portals = re.findall(r"\{(BG_IC_GO_TELEPORTER_\d_\d), (GO_TELEPORTER_\d), ([^}]+)\}", header)
assert len(portals) == 12
native = (repo / "sql/old/3.3.5a/TDB52_to_TDB53_updates/world/2013_09_04_03_world_isle_of_conquest.sql").read_text()
triggers = re.findall(r"\(@CGUID\+\d+, 22515, 628, 3, 1, ([^,]+), ([^,]+), ([^,]+), ([^,]+), 7200", native)
assert len(triggers) == 14
definitions = "enum {" + ",".join(p[0] for p in portals) + "};\n"
spawns = "ICGo const BG_IC_Teleporters[]={\n" + ",\n".join("{" + ",".join(p) + "}" for p in portals) + "};\n"
setup = "\n".join("triggers.emplace_back(Position{" + ",".join(t) + "});" for t in triggers)
code = r'''
#include "src/server/game/AI/NpcBots/botbgtactics.h"
#include <vector>
#include <list>
#include <map>
#include <cmath>
#include <iostream>
#include <cstdlib>
using uint32=std::uint32_t; using uint8=std::uint8_t;
using namespace NPCBots::SiegeBG;
enum TeamId {TEAM_ALLIANCE,TEAM_HORDE};
enum { BATTLEGROUND_IC, BATTLEGROUND_SA=2, STATUS_IN_PROGRESS=1, GO_FLAG_NOT_SELECTABLE=1,
UNIT_STATE_CASTING=1, UNIT_STATE_CONTROLLED=2, MOTION_PRIORITY_NORMAL=1,
SPELL_AURA_MOUNTED=1, SPELL_TELEPORT_VISUAL_ONLY=51347,
GO_TELEPORTER_1=195314,GO_TELEPORTER_2=195313,GO_TELEPORTER_3=195315,GO_TELEPORTER_4=195316 };
float constexpr INTERACTION_DISTANCE=5.f;
struct Position {
 float x=0,y=0,z=0,o=0;
 float GetPositionX()const{return x;} float GetPositionY()const{return y;}
 float GetExactDist(Position const* p)const{return std::sqrt((x-p->x)*(x-p->x)+(y-p->y)*(y-p->y)+(z-p->z)*(z-p->z));}
 Position const& GetPosition()const{return *this;}
};
struct Motion {
 unsigned clears=0,moves=0; Position goal;
 void Clear(unsigned){++clears;}
 void MovePoint(unsigned,Position const& p,bool){goal=p;++moves;}
};
struct Unit:Position {
 Motion motion; unsigned state=0,phase=1,teleports=0,visuals=0; bool alive=true,combat=false,mounted=true,vehicle=false,transport=false;
 Unit()=default; Unit(Position const& p):Position(p){}
 bool IsAlive()const{return alive;} bool IsInCombat()const{return combat;}
 bool HasUnitState(unsigned mask)const{return state&mask;}
 bool GetVehicle()const{return vehicle;} bool GetTransport()const{return transport;}
 bool CanBotMoveVehicle()const{return false;} Unit* GetVehicleBase(){return this;}
 unsigned GetGUID()const{return 1;}
 bool isMoving()const{return motion.moves>0;}
 void StopMoving(){} Motion* GetMotionMaster(){return &motion;}
 bool InSamePhase(Unit const* p)const{return phase&p->phase;}
 bool IsWithinDistInMap(Unit const* p,float r)const{return GetExactDist(p)<=r;}
 void RemoveAurasByType(unsigned){mounted=false;}
 void CastSpell(Unit*,unsigned id,bool){if(id==SPELL_TELEPORT_VISUAL_ONLY)++visuals;}
 void NearTeleportTo(Position const& p){static_cast<Position&>(*this)=p;++teleports;}
};
using Creature=Unit;
std::vector<Creature> triggers;
struct GameObject:Unit {
 unsigned entry=0; bool spawned=true,disabled=false;
 unsigned GetEntry()const{return entry;}
 bool isSpawned()const{return spawned;} bool HasFlag(unsigned)const{return disabled;}
 void GetCreatureListWithEntryInGrid(std::list<Creature*>& out,unsigned id,float radius)const {
  if(id!=22515)std::abort();
  for(auto& t:triggers)if(GetExactDist(&t)<=radius)out.push_back(&t);
 }
};
struct ICGo {unsigned type,entry;float x,y,z,o;};
DEFINITIONS
SPAWNS
struct Battleground {
 unsigned type=BATTLEGROUND_IC,status=STATUS_IN_PROGRESS;TeamId team=TEAM_HORDE;std::map<unsigned,GameObject> objects;
 unsigned GetTypeID()const{return type;} unsigned GetStatus()const{return status;}
 TeamId GetBotTeamId(unsigned)const{return team;}
 GameObject* GetBGObject(unsigned id,bool){auto it=objects.find(id);return it==objects.end()?nullptr:&it->second;}
};
using BattlegroundSA=Battleground;
bool MoveSADefender(Creature*,BattlegroundSA*,Position const&){return false;}
HELPER
struct Controller {
 Creature* me;Battleground* bg;uint32 _siegeTravelNode=5320,_siegeThinkTimer=1000,_siegeMountTimer=0;
 Battleground* GetBG(){return bg;} bool CanBotMoveVehicle(){return false;}
 bool MoveToSiegeObjective(Position const&,float,uint32,bool);
};
PREFIX
void Check(bool yes,char const* why){if(!yes){std::cerr<<why<<'\n';std::exit(1);}}
int main(){
 SETUP
 Battleground bg;
 for(auto const& s:BG_IC_Teleporters){GameObject go;static_cast<Position&>(go)={s.x,s.y,s.z,s.o};go.entry=s.entry;bg.objects.emplace(s.type,go);}
 unsigned traversals=0;
 for(auto const& s:BG_IC_Teleporters){
  auto* entrance=&bg.objects.at(s.type);
  TeamId team=s.entry==GO_TELEPORTER_1||s.entry==GO_TELEPORTER_2?TEAM_HORDE:TEAM_ALLIANCE;
  bg.team=team;
  bool leaving=ICPositionKeep(s.x,s.y)==team;
  Creature bot(entrance->GetPosition());Controller ai{&bot,&bg};
  Position goal=leaving?Position{776.23f,-804.28f,6.45f}:team==TEAM_HORDE?Position{1284.76f,-705.67f,48.92f}:Position{299.15f,-784.59f,48.92f};
  Check(!ai.MoveToSiegeObjective(goal,3.f,0,true),"teleport tick must stop planning");
  Check(bot.teleports==1&&!bot.mounted&&bot.visuals==1,"platform did not transfer infantry");
  Check((ICPositionKeep(bot.x,bot.y)==team)!=leaving,"landed on same side of wall");
  Check(ai._siegeTravelNode==0&&ai._siegeThinkTimer==2000,"old checkpoint survives teleport");
  bool nativeLanding=false;for(auto const& t:triggers)if(bot.GetExactDist(&t)<0.01f)nativeLanding=true;
  Check(nativeLanding,"not a native landing trigger");
  Check(ai.MoveToSiegeObjective(goal,3.f,0,true),"teleporter bounced bot back instead of continuing to goal");
  Check(bot.teleports==1,"unexpected return teleport");
  ++traversals;
  for(unsigned blocked=0;blocked<10;++blocked){
   Creature b(entrance->GetPosition());bg.status=STATUS_IN_PROGRESS;entrance->spawned=true;entrance->disabled=false;
   if(blocked==0)b.x+=20;if(blocked==1)b.combat=true;if(blocked==2)b.vehicle=true;if(blocked==3)b.transport=true;
   if(blocked==4)b.state=UNIT_STATE_CONTROLLED;if(blocked==5)b.state=UNIT_STATE_CASTING;
   if(blocked==6)b.alive=false;if(blocked==7)entrance->disabled=true;if(blocked==8)entrance->spawned=false;if(blocked==9)bg.status=0;
   Check(!UseICKeepTeleporter(&b,&bg,entrance,team)&&b.teleports==0,"invalid interaction allowed teleport");
  }
  bg.status=STATUS_IN_PROGRESS;entrance->spawned=true;entrance->disabled=false;
  Creature wrong(entrance->GetPosition());Check(!UseICKeepTeleporter(&wrong,&bg,entrance,team==TEAM_HORDE?TEAM_ALLIANCE:TEAM_HORDE),"enemy platform allowed");
  Creature b(entrance->GetPosition());b.phase=2;Check(!UseICKeepTeleporter(&b,&bg,entrance,team),"wrong phase allowed");
  auto saved=triggers;triggers.clear();Creature missing(entrance->GetPosition());
  Check(!UseICKeepTeleporter(&missing,&bg,entrance,team),"missing landing allowed teleport");triggers=saved;
  Creature breach(entrance->GetPosition());Controller breached{&breach,&bg};
  Check(breached.MoveToSiegeObjective(goal,3.f,team==TEAM_HORDE?7:56,true)&&breach.teleports==0,"breached keep forced teleporter");
 }
 // Both spawn groups must approach a selectable platform without crossing a wall.
 for(auto team:{TEAM_ALLIANCE,TEAM_HORDE}){
  bg.team=team;Creature bot(team==TEAM_HORDE?Position{1284.76f,-705.67f,48.92f}:Position{299.15f,-784.59f,48.92f});Controller ai{&bot,&bg};
  ai.MoveToSiegeObjective({776.23f,-804.28f,6.45f},3.f,0,true);
  Check(bot.motion.moves==1&&bot.teleports==0,"spawn did not approach platform");
  Check(ICPositionKeep(bot.motion.goal.x,bot.motion.goal.y)==team,"approach selected exterior platform");
 }
 std::cout<<"PASS: "<<traversals<<" native keep platforms, both factions/directions, no bounce, gates, spawn approach, interaction guards and missing landing.\n";
}
'''
for key, value in {"DEFINITIONS": definitions,"SPAWNS": spawns,"HELPER": helper,"PREFIX": prefix,"SETUP": setup}.items():
    code = code.replace(key, value)
with tempfile.TemporaryDirectory(prefix="ioc-keep-portals-") as directory:
    cpp = Path(directory) / "test.cpp"
    exe = Path(directory) / "test"
    cpp.write_text(code)
    subprocess.run(["c++", "-std=c++20", "-O2", "-Wall", "-Wextra", "-Wno-misleading-indentation", "-Wno-unused-parameter", "-I", str(repo), str(cpp), "-o", str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
