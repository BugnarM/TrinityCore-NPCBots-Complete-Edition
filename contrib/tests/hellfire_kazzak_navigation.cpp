#include "MMapFactory.h"
#include "VMapFactory.h"
#include "VMapManager2.h"
#include "MapDefines.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <array>
#include <vector>
struct Node{float p[3];dtPolyRef ref;};
int main(){auto* mm=MMAP::MMapFactory::createOrGetMMapManager();mm->InitializeThreadUnsafe({530});
for(int x=27;x<=32;++x)for(int y=25;y<=29;++y)mm->loadMap("/home/trinity/server/data/",530,x,y);
assert(mm->loadMapInstance("/home/trinity/server/data/",530,0));auto* q=mm->GetNavMeshQuery(530,0);assert(q);
dtQueryFilter f;f.setIncludeFlags(NAV_GROUND);f.setExcludeFlags(NAV_WATER|NAV_MAGMA_SLIME);float ext[]={3,65,3};
unsigned count;std::cin>>count;std::vector<Node> nodes(count);bool valid=true;
for(unsigned i=0;i<count;++i){float x,y,z;std::cin>>x>>y>>z;float p[]={y,z,x};auto& n=nodes[i];n.ref=0;
bool ok=dtStatusSucceed(q->findNearestPoly(p,ext,&f,&n.ref,n.p))&&n.ref&&std::hypot(n.p[0]-y,n.p[2]-x)<2;
valid &=ok;std::cout<<"NODE,"<<i<<","<<n.p[2]<<","<<n.p[0]<<","<<n.p[1]<<","<<ok<<"\n";}
unsigned a,b;while(std::cin>>a>>b){auto& s=nodes[a];auto& t=nodes[b];dtPolyRef path[2048];int n=0;
bool ok=s.ref&&t.ref&&dtStatusSucceed(q->findPath(s.ref,t.ref,s.p,t.p,&f,path,&n,2048))&&n&&path[n-1]==t.ref;
float straight[2048*3];int pts=0;float length=0,direct=std::hypot(t.p[0]-s.p[0],t.p[2]-s.p[2]);
if(ok){ok=dtStatusSucceed(q->findStraightPath(s.p,t.p,path,n,straight,nullptr,nullptr,&pts,2048));for(int i=1;i<pts;++i)length+=std::hypot(straight[i*3]-straight[(i-1)*3],straight[i*3+2]-straight[(i-1)*3+2]);}
ok &= length<=direct*2.5f+20.f;
std::cout<<"EDGE,"<<a<<","<<b<<","<<direct<<","<<length<<","<<ok<<"\n";valid&=ok;}
auto* vm=VMAP::VMapFactory::createOrGetVMapManager();vm->InitializeThreadUnsafe({530});unsigned loaded=0;
for(int x=27;x<=32;++x)for(int y=25;y<=29;++y)loaded+=vm->loadMap("/home/trinity/server/data/vmaps/",530,x,y)==VMAP::VMAP_LOAD_RESULT_OK;
assert(loaded);std::cout<<"VMAPS,"<<loaded<<"\n";
for(unsigned i=0;i<count;++i){auto const& p=nodes[i].p;if(std::hypot(p[2]-943.49f,p[0]-2269.77f)>65.f)continue;
bool los=vm->isInLineOfSight(530,p[2],p[0],p[1]+2.f,943.49f,2269.77f,306.786f+2.f,VMAP::ModelIgnoreFlags::Nothing);
std::cout<<"BOSS_LOS,"<<i<<","<<los<<"\n";}
VMAP::VMapFactory::clear();MMAP::MMapFactory::clear();return valid?0:2;}
