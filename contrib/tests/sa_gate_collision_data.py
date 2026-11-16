#!/usr/bin/env python3
"""Run the production SA segment guard against installed native WMO models."""
from pathlib import Path
import re
import csv
import shlex
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
build = Path('/home/trinity/build/npcbots')
source = (repo / 'src/server/game/Battlegrounds/Zones/BattlegroundSA.cpp').read_text()
guard = source[source.index('bool BattlegroundSA::IsBotGateSegmentClear('):source.index('void BattlegroundSAScore::')]
header = (repo / 'src/server/game/Battlegrounds/Zones/BattlegroundSA.h').read_text()
positions = header.split('Position const BG_SA_ObjSpawnlocs')[1].split('// Portal')[0]
positions = re.findall(r'\{\s*([^{}]+)\}', positions)[:6]
assert len(positions) == 6
setup = '\n'.join('bg.objects.emplace_back(Position{' + p + '},' + ('8165' if i == 5 else '7906') + ',data);' for i, p in enumerate(positions))
portals = header.split('// Portal')[1].split('// Ships')[0]
portals = re.findall(r'\{\s*([^{}]+)\}', portals)
assert len(portals) == 5
portal_setup = ','.join('{' + p + '}' for p in portals)
cannons = header.split('Position const BG_SA_NpcSpawnlocs')[1].split('// Demolishers')[0]
cannons = re.findall(r'\{\s*([^{}]+)\}', cannons)[:10]
cannon_setup = ','.join('{' + p + '}' for p in cannons)
spawn_section=header.split('Position const BG_SA_NpcSpawnlocs')[1].split('enum BG_SA_Objects')[0]
beach_spawns=re.findall(r'\{\s*([^{}]+)\}',spawn_section)[10:14]
assert len(beach_spawns)==4
waypoints=[]
for line in (repo/'sql/custom/world/2026_10_23_01_battleground_bots_sa_ic.sql').read_text().splitlines():
    if line.startswith('(52'):
        fields=next(csv.reader([line[1:line.rfind(')')]],quotechar="'",skipinitialspace=True))
        waypoints.append('{'+','.join(fields[11:14])+',0}')
