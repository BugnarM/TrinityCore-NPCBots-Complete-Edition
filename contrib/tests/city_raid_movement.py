#!/usr/bin/env python3
"""Exercise actual controller navigation against partial, invalid and cached path results."""
from pathlib import Path
import subprocess,tempfile
repo=Path(__file__).resolve().parents[2];s=(repo/'src/server/scripts/Events/city_raids.cpp').read_text()
def block(signature):
 a=s.index(signature);b=s.index('{',a)+1;n=1
 while n:n+=(s[b]=='{')-(s[b]=='}');b+=1
 return s[a:b]
code=r'''
#include "src/server/game/AI/NpcBots/botcityraid.h"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <memory>
#include <vector>
using uint32=std::uint32_t;namespace Tactics=NPCBots::CityRaid;using State=Tactics::State;
constexpr unsigned UNIT_STATE_NOT_MOVE=1,MOTION_PRIORITY_NORMAL=1,LINEOFSIGHT_ALL_CHECKS=1;
constexpr unsigned PATHFIND_NORMAL=1,PATHFIND_SHORTCUT=2,PATHFIND_INCOMPLETE=4,PATHFIND_NOPATH=8,PATHFIND_NOT_USING_PATH=16,PATHFIND_SHORT=32;
namespace VMAP{enum class ModelIgnoreFlags{Nothing};}
struct Position{
 float m_positionX=0,m_positionY=0,m_positionZ=0;
 float GetPositionX()const{return m_positionX;}float GetPositionY()const{return m_positionY;}float GetPositionZ()const{return m_positionZ;}
 float GetExactDist2d(Position const* p)const{return std::hypot(m_positionX-p->m_positionX,m_positionY-p->m_positionY);}
 void Relocate(float x,float y,float z){m_positionX=x;m_positionY=y;m_positionZ=z;}
 float GetAbsoluteAngle(Position const* p)const{return std::atan2(p->m_positionY-m_positionY,p->m_positionX-m_positionX);}
};
struct Guid{bool IsEmpty()const{return true;}};
struct Map{template<class... Args>bool isInLineOfSight(Args...){return true;}};
struct Motion{void Clear(unsigned){}};
struct Unit:Position{
 bool moving=false,rooted=false,casting=false;unsigned launches=0;Map map;Motion motion;
 bool HasUnitState(unsigned)const{return rooted;}bool IsMovementPreventedByCasting()const{return casting;}
 bool isMoving()const{return moving;}void UpdateAllowedPositionZ(float,float,float&){}
 Map* GetMap(){return &map;}unsigned GetPhaseMask()const{return 1;}Motion* GetMotionMaster(){return &motion;}
};
namespace ObjectAccessor{Unit* GetUnit(Unit const&,Guid){return nullptr;}}
struct Runtime{unsigned tokens=8;std::unique_ptr<int> Navigation=std::make_unique<int>(1);bool TakePathToken(){if(!tokens)return false;--tokens;return true;}};
struct Point{float x,y,z;};
struct PathGenerator{
 Unit* bot;bool straight=false;std::vector<Point> path;
 static inline unsigned type=PATHFIND_NORMAL,calls=0;static inline bool overrideOk=true;static inline float fraction=1;
 explicit PathGenerator(Unit* p):bot(p){}
 void SetUseStraightPath(bool b){straight=b;}bool SetNavMeshQuery(int*){return overrideOk;}
 bool CalculatePath(float x,float y,float z,bool force){assert(straight&&!force);++calls;path={{bot->m_positionX,bot->m_positionY,bot->m_positionZ},{bot->m_positionX+(x-bot->m_positionX)*fraction,y,z}};return true;}
 unsigned GetPathType()const{return type;}std::vector<Point> const& GetPath()const{return path;}
};
namespace Movement{struct MoveSplineInit{
 Unit* bot;explicit MoveSplineInit(Unit* p):bot(p){}void MovebyPath(std::vector<Point>const&){}void SetWalk(bool){}void Launch(){++bot->launches;bot->moving=true;}
};}
struct Controller{
 Unit creature;Unit* me=&creature;bool _retreated=false,_hasGoal=false,_goalComplete=false,_reinforcing=false;uint32 _pathTimer=0;unsigned _index=0;
 Position _lastGoal,_acceptedGoal;struct Order{State Mode=State::Marching;Guid Focus;}_order;
 std::shared_ptr<Runtime> _runtime=std::make_shared<Runtime>();
MOVE
};
int main(){
 Position goal{100,0,0};Controller c;
 PathGenerator::type=PATHFIND_INCOMPLETE;PathGenerator::fraction=0.5f;c.Move(goal);assert(c.creature.launches==1&&!c._goalComplete);
 // A stopped spline at an incomplete prefix must issue another path towards the original destination.
 c.creature.moving=false;c.creature.Relocate(50,0,0);c._pathTimer=0;c.Move(goal);assert(c.creature.launches==2);
 c.creature.moving=false;c.creature.Relocate(75,0,0);c._pathTimer=0;PathGenerator::type=PATHFIND_NORMAL;PathGenerator::fraction=0.9f;c.Move(goal);assert(c._goalComplete);
 c.creature.moving=false;c.creature.Relocate(97.5f,0,0);c._pathTimer=0;auto calls=PathGenerator::calls;c.Move(goal);assert(PathGenerator::calls==calls); // accepted valid goal cache
 for(unsigned invalid:{PATHFIND_NOPATH,PATHFIND_NORMAL|PATHFIND_SHORT,PATHFIND_NORMAL|PATHFIND_NOT_USING_PATH,PATHFIND_NORMAL|PATHFIND_SHORTCUT}){
  Controller bad;PathGenerator::type=invalid;bad.Move(goal);assert(bad.creature.launches==0);
 }
 PathGenerator::type=PATHFIND_NORMAL;Controller noTokens;noTokens._runtime->tokens=0;calls=PathGenerator::calls;noTokens.Move(goal);assert(PathGenerator::calls==calls);
 Controller cast;cast.creature.casting=true;cast.Move(goal);assert(!cast.creature.launches);
 Controller root;root.creature.rooted=true;root.Move(goal);assert(!root.creature.launches);
 Controller wrongMesh;PathGenerator::overrideOk=false;wrongMesh.Move(goal);assert(!wrongMesh.creature.launches);
 std::cout<<"PASS: actual movement resumes partial prefixes, caches accepted destinations, rejects unsafe shortcuts/overflow/missing-mmap paths, obeys token budget and casting/root checks\n";
}
'''.replace('\nMOVE\n','\n'+block('    void Move(Position goal)')+'\n')
with tempfile.TemporaryDirectory(prefix='city-movement-') as tmp:
 src=Path(tmp)/'test.cpp';exe=Path(tmp)/'test';src.write_text(code)
 subprocess.run(['c++','-std=c++20','-O2','-Wall','-Wextra','-Werror','-I',str(repo),str(src),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
