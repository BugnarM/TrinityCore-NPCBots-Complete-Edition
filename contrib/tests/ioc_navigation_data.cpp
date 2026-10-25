// Optional data integration test. Run from repo root with an extracted mmaps directory.
// g++ -std=c++20 -O2 -Wall -Wextra -Werror -Isrc/common -Isrc/common/Utilities -Isrc/common/Collision/Maps -Idep/recastnavigation/Detour/Include contrib/tests/ioc_navigation_data.cpp /home/trinity/build/npcbots/dep/recastnavigation/Detour/libDetour.a -o /tmp/ioc-navigation-data
#include "MapDefines.h"
#include "DetourNavMeshQuery.h"
#include "../../src/server/game/AI/NpcBots/botbgtactics.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>
#include <cmath>
#include <sstream>
#include <map>
#include <stdexcept>

static void Require(bool value, std::string const& message)
{
    if (!value) throw std::runtime_error(message);
}
struct Node { unsigned id; float x,y,z; std::vector<unsigned> links; dtPolyRef poly=0; float near[3]{}; };

int main(int argc, char** argv)
{
 try
 {
    Require(argc==2, "Pass the mmaps directory as the only argument");
    std::filesystem::path base(argv[1]);
    dtNavMeshParams params{};
    std::ifstream input(base/"628.mmap",std::ios::binary);
    Require(bool(input.read(reinterpret_cast<char*>(&params),sizeof params)), "Invalid 628.mmap");
    auto mesh=std::unique_ptr<dtNavMesh,decltype(&dtFreeNavMesh)>(dtAllocNavMesh(),dtFreeNavMesh);
    Require(dtStatusSucceed(mesh->init(&params)), "Invalid navmesh parameters");
    unsigned loaded=0;
    for(auto const& entry : std::filesystem::directory_iterator(base))
    {
        auto name=entry.path().filename().string();
        if(name.rfind("628",0)!=0 || entry.path().extension()!=".mmtile") continue;
        std::ifstream tile(entry.path(),std::ios::binary);
        MmapTileHeader header;
        Require(bool(tile.read(reinterpret_cast<char*>(&header),sizeof header)), "Truncated tile: "+name);
        Require(header.mmapMagic==MMAP_MAGIC && header.dtVersion==DT_NAVMESH_VERSION && header.mmapVersion==MMAP_VERSION,
            "Incompatible tile: "+name);
        Require(header.size>=sizeof(dtMeshHeader) && header.size==std::filesystem::file_size(entry.path())-sizeof header,
            "Corrupt tile size: "+name);
        auto* data=static_cast<unsigned char*>(dtAlloc(header.size,DT_ALLOC_PERM));
        Require(data!=nullptr,"Allocation failure");
        if(!tile.read(reinterpret_cast<char*>(data),header.size) ||
            dtStatusFailed(mesh->addTile(data,int(header.size),DT_TILE_FREE_DATA,0,nullptr)))
        {
            dtFree(data); throw std::runtime_error("Detour rejected tile: "+name);
        }
        ++loaded;
    }
    Require(loaded>0,"No IC navigation tiles");
    dtNavMeshQuery query;
    Require(dtStatusSucceed(query.init(mesh.get(),4096)),"Cannot initialize query");
    dtQueryFilter filter; filter.setIncludeFlags(NAV_GROUND|NAV_WATER);
    std::map<unsigned,Node> nodes;
    std::ifstream sql("sql/custom/world/2026_10_23_01_battleground_bots_sa_ic.sql");
    Require(bool(sql),"Missing production SQL");
    std::string line;
    while(std::getline(sql,line))
    {
        if(line.rfind("(53",0)!=0) continue;
        line=line.substr(1,line.find_last_of(')')-1);
        std::vector<std::string> fields;
        std::string field; bool quoted=false;
        for(char c:line)
        {
            if(c=='\'') quoted=!quoted;
            if(c==','&&!quoted){fields.push_back(field);field.clear();}
            else field+=c;
        }
        fields.push_back(field);
        Require(fields.size()==16,"Waypoint column count");
        Node node{unsigned(std::stoul(fields[0])),std::stof(fields[11]),std::stof(fields[12]),std::stof(fields[13]),{}};
        auto links=fields[15]; links=links.substr(links.find('\'')+1); links=links.substr(0,links.find('\''));
        std::istringstream tokens(links); std::string token;
        while(tokens>>token) node.links.push_back(unsigned(std::stoul(token.substr(0,token.find(':')))));
        nodes.emplace(node.id,node);
    }
    Require(nodes.size()==23,"Expected all 23 production IC checkpoints");
    float extent[]={3.f,5.f,3.f};
    for(auto& [id,node] : nodes)
    {
        float pos[]={node.y,node.z,node.x};
        Require(dtStatusSucceed(query.findNearestPoly(pos,extent,&filter,&node.poly,node.near)) && node.poly,
            "Checkpoint not on a walkable polygon: "+std::to_string(id));
        Require(std::hypot(node.near[2]-node.x,node.near[0]-node.y)<2.f && std::fabs(node.near[1]-node.z)<3.f,
            "Checkpoint coordinates are outside navigation tolerance: "+std::to_string(id));
    }
    unsigned edges=0;
    for(auto const& [id,node] : nodes)
        for(unsigned next : node.links)
        {
            auto const& target=nodes.at(next);
            dtPolyRef current=node.poly;
            float position[3]={node.near[0],node.near[1],node.near[2]};
            bool arrived=false;
            for(unsigned prefix=0;prefix<32;++prefix)
            {
                if(current==target.poly){arrived=true;break;}
                dtPolyRef corridor[74]; int count=0;
                auto status=query.findPath(current,target.poly,position,target.near,&filter,corridor,&count,74);
                Require(dtStatusSucceed(status) && count,
                    "Unreachable road edge: "+std::to_string(id)+" -> "+std::to_string(next));
                float points[74*3]; int pointCount=0;
                status=query.findStraightPath(position,target.near,corridor,count,points,nullptr,nullptr,&pointCount,74);
                Require(dtStatusSucceed(status) && pointCount>=1 && pointCount<74,
                    "Invalid road spline: "+std::to_string(id)+" -> "+std::to_string(next)+" points="+std::to_string(pointCount));
                for(int p=1;p<pointCount;++p)
                    Require(NPCBots::SiegeBG::ICSegmentOpen(points[(p-1)*3+2],points[(p-1)*3],points[p*3+2],points[p*3],63),
                        "Road crosses a solid keep wall: "+std::to_string(id)+" -> "+std::to_string(next));
                if(corridor[count-1]==target.poly){arrived=true;break;}
                Require(corridor[count-1]!=current,"Road prefix makes no progress: "+std::to_string(id)+" -> "+std::to_string(next));
                current=corridor[count-1];
                for(unsigned j=0;j<3;++j) position[j]=points[(pointCount-1)*3+j];
            }
            Require(arrived,"Road exceeds prefix budget: "+std::to_string(id)+" -> "+std::to_string(next));
            ++edges;
        }
    // Actual native keep portal positions must be navigable from the spawn courtyard.
    struct Portal { unsigned spawn; float x,y,z; };
    Portal portals[]={{5300,326.285f,-777.366f,49.0208f},{5305,1235.53f,-683.872f,49.304f}};
    for(auto const& portal:portals)
    {
        auto const& source=nodes.at(portal.spawn);
        float pos[]={portal.y,portal.z,portal.x},near[3]{};dtPolyRef poly=0;
        Require(dtStatusSucceed(query.findNearestPoly(pos,extent,&filter,&poly,near))&&poly,"Portal is not navigable");
        dtPolyRef corridor[74];int count=0;
        Require(dtStatusSucceed(query.findPath(source.poly,poly,source.near,near,&filter,corridor,&count,74)) && count && corridor[count-1]==poly,
            "Spawn cannot reach its real keep portal");
        float points[222];int size=0;
        Require(dtStatusSucceed(query.findStraightPath(source.near,near,corridor,count,points,nullptr,nullptr,&size,74)),"Invalid portal path");
        for(int i=1;i<size;++i)
            Require(NPCBots::SiegeBG::ICSegmentOpen(points[(i-1)*3+2],points[(i-1)*3],points[i*3+2],points[i*3],0),
                "Portal approach crosses an intact wall");
    }
    std::cout<<"PASS: "<<loaded<<" production-compatible IC tiles, 23 checkpoints, "<<edges<<" directed road paths, both keep portal approaches.\n";
 }
 catch(std::exception const& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
