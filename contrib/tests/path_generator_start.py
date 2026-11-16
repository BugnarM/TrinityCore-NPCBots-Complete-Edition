#!/usr/bin/env python3
"""Check production path-start delegation without moving the source actor."""
from pathlib import Path
import subprocess
import tempfile
repo=Path(__file__).resolve().parents[2]
source=(repo/'src/server/game/Movement/PathGenerator.cpp').read_text()
methods=source[source.index('bool PathGenerator::CalculatePath('):source.index('dtPolyRef PathGenerator::GetPathPolyByPosition(')]
code=r'''
#include <cmath>
#include <limits>
#include <stdexcept>
#include <iostream>
#define TC_METRIC_DETAILED_EVENT(...)
#define TC_LOG_DEBUG(...)
namespace G3D {struct Vector3 {float x,y,z;Vector3(float a,float b,float c):x(a),y(b),z(c){};};}
namespace Trinity {bool IsValidMapCoord(float x,float y,float z){return std::isfinite(x)&&std::isfinite(y)&&std::isfinite(z);}}
enum PathType {PATHFIND_NORMAL=1,PATHFIND_NOT_USING_PATH=16};
enum {UNIT_STATE_IGNORE_PATHFINDING=1};
struct Unit {
 float x=4,y=5,z=6;bool ignore=false;
 void GetPosition(float& a,float& b,float& c)const{a=x;b=y;c=z;}
 Unit const* ToUnit()const{return this;}bool HasUnitState(unsigned)const{return ignore;}
};
struct PathGenerator {
 Unit const* _source;bool _navMesh=true,_navMeshQuery=true,_forceDestination=false,tiles=true;
 PathType _type=PATHFIND_NORMAL;unsigned paths=0,shortcuts=0,filters=0;
 G3D::Vector3 start{0,0,0},end{0,0,0},usedStart{0,0,0},usedEnd{0,0,0};
 bool CalculatePath(float,float,float,bool=false);
 bool CalculatePathFrom(G3D::Vector3 const&,G3D::Vector3 const&,bool=false);
 void SetStartPosition(G3D::Vector3 const& p){start=p;}void SetEndPosition(G3D::Vector3 const& p){end=p;}
 bool HaveTile(G3D::Vector3 const&)const{return tiles;}void BuildShortcut(){++shortcuts;}
 void UpdateFilter(){++filters;}void BuildPolyPath(G3D::Vector3 const& a,G3D::Vector3 const& b){++paths;usedStart=a;usedEnd=b;}
};
METHODS
void Check(bool b,char const* why){if(!b)throw std::runtime_error(why);}
int main(){
 Unit actor;PathGenerator path{&actor};
 Check(path.CalculatePath(10,20,30,true)&&path.paths==1&&path.usedStart.x==4&&path.usedStart.y==5&&path.usedStart.z==6&&path._forceDestination,"normal path no longer starts at actor");
 Check(path.CalculatePathFrom({40,50,60},{70,80,90},false)&&path.paths==2&&path.usedStart.x==40&&path.usedStart.z==60&&path.usedEnd.x==70&&!path._forceDestination,"lookahead ignores explicit start/goal");
 Check(actor.x==4&&actor.y==5&&actor.z==6,"lookahead moved the actor");
 Check(!path.CalculatePathFrom({NAN,0,0},{0,0,0})&&!path.CalculatePath(1,2,INFINITY)&&path.paths==2,"invalid coordinates accepted");
 for(unsigned reason=0;reason<4;++reason){PathGenerator p{&actor};if(reason==0)p._navMesh=false;if(reason==1)p._navMeshQuery=false;if(reason==2)p.tiles=false;if(reason==3)actor.ignore=true;
  Check(p.CalculatePathFrom({1,2,3},{4,5,6})&&p.shortcuts==1&&p.paths==0&&unsigned(p._type)==17,"missing mesh/ignore flags changed");actor.ignore=false;}
 std::cout<<"PASS: production path delegation preserves normal start, flags, missing-mesh behavior; explicit lookahead never moves its actor\n";
}
'''.replace('METHODS',methods)
with tempfile.TemporaryDirectory(prefix='path-start-') as directory:
    cpp,exe=Path(directory)/'test.cpp',Path(directory)/'test';cpp.write_text(code)
    subprocess.run(['c++','-std=c++20','-O2','-Wall','-Wextra',str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True)
