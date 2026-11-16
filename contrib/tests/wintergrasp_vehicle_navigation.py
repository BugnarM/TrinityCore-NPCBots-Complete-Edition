#!/usr/bin/env python3
"""Run the production WG navigator against simulated collision and motion."""
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
source = (repo / "src/server/scripts/Northrend/zone_wintergrasp.cpp").read_text()
controller = source[source.index("class BotWarVehicleNavigation"):source.index("struct BotRouteMovement")]
assert "MovePoint" not in controller and "MovebyPath" in controller
vehicle_ai = source[source.index("struct npc_wg_bot_vehicle"):source.index("struct npc_wg_bot_soldier")]
assert vehicle_ai.index("if (!driver || driver->ToPlayer())") < vehicle_ai.index("UpdateNavigation")

code = r'''
#include "src/server/scripts/Northrend/wintergrasp_vehicle_navigation.h"
#include <array>
#include <cassert>
#include <iostream>
#include <limits>
#include <vector>
using uint32=std::uint32_t;
using namespace WintergraspBotWar;
namespace G3D { struct Vector3 { float x,y,z; Vector3(float a=0,float b=0,float c=0):x(a),y(b),z(c){} }; }
namespace VMAP { enum class ModelIgnoreFlags { Nothing }; }
enum { LINEOFSIGHT_CHECK_GOBJECT=2, LINEOFSIGHT_ALL_CHECKS=3, UNIT_STATE_NOT_MOVE, MOTION_PRIORITY_NORMAL, MOVE_RUN,
       PATHFIND_NORMAL=1,PATHFIND_INCOMPLETE=4 };
struct Position
{
    float m_positionX=0,m_positionY=0,m_positionZ=0;
    Position(float x=0,float y=0,float z=0):m_positionX(x),m_positionY(y),m_positionZ(z){}
    float GetPositionX() const{return m_positionX;}
    float GetPositionY() const{return m_positionY;}
    float GetPositionZ() const{return m_positionZ;}
    float GetExactDist2d(Position const* p) const{return std::hypot(m_positionX-p->m_positionX,m_positionY-p->m_positionY);}
};
struct Box { float left,right,bottom,top; };
struct Map
{
    std::vector<Box> boxes, terrain;
    unsigned additionalFlags=0;
    bool navigation=true;
    bool cornerRoute=false;
    bool denseRoute=false;
    unsigned queries=0;
    bool isInLineOfSight(float x1,float y1,float,float x2,float y2,float,uint32,int checks,VMAP::ModelIgnoreFlags) const
    {
        std::vector<Box> obstacles=boxes;
        if(checks&1)obstacles.insert(obstacles.end(),terrain.begin(),terrain.end());
        for(auto const& b:obstacles)
        {
            float first=0,last=1;
            auto clip=[&](float pos,float delta,float low,float high){
                if(std::fabs(delta)<0.0001f)return pos<low||pos>high;
                float a=(low-pos)/delta,c=(high-pos)/delta;
                if(a>c)std::swap(a,c);
                first=std::max(first,a);last=std::min(last,c);
                return first>last;
            };
            if(!clip(x1,x2-x1,b.left,b.right)&&!clip(y1,y2-y1,b.bottom,b.top)&&first<=last)return false;
        }
        return true;
    }
};
namespace Movement { using PointsArray=std::vector<G3D::Vector3>; }
struct Creature;
struct MotionMaster { void Clear(int){} };
struct Creature : Position
{
    Map* map;
    MotionMaster motion;
    Movement::PointsArray spline;
    std::size_t cursor=1;
    bool moving=false,rooted=false,casting=false,vehicle=true;
    bool IsVehicle() const{return vehicle;}
    bool IsWalking() const{return false;}
    unsigned launches=0;
    float radius=1.25f,height=3.f;
    explicit Creature(Map& value):map(&value){}
    bool IsInWorld() const{return true;}
    bool isMoving() const{return moving;}
    bool HasUnitState(int) const{return rooted;}
    bool IsMovementPreventedByCasting() const{return casting;}
    Position const& GetPosition() const{return *this;}
    Map* GetMap() const{return map;}
    uint32 GetPhaseMask() const{return 1;}
    float GetBoundingRadius() const{return radius;}
    float GetCollisionHeight() const{return height;}
    float GetSpeed(int) const{return 7.f;}
    MotionMaster* GetMotionMaster(){return &motion;}
    void StopMoving(){moving=false;}
    void UpdateAllowedPositionZ(float,float,float& z){z=0;}
    void Step()
    {
        float budget=7.f;
        while(moving&&budget>0&&cursor<spline.size())
        {
            auto const& target=spline[cursor];
            G3D::Vector3 old(m_positionX,m_positionY,m_positionZ);
            float distance=std::hypot(target.x-m_positionX,target.y-m_positionY);
            float fraction=distance?std::min(1.f,budget/distance):1.f;
            G3D::Vector3 next(m_positionX+(target.x-m_positionX)*fraction,m_positionY+(target.y-m_positionY)*fraction,0);
            auto clear=[&](float x,float y,float z,float a,float b,float c){return map->isInLineOfSight(x,y,z,a,b,c,1,0,VMAP::ModelIgnoreFlags::Nothing);};
            assert(Navigation::SegmentClear(old,next,GetBoundingRadius(),GetCollisionHeight(),clear));
            m_positionX=next.x;m_positionY=next.y;
            budget-=std::min(distance,budget);
            if(fraction==1.f)++cursor;
            if(cursor==spline.size())moving=false;
        }
    }
};
class PathGenerator
{
    Creature* owner;
    Movement::PointsArray points;
    uint32 type=1;
public:
    explicit PathGenerator(Creature* value):owner(value){}
    bool CalculatePath(float x,float y,float z,bool force)
    {
        assert(!force);++owner->map->queries;
        points={{owner->GetPositionX(),owner->GetPositionY(),owner->GetPositionZ()},{x,y,z}};
        if(owner->map->cornerRoute)
            points={{owner->GetPositionX(),owner->GetPositionY(),owner->GetPositionZ()},{9,0,0},{9,10,0},{30,10,0},{x,y,z}};
        if(owner->map->denseRoute)
        {
            float distance=std::hypot(x-owner->GetPositionX(),y-owner->GetPositionY());
            points.resize(1);
            for(float step=4.f;step<distance;step+=4.f)
                points.emplace_back(owner->GetPositionX()+(x-owner->GetPositionX())*step/distance,
                    owner->GetPositionY()+(y-owner->GetPositionY())*step/distance,z);
            points.emplace_back(x,y,z);
        }
        type=!owner->map->navigation?17u:std::hypot(x-owner->GetPositionX(),y-owner->GetPositionY())>80.f?32u:1u;
        type|=owner->map->additionalFlags;
        return true;
    }
    uint32 GetPathType() const{return type;}
    auto const& GetPath() const{return points;}
};
namespace Movement
{
class MoveSplineInit
{
    Creature* owner;
    PointsArray points;
public:
    explicit MoveSplineInit(Creature* value):owner(value){}
    void MovebyPath(PointsArray const& value){points=value;}
    void SetWalk(bool walk){assert(!walk);}
    int Launch(){owner->spline=points;owner->cursor=1;owner->moving=true;++owner->launches;return 1;}
};
}
bool ProjectBotWarGround(Map*,Position&){return false;}
CONTROLLER
bool Drive(BotWarVehicleNavigation& navigator,Creature& vehicle,Position const& goal,unsigned seconds,bool* retreated=nullptr)
{
    for(unsigned tick=0;tick<seconds;++tick)
    {
        navigator.Update(&vehicle,1000);
        vehicle.Step();
        if(retreated&&vehicle.GetPositionX()<5.f)*retreated=true;
        if(vehicle.GetExactDist2d(&goal)<=4.f)return true;
    }
    return false;
}
int main()
{
    for(unsigned type=0;type<256;++type)
    {
        assert(Navigation::VerifiedPath(type,2)==(type==1||type==4));
        assert(Navigation::VerifiedInfantryPath(type,2)==(type==1||type==4||type==129||type==132));
    }
    Map map;map.boxes={{8,12,-4,4}};
    Creature vehicle(map);BotWarVehicleNavigation nav;Position goal(30,0,0);
    nav.Move(&vehicle,goal);
    assert(Drive(nav,vehicle,goal,120));
    // Keep the actual next spline corner, rather than testing a diagonal across it.
    Map turning;turning.boxes={{-5,7,2,8}};turning.cornerRoute=true;
    Creature corner(turning);BotWarVehicleNavigation turns;
    turns.Move(&corner,goal);
    assert(Drive(turns,corner,goal,120)&&corner.launches==1);
    // A changed tactical goal immediately replaces an unfinished old prefix.
    Map redirect;Creature redirected(redirect);BotWarVehicleNavigation orders;
    orders.Move(&redirected,Position(100,0,0));redirected.Step();
    Position newGoal(0,40,0);orders.Move(&redirected,newGoal);
    assert(Drive(orders,redirected,newGoal,120));
    // A U-shaped pocket requires a real retreat, not just choosing forward points.
    Map pocket;pocket.boxes={{5,25,3,5},{5,25,-5,-3},{23,25,-5,5}};
    Creature trapped(pocket);trapped.m_positionX=20;
    BotWarVehicleNavigation escape;Position outside(40,0,0);bool retreated=false;
    escape.Move(&trapped,outside);
    assert(Drive(escape,trapped,outside,120,&retreated)&&retreated);
    // A new obstruction invalidates a previously issued spline.
    Map changing;Creature moving(changing);BotWarVehicleNavigation repair;Position far(60,0,0);
    repair.Move(&moving,far);moving.Step();
    changing.boxes={{15,19,-4,4}};
    assert(Drive(repair,moving,far,120));
    // A real smooth path has four-metre segments: look beyond the first corner
    // so the vehicle cannot cover seven metres before noticing a new obstacle.
    Map dense;dense.denseRoute=true;Creature fast(dense);BotWarVehicleNavigation ahead;
    ahead.Move(&fast,far);fast.Step();
    dense.boxes={{13,16,-4,4}};
    assert(Drive(ahead,fast,far,120));
    // An enclosed vehicle waits with bounded retries, then resumes when unblocked.
    Map closed;closed.boxes={{-4,-3,-4,4},{3,4,-4,4},{-4,4,-4,-3},{-4,4,3,4}};
    Creature stopped(closed);BotWarVehicleNavigation retry;Position openGoal(40,0,0);
    retry.Move(&stopped,openGoal);
    assert(!Drive(retry,stopped,openGoal,12)&&stopped.launches==0&&closed.queries<=30);
    closed.boxes.clear();assert(Drive(retry,stopped,openGoal,60));
    // Absent navmesh must not produce the engine's straight-line fallback.
    Map absent;absent.navigation=false;Creature waiting(absent);BotWarVehicleNavigation wait;
    wait.Move(&waiting,openGoal);assert(!Drive(wait,waiting,openGoal,8)&&waiting.launches==0);
    absent.navigation=true;assert(Drive(wait,waiting,openGoal,60));
    // Real lack of progress must choose a different route, even if navmesh and
    // raycasts still claim that the original forward path is valid.
    Map stuckMap;Creature stuck(stuckMap);BotWarVehicleNavigation unstuck;
    unstuck.Move(&stuck,far);
    for(unsigned tick=0;tick<5;++tick)unstuck.Update(&stuck,1000);
    assert(stuck.launches>=2&&std::fabs(stuck.spline.back().y)>1.f&&unstuck.Stalls()>=1);
    assert(Drive(unstuck,stuck,far,120)&&unstuck.Recoveries()>=1);
    // Long paths exceeding the native path limit progress through short prefixes.
    Map longRoad;Creature traveler(longRoad);BotWarVehicleNavigation prefixes;Position distant(400,0,0);
    prefixes.Move(&traveler,distant);assert(Drive(prefixes,traveler,distant,180)&&traveler.launches>2);
    Map roots;Creature rooted(roots);rooted.rooted=true;BotWarVehicleNavigation controlled;
    controlled.Move(&rooted,openGoal);assert(!Drive(controlled,rooted,openGoal,5)&&rooted.launches==0&&controlled.Stalls()==0);
    rooted.rooted=false;assert(Drive(controlled,rooted,openGoal,60));
    // Vehicle width rejects a path whose center alone clears a narrow obstacle.
    Map narrow;narrow.boxes={{8,12,0.7f,2}};
    auto clear=[&](float x,float y,float z,float a,float b,float c){return narrow.isInLineOfSight(x,y,z,a,b,c,1,0,VMAP::ModelIgnoreFlags::Nothing);};
    assert(!Navigation::SegmentClear(G3D::Vector3(0,0,0),G3D::Vector3(20,0,0),1.25f,3.f,clear));
    // The production navigator is now shared by foot soldiers: their smaller
    // collision radius passes a footpath that is too narrow for a siege engine.
    Creature infantry(narrow);infantry.vehicle=false;infantry.radius=0.3f;infantry.height=2.f;
    BotWarVehicleNavigation march;Position footGoal(20,0,0);
    march.Move(&infantry,footGoal);assert(Drive(march,infantry,footGoal,30));
    assert(infantry.launches==1);
    Map footPocket;footPocket.boxes={{5,25,3,5},{5,25,-5,-3},{23,25,-5,5}};
    Creature foot(footPocket);foot.vehicle=false;foot.radius=0.3f;foot.height=2.f;foot.m_positionX=20;
    BotWarVehicleNavigation escapeFoot;escapeFoot.Move(&foot,outside);
    assert(Drive(escapeFoot,foot,outside,120));
    // A real mesh may return an endpoint-height annotation on a valid prefix.
    Map heightHint;heightHint.additionalFlags=128;heightHint.terrain={{8,12,-3,3}};
    Creature soldier(heightHint);soldier.vehicle=false;soldier.radius=0.3f;
    BotWarVehicleNavigation nativeFoot;nativeFoot.Move(&soldier,footGoal);
    assert(Drive(nativeFoot,soldier,footGoal,30));
    Creature siege(heightHint);BotWarVehicleNavigation stillStrict;stillStrict.Move(&siege,footGoal);
    assert(!Drive(stillStrict,siege,footGoal,5)&&siege.launches==0);
    // Dynamic closed gates remain blocking even for infantry.
    Map gate;gate.boxes={{3,4,-100,100}};
    Creature blockedFoot(gate);blockedFoot.vehicle=false;blockedFoot.radius=0.3f;
    BotWarVehicleNavigation closedGate;closedGate.Move(&blockedFoot,footGoal);
    assert(!Drive(closedGate,blockedFoot,footGoal,5));
    controlled.Cancel(&rooted);assert(!rooted.moving);
    std::cout<<"PASS: production navigator avoids obstacles, reverses out of pockets, rechecks dynamic collision, retries safely, follows long-road prefixes and respects roots/vehicle width\n";
}
'''.replace("CONTROLLER", controller)
with tempfile.TemporaryDirectory(prefix="wg-vehicle-nav-") as tmp:
    cpp, binary = Path(tmp) / "test.cpp", Path(tmp) / "test"
    cpp.write_text(code)
    subprocess.run(["c++", "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror", "-I", str(repo), str(cpp), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