code = r'''
#include "GameObjectModel.h"
#include "ModelIgnoreFlags.h"
#include "DetourNavMeshQuery.h"
#include "MapDefines.h"
#include "VMapFactory.h"
#include "IVMapManager.h"
#include "VMapManager2.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <list>
#include <queue>
#include <memory>
#include <stdexcept>
#include <vector>
#include <filesystem>
#include <fstream>
using uint32=std::uint32_t;using uint8=std::uint8_t;
enum {MAX_GATES=6,BG_SA_GATE_OK=1,BG_SA_GATE_DAMAGED=2,BG_SA_GATE_DESTROYED=3};
struct Position {float x,y,z,o;};
struct Owner:GameObjectModelOwnerBase {
 Position p;unsigned display;
 Owner(Position a,unsigned b):p(a),display(b){}
 bool IsSpawned()const override{return true;}uint32 GetDisplayId()const override{return display;}
 uint32 GetPhaseMask()const override{return 1;}
 G3D::Vector3 GetPosition()const override{return {p.x,p.y,p.z};}
 float GetOrientation()const override{return p.o;}float GetScale()const override{return 1.f;}
 void DebugVisualizeCorner(G3D::Vector3 const&)const override{}
};
struct GameObject {
 Position p;bool spawned=true;GameObjectModel* m_model;
 GameObject(Position a,unsigned display,std::string const& data):p(a),m_model(GameObjectModel::Create(std::make_unique<Owner>(a,display),data)){}
 bool isSpawned()const{return spawned;}bool InSamePhase(unsigned mask)const{return mask&1;}
 float GetPositionX()const{return p.x;}float GetPositionY()const{return p.y;}float GetPositionZ()const{return p.z;}
};
struct BattlegroundSA {
 unsigned GateStatus[MAX_GATES]={1,1,1,1,1,1};std::vector<GameObject> objects;
 GameObject* GetBGObject(unsigned i,bool){return i<objects.size()?&objects[i]:nullptr;}
 bool IsBotGateSegmentClear(G3D::Vector3 const&,G3D::Vector3 const&,float,uint32);
};
GUARD
void Require(bool ok,std::string const& message){if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv){try{
 Require(argc==2,"Pass the server data directory");std::string data=argv[1];if(data.back()!='/')data+='/';
 LoadGameObjectModelList(data);BattlegroundSA bg;SETUP
 for(auto const& go:bg.objects)Require(go.m_model&&go.m_model->isEnabled(),"Native gate WMO unavailable");
 unsigned checks=0;
 for(unsigned mask=0;mask<64;++mask){
  for(unsigned g=0;g<6;++g)bg.GateStatus[g]=(mask&(1u<<g))?BG_SA_GATE_DESTROYED:BG_SA_GATE_OK;
  for(unsigned g=0;g<6;++g){
   auto p=bg.objects[g].p;G3D::Vector3 center(p.x,p.y,p.z+2.f),normal(std::cos(p.o),std::sin(p.o),0.f);
   auto a=center+normal*30.f,b=center-normal*30.f;
   for(bool reverse:{false,true}){
    Require(bg.IsBotGateSegmentClear(reverse?b:a,reverse?a:b,0.5f,1)==bool(mask&(1u<<g)),"Gate crossing mismatch: "+std::to_string(g)+" mask "+std::to_string(mask));++checks;
   }
  }
 }
 for(auto& state:bg.GateStatus)state=BG_SA_GATE_OK;
 Require(bg.IsBotGateSegmentClear({1600,-106,9},{1618,61,7},0.5f,1),"Unrelated beach road blocked");
 auto& gate=bg.objects[0];auto* model=gate.m_model;gate.m_model=nullptr;
 Require(!bg.IsBotGateSegmentClear({gate.p.x+30,gate.p.y,gate.p.z},{gate.p.x-30,gate.p.y,gate.p.z},0.5f,1),"Missing model allowed gate shortcut");
 Require(bg.IsBotGateSegmentClear({1600,-106,9},{1618,61,7},0.5f,1),"Missing model blocked unrelated road");gate.m_model=model;
 gate.m_model->disable();
 Require(!bg.IsBotGateSegmentClear({gate.p.x+30,gate.p.y,gate.p.z},{gate.p.x-30,gate.p.y,gate.p.z},0.5f,1),"Disabled model allowed gate shortcut");gate.m_model->enable(1);
 Require(bg.IsBotGateSegmentClear({gate.p.x+30,gate.p.y,gate.p.z},{gate.p.x-30,gate.p.y,gate.p.z},0.5f,2),"Wrong-phase gate blocks movement");
 // Real navmesh routes to the platforms and on from their native 23472 landings.
 std::filesystem::path mmap=std::filesystem::path(data)/"mmaps";dtNavMeshParams params{};
 std::ifstream mapFile(mmap/"607.mmap",std::ios::binary);Require(bool(mapFile.read(reinterpret_cast<char*>(&params),sizeof params)),"Missing SA navmesh");
 auto mesh=std::unique_ptr<dtNavMesh,decltype(&dtFreeNavMesh)>(dtAllocNavMesh(),dtFreeNavMesh);
 Require(dtStatusSucceed(mesh->init(&params)),"Cannot initialize navmesh");
 for(auto const& file:std::filesystem::directory_iterator(mmap)){
  if(file.path().filename().string().rfind("607",0)!=0||file.path().extension()!=".mmtile")continue;
  std::ifstream in(file.path(),std::ios::binary);MmapTileHeader h;
  Require(bool(in.read(reinterpret_cast<char*>(&h),sizeof h))&&h.mmapMagic==MMAP_MAGIC&&h.dtVersion==DT_NAVMESH_VERSION&&h.mmapVersion==MMAP_VERSION,"Invalid SA tile");
  auto* bytes=static_cast<unsigned char*>(dtAlloc(h.size,DT_ALLOC_PERM));Require(bytes!=nullptr,"Allocation failed");
  Require(bool(in.read(reinterpret_cast<char*>(bytes),h.size))&&dtStatusSucceed(mesh->addTile(bytes,h.size,DT_TILE_FREE_DATA,0,nullptr)),"Cannot load SA tile");
 }
 dtNavMeshQuery query;Require(dtStatusSucceed(query.init(mesh.get(),4096)),"Cannot initialize query");dtQueryFilter filter;filter.setIncludeFlags(NAV_GROUND|NAV_WATER);
 auto* vmap=VMAP::VMapFactory::createOrGetVMapManager();
 for(auto const& file:std::filesystem::directory_iterator(std::filesystem::path(data)/"vmaps")){
  auto name=file.path().filename().string();unsigned map,x,y;if(file.path().extension()==".vmtile"&&std::sscanf(name.c_str(),"%u_%u_%u.vmtile",&map,&x,&y)==3&&map==607)
   Require(vmap->loadMap((data+"vmaps").c_str(),607,x,y)==1,"Cannot load static SA vmap");
 }
 auto safePath=[&](Position const& a,Position const& b){
  float start[]={a.y,a.z,a.x},end[]={b.y,b.z,b.x},extent[]={8.f,80.f,8.f},sn[3],en[3];dtPolyRef sp=0,ep=0;
  if(dtStatusFailed(query.findNearestPoly(start,extent,&filter,&sp,sn))||!sp||dtStatusFailed(query.findNearestPoly(end,extent,&filter,&ep,en))||!ep)return false;
  dtPolyRef corridor[4096];int count=0;
  if(dtStatusFailed(query.findPath(sp,ep,sn,en,&filter,corridor,&count,4096))||!count||corridor[count-1]!=ep)return false;
  float points[4096*3];int size=0;if(dtStatusFailed(query.findStraightPath(sn,en,corridor,count,points,nullptr,nullptr,&size,4096))||size<1)return false;
  G3D::Vector3 from(a.x,a.y,a.z);
  for(int i=1;i<size;++i){G3D::Vector3 to(points[i*3+2],points[i*3],points[i*3+1]);if(!bg.IsBotGateSegmentClear(from,to,0.5f,1))return false;from=to;}
  return true;
 };
 Position platforms[]={PORTALS};
 // Native world-trigger coordinates verified in world2 (entry 23472, map 607).
 Position landings[]={{1441.08f,-240.594f,35.3471f,0},{1389.06f,102.941f,34.5742f,0},
  {1043.52f,-87.8507f,87.197f,0},{1193.76f,69.9405f,58.1295f,0},{1227.89f,-235.458f,60.1112f,0}};
 Position cannons[]={CANNONS};Position roads[]={ROADS};
 struct Front{unsigned mask,gate;int gun=-1;};
 unsigned routes=0;
 for(auto front:{Front{0,0},Front{0,2},Front{5,3},Front{5,4},Front{29,1},Front{31,5},Front{63,5},
  Front{0,0,0},Front{0,0,1},Front{0,2,4},Front{0,2,5},Front{5,3,6},Front{5,3,7},Front{5,4,8},Front{5,4,9},Front{29,1,2},Front{29,1,3}}){
  for(unsigned g=0;g<6;++g)bg.GateStatus[g]=(front.mask&(1u<<g))?BG_SA_GATE_DESTROYED:BG_SA_GATE_OK;
  auto goal=bg.objects[front.gate].p;goal.x+=35.f;
  if(front.gun>=0)goal=cannons[front.gun];
  Position current{1209.7f,-65.16f,70.1f,0};bool arrived=false;
  std::vector<Position> graph{current,goal};for(auto p:cannons)graph.push_back(p);for(auto p:platforms)graph.push_back(p);for(auto p:landings)graph.push_back(p);for(auto p:roads)graph.push_back(p);
  std::vector<std::pair<unsigned,unsigned>> drops;
  for(unsigned gun=0;gun<10;++gun)for(unsigned angle=0;angle<8;++angle){
   auto from=cannons[gun];float a=angle*3.14159265f/4;float pos[]={from.y+std::sin(a)*12.f,from.z-12.f,from.x+std::cos(a)*12.f};
   pos[1]=vmap->getHeight(607,pos[2],pos[0],from.z+2.f,30.f);if(pos[1]<-1000.f)pos[1]=from.z-12.f;
   float ext[]={3.f,5.f,3.f},near[3];dtPolyRef ref=0;
   if(dtStatusFailed(query.findNearestPoly(pos,ext,&filter,&ref,near))||!ref||from.z-near[1]<3.f||from.z-near[1]>20.f)continue;
   Position to{near[2],near[0],near[1],0};bool clear=true;G3D::Vector3 previous(from.x,from.y,from.z);
   for(unsigned sample=1;sample<=16&&clear;++sample){float t=sample/16.f;G3D::Vector3 next(from.x+(to.x-from.x)*t,from.y+(to.y-from.y)*t,from.z+(to.z-from.z)*t+4.f*(7.5f*7.5f/(2.f*19.29110527f))*t*(1-t));clear=bg.IsBotGateSegmentClear(previous,next,0.5f,1)&&vmap->isInLineOfSight(607,previous.x,previous.y,previous.z+1,next.x,next.y,next.z+1,VMAP::ModelIgnoreFlags::Nothing);previous=next;}
   if(clear){drops.emplace_back(gun+2,graph.size());graph.push_back(to);}
  }
  std::vector<int> previous(graph.size(),-1);previous[0]=0;std::queue<unsigned> pending;pending.push(0);
  while(!pending.empty()){
   auto from=pending.front();pending.pop();if(from==1){arrived=true;break;}
   for(unsigned to=1;to<graph.size();++to)if(previous[to]<0&&((from>=12&&from<17&&to==from+5)||std::find(drops.begin(),drops.end(),std::pair<unsigned,unsigned>{from,to})!=drops.end()||safePath(graph[from],graph[to]))){previous[to]=from;pending.push(to);}
  }
  if(!arrived){std::cerr<<"Reachable:";for(unsigned i=0;i<previous.size();++i)if(previous[i]>=0)std::cerr<<" "<<i;std::cerr<<"\n";
   for(auto pos:{goal,landings[1],platforms[1],cannons[0]}){float p[]={pos.y,pos.z,pos.x},e[]={8.f,80.f,8.f},near[3];dtPolyRef ref=0;query.findNearestPoly(p,e,&filter,&ref,near);std::cerr<<"projection "<<pos.x<<","<<pos.y<<","<<pos.z<<" => "<<near[2]<<","<<near[0]<<","<<near[1]<<"\n";}
  }
  Require(arrived,"No solid-gate-safe defender route to front "+std::to_string(front.gate)+" mask "+std::to_string(front.mask));++routes;
 }
 for(auto& state:bg.GateStatus)state=BG_SA_GATE_OK;
 Position beachVehicles[]={BEACH};unsigned approaches=0;
 for(auto from:{Position{1600.381f,-106.263f,8.8745f,0},Position{1618.047729f,61.424641f,7.248210f,0},Position{1611.597656f,-117.270073f,8.719355f,0}})
  for(auto to:beachVehicles){Require(safePath(from,to),"Beach vehicle unreachable from opposite harbor");++approaches;}
 std::cout<<"PASS: production collision guard, 6 native WMO gates, 64 masks, "<<checks<<" physical crossings, missing/disabled models and phase guards\n";
 std::cout<<"PASS: real SA navmesh/static vmaps, "<<routes<<" defender fronts/turrets via native platforms/landings, short wall descents or open roads\n";
 std::cout<<"PASS: "<<approaches<<" beach vehicle approaches from round spawn and both harbors\n";
 for(auto& go:bg.objects)delete go.m_model;
}catch(std::exception const& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}}
'''
code = code.replace('GUARD', guard).replace('SETUP', setup).replace('PORTALS', portal_setup).replace('CANNONS',cannon_setup).replace('ROADS',','.join(waypoints)).replace('BEACH',','.join('{'+p+'}' for p in beach_spawns))
includes = ['src/common','src/common/Utilities','src/common/Collision','src/common/Collision/Management','src/common/Collision/Models','src/common/Collision/Maps','dep/g3dlite/include','dep/recastnavigation/Detour/Include','dep/fmt/include']
link = shlex.split((build / 'src/server/worldserver/CMakeFiles/worldserver.dir/link.txt').read_text())
link = link[link.index('../../common/libcommon.a'):]
link = [str((build / 'src/server/worldserver' / token).resolve()) if token.startswith('..') else token for token in link]
with tempfile.TemporaryDirectory(prefix='sa-native-gate-') as directory:
    cpp, exe = Path(directory)/'test.cpp', Path(directory)/'test'
    cpp.write_text(code)
    subprocess.run(['c++','-std=c++20','-O2','-pthread',*[flag for inc in includes for flag in ('-I',str(repo/inc))],str(cpp),*link,'-o',str(exe)],check=True)
    subprocess.run([str(exe),'/home/trinity/server/data'],check=True)
