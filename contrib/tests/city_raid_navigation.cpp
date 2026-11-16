// Real installed navmesh verification; invoked by city_raid_navigation.py.
#include "MMapFactory.h"
#include "MapDefines.h"
#include "../../src/server/game/AI/NpcBots/botcityraid.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <set>
#include <vector>
struct Node { float p[3]; dtPolyRef ref=0; };
int main()
{
    unsigned mapId,count;std::cin>>mapId>>count;
    auto* mm=MMAP::MMapFactory::createOrGetMMapManager();
    mm->InitializeThreadUnsafe({mapId});
    std::vector<Node> nodes(count);
    for (auto& n:nodes)
    {
        float x,y,z;std::cin>>x>>y>>z;n.p[0]=y;n.p[1]=z;n.p[2]=x;
        int tx=int(32-x/533.33333f),ty=int(32-y/533.33333f);
        for (int a=tx-2;a<=tx+2;++a) for (int b=ty-2;b<=ty+2;++b)
            mm->loadMap("/home/trinity/server/data/",mapId,a,b);
    }
    if (!mm->loadMapInstance("/home/trinity/server/data/",mapId,0)) return 1;
    dtNavMeshQuery navigation;navigation.init(mm->GetNavMesh(mapId),NPCBots::CityRaid::QueryNodes);
    auto* q=&navigation; // dedicated city-raid query; global map query stays at 1024 nodes
    dtQueryFilter filter;filter.setIncludeFlags(NAV_GROUND);filter.setExcludeFlags(NAV_WATER|NAV_MAGMA_SLIME);
    float ext[]={3,10,3};bool valid=true;
    for (unsigned i=0;i<count;++i)
    {
        auto& n=nodes[i];float p[3]{};
        bool ok=dtStatusSucceed(q->findNearestPoly(n.p,ext,&filter,&n.ref,p))&&n.ref
            &&std::hypot(p[0]-n.p[0],p[2]-n.p[2])<0.5f;
        std::copy(p,p+3,n.p);valid&=ok;
        std::cout<<"NODE,"<<mapId<<","<<i<<","<<ok<<"\n";
    }
    unsigned a,b;
    while (std::cin>>a>>b)
    {
        Node current=nodes.at(a);auto const& target=nodes.at(b);
        bool ok=current.ref&&target.ref;unsigned steps=0;
        std::set<dtPolyRef> seen;
        // Same 74-poly and 74-corner caps as PathGenerator. Long routes must
        // advance through incomplete prefixes, without any direct-line fallback.
        while (ok&&current.ref!=target.ref&&steps<64)
        {
            if (!seen.insert(current.ref).second) {ok=false;break;}
            dtPolyRef path[74];int n=0;
            ok=dtStatusSucceed(q->findPath(current.ref,target.ref,current.p,target.p,&filter,path,&n,74))&&n;
            if (!ok) break;
            float end[3];q->closestPointOnPoly(path[n-1],target.p,end,nullptr);
            float corners[74*3];int points=0;
            ok=dtStatusSucceed(q->findStraightPath(current.p,end,path,n,corners,nullptr,nullptr,&points,74))&&points>=2&&points<74;
            if (!ok) break;
            current.ref=path[n-1];std::copy(end,end+3,current.p);++steps;
        }
        ok&=current.ref==target.ref;valid&=ok;
        std::cout<<"EDGE,"<<mapId<<","<<a<<","<<b<<","<<steps<<","<<ok<<"\n";
    }
    MMAP::MMapFactory::clear();return valid?0:2;
}
