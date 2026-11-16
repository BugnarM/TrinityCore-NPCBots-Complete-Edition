// SA road integration test. Run from the repository root with an extracted mmaps directory.
// g++ -std=c++20 -O2 -Wall -Wextra -Werror -Isrc/common -Isrc/common/Utilities -Isrc/common/Collision/Maps -Idep/recastnavigation/Detour/Include contrib/tests/sa_navigation_data.cpp /home/trinity/build/npcbots/dep/recastnavigation/Detour/libDetour.a -o /tmp/sa-navigation-data
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
    std::ifstream input(base/"607.mmap",std::ios::binary);
    Require(bool(input.read(reinterpret_cast<char*>(&params),sizeof params)), "Invalid 607.mmap");
    auto mesh=std::unique_ptr<dtNavMesh,decltype(&dtFreeNavMesh)>(dtAllocNavMesh(),dtFreeNavMesh);
    Require(dtStatusSucceed(mesh->init(&params)), "Invalid navmesh parameters");
    unsigned loaded=0;
    for(auto const& entry : std::filesystem::directory_iterator(base))
    {
        auto name=entry.path().filename().string();
        if(name.rfind("607",0)!=0 || entry.path().extension()!=".mmtile") continue;
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
    Require(loaded>0,"No SA navigation tiles");
    dtNavMeshQuery query;
    Require(dtStatusSucceed(query.init(mesh.get(),4096)),"Cannot initialize query");
    dtQueryFilter filter; filter.setIncludeFlags(NAV_GROUND|NAV_WATER);
    std::map<unsigned,Node> nodes;
    std::ifstream sql("sql/custom/world/2026_10_23_01_battleground_bots_sa_ic.sql");
    Require(bool(sql),"Missing production SQL");
    std::string line;
    while(std::getline(sql,line))
    {
        if(line.rfind("(52",0)!=0) continue;
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
    Require(nodes.size()==27,"Expected all 27 production SA checkpoints");
    float extent[]={8.f,80.f,8.f};
    for(auto& [id,node] : nodes)
    {
        float pos[]={node.y,node.z,node.x};
        Require(dtStatusSucceed(query.findNearestPoly(pos,extent,&filter,&node.poly,node.near)) && node.poly,
            "Checkpoint not on a walkable polygon: "+std::to_string(id));
        Require(std::hypot(node.near[2]-node.x,node.near[0]-node.y)<8.f && std::fabs(node.near[1]-node.z)<20.f,
            "Checkpoint coordinates are outside navigation tolerance: "+std::to_string(id));
    }

    // Both attacking beach lanes and the routes from defender spawn to the
    // inner sides of the green/blue gates, using the real production SQL.
    unsigned roads[][2] = {
        {5200,5202},{5202,5204},{5201,5202},
        {5200,5203},{5201,5203},{5203,5206},
        {5215,5214},{5214,5223},{5223,5211},{5211,5210},{5210,5208},{5208,5221},
        {5214,5224},{5224,5213},{5213,5212},{5212,5209},{5209,5222}
    };
    unsigned edges=0;
    for(auto const& road : roads)
        for(bool reverse : {false,true})
        {
            auto const& from=nodes.at(road[reverse?1:0]);
            auto const& to=nodes.at(road[reverse?0:1]);
            dtPolyRef corridor[4096]; int count=0;
            auto status=query.findPath(from.poly,to.poly,from.near,to.near,&filter,corridor,&count,4096);
            Require(dtStatusSucceed(status) && count && corridor[count-1]==to.poly,
                "Disconnected SA road: "+std::to_string(from.id)+" -> "+std::to_string(to.id));
            ++edges;
        }
    std::cout<<"PASS: "<<loaded<<" compatible SA tiles, 27 checkpoint projections, "<<edges
        <<" directed beach/defender road paths, including the blue inner gate\n";
 }
 catch(std::exception const& e){std::cerr<<"FAIL: "<<e.what()<<'\n';return 1;}
}
