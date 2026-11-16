#!/usr/bin/env python3
"""Exercise the production ground projection against installed Wintergrasp mmaps.

Requires the existing /home/trinity/build/npcbots common/Detour libraries and data.
No worldserver, database connection or player session is started.
"""
from pathlib import Path
import shlex
import subprocess
import tempfile
repo=Path(__file__).resolve().parents[2]
build=Path('/home/trinity/build/npcbots')
s=(repo/'src/server/scripts/Northrend/zone_wintergrasp.cpp').read_text()
start=s.index('bool ProjectBotWarGround(');end=s.index('{',start)+1;depth=1
while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
projection=s[start:end]
code=r'''
#include "MMapFactory.h"
#include "MapDefines.h"
#include "src/server/scripts/Northrend/wintergrasp_vehicle_navigation.h"
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
struct Position
{
 float m_positionX,m_positionY,m_positionZ;
 float GetPositionX()const{return m_positionX;}float GetPositionY()const{return m_positionY;}float GetPositionZ()const{return m_positionZ;}
};
struct Map
{
 void LoadGrid(float,float){}
 unsigned GetId()const{return 571;}unsigned GetInstanceId()const{return 0;}
};
PROJECTION
int main()
{
 auto* mmap=MMAP::MMapFactory::createOrGetMMapManager();mmap->InitializeThreadUnsafe({571});
 for(int x=20;x<=25;++x)for(int y=24;y<=29;++y)mmap->loadMap("/home/trinity/server/data/",571,x,y);
 assert(mmap->loadMapInstance("/home/trinity/server/data/",571,0));
 auto* query=mmap->GetNavMeshQuery(571,0);assert(query);
 Map map;
 std::array<Position,11> points={{{4550,2840,385},{4750,2840,380},{4920,2840,375},{5050,2840,385},
  {4948.524f,3342.338f,376.875f},{4949.344f,2432.586f,320.177f},{4390.78f,3304.09f,372.43f},
  {4398.08f,2356.50f,376.19f},{5100,2841,390},{5100,3100,385},{5110,2680,385}}};
 std::array<dtPolyRef,11> polygons{};dtQueryFilter filter;filter.setIncludeFlags(NAV_GROUND|NAV_WATER);
 unsigned badOriginalHeights=0;
 for(unsigned i=0;i<points.size();++i)
 {
  auto old=points[i];assert(ProjectBotWarGround(&map,points[i],true));
  if(std::fabs(old.m_positionZ-points[i].m_positionZ)>7)++badOriginalHeights;
  float p[]={points[i].m_positionY,points[i].m_positionZ,points[i].m_positionX},e[]={3,5,3},nearest[3];
  assert(dtStatusSucceed(query->findNearestPoly(p,e,&filter,&polygons[i],nearest))&&polygons[i]);
  assert(std::fabs(p[1]-nearest[1])<0.5f);
 }
 assert(badOriginalHeights>=4); // the real installed map reproduces the regression that flat mock maps missed
 auto connected=[&](int first,int last,unsigned mask) {
  dtQueryFilter routeFilter;routeFilter.setIncludeFlags(mask);
  auto a=points[first],b=points[last];float p[]={a.m_positionY,a.m_positionZ,a.m_positionX},q[]={b.m_positionY,b.m_positionZ,b.m_positionX};
  dtPolyRef current=polygons[first];
  for(unsigned attempt=0;attempt<16;++attempt)
  {
   dtPolyRef corridor[2048];int count=0;
   if(!dtStatusSucceed(query->findPath(current,polygons[last],p,q,&routeFilter,corridor,&count,2048))||!count)return false;
   if(corridor[count-1]==polygons[last])return true;
   if(corridor[count-1]==current)return false;
   current=corridor[count-1];
   float nearest[3];if(!dtStatusSucceed(query->closestPointOnPoly(current,q,nearest,nullptr)))return false;
   std::copy(nearest,nearest+3,p); // simulate following the valid prefix before replanning
  }
  return false;
 };
 for(auto pair:std::array<std::array<int,2>,6>{{{0,1},{1,2},{2,3},{6,4},{7,5},{8,2}}})
 {
  unsigned ordinary=NAV_GROUND|NAV_WATER|NAV_MAGMA_SLIME;
  bool standard=connected(pair[0],pair[1],ordinary);
  assert(standard);
 }
 using namespace WintergraspBotWar;
 assert(Navigation::VerifiedInfantryPath(132,3)&&!Navigation::VerifiedPath(132,3));
 assert(!Navigation::VerifiedInfantryPath(17,3)&&!Navigation::VerifiedInfantryPath(8,3));
 std::cout<<"PASS: actual installed mmaps reproduce "<<badOriginalHeights<<" bad Z hints; production projection fixes all eleven samples and six assault/counterattack corridors remain connected\n";
 MMAP::MMapFactory::clear();
}
'''.replace('PROJECTION',projection)
flags=[]
for line in (build/'src/common/CMakeFiles/common.dir/flags.make').read_text().splitlines():
    if line.startswith(('CXX_DEFINES =','CXX_INCLUDES =')):flags+=shlex.split(line.split('=',1)[1])
libs=[str(build/p) for p in ['src/common/libcommon.a','dep/fmt/libfmt.a','dep/g3dlite/libg3dlib.a','dep/SFMT/libsfmt.a','dep/recastnavigation/Detour/libDetour.a']]
with tempfile.TemporaryDirectory(prefix='wg-real-map-') as temp:
    cpp,binary=Path(temp)/'test.cpp',Path(temp)/'test';cpp.write_text(code)
    command=['c++','-std=c++20','-O2',*flags,'-UNDEBUG','-I',str(repo),str(cpp),'-o',str(binary),'-Wl,--start-group',*libs,'-Wl,--end-group','-lboost_filesystem','-lboost_program_options','-lboost_regex','-lboost_locale','-lboost_chrono','-lboost_system','-lboost_thread','-lboost_atomic','-lssl','-lcrypto','-lz','-ldl','-lpthread']
    subprocess.run(command,check=True)
    subprocess.run([str(binary)],check=True)
