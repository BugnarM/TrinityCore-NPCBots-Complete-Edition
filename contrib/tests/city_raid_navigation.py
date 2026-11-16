#!/usr/bin/env python3
"""Check both cities' actual configured routes and graveyards on installed ground navmeshes."""
from pathlib import Path
import re,shlex,subprocess,tempfile
repo=Path(__file__).resolve().parents[2];build=Path('/home/trinity/build/npcbots')
s=(repo/'src/server/scripts/Events/city_raids.cpp').read_text()
s=s[s.index('static const CityRaidConfig'):s.index('\nnamespace\n')]
points=re.findall(r'Position\(([^)]+)\)',s);assert len(points)==32
flags=[]
for line in (build/'src/common/CMakeFiles/common.dir/flags.make').read_text().splitlines():
 if line.startswith(('CXX_DEFINES =','CXX_INCLUDES =')):flags+=shlex.split(line.split('=',1)[1])
libs=[str(build/p) for p in ['src/common/libcommon.a','dep/fmt/libfmt.a','dep/g3dlite/libg3dlib.a','dep/SFMT/libsfmt.a','dep/recastnavigation/Detour/libDetour.a']]
# nodes: attacker GY, defender GY, 11 waypoints, 3 checkpoints.
edges=[(i,i+1) for i in range(2,12)]+[(i+1,i) for i in range(2,12)]+[(0,2),(2,0)]
edges += [(1,i) for i in (13,14,15)]+[(i,1) for i in (13,14,15)]
# Dynamic defenders fall back to the next checkpoint as the assault progresses.
edges += [(13,14),(14,15),(13,15)]
with tempfile.TemporaryDirectory(prefix='city-navigation-') as tmp:
 binary=Path(tmp)/'verify'
 subprocess.run(['c++','-std=c++20','-O2',*flags,str(repo/'contrib/tests/city_raid_navigation.cpp'),'-o',str(binary),'-Wl,--start-group',*libs,'-Wl,--end-group','-lboost_filesystem','-lboost_program_options','-lboost_regex','-lboost_locale','-lboost_chrono','-lboost_system','-lboost_thread','-lboost_atomic','-lssl','-lcrypto','-lz','-ldl','-lpthread'],check=True)
 for mapid in (0,1):
  nodes=[tuple(float(x.strip().removesuffix('f')) for x in row.split(',')[:3]) for row in points[mapid*16:(mapid+1)*16]]
  payload=f'{mapid}\n16\n'+''.join(f'{x} {y} {z}\n' for x,y,z in nodes)+''.join(f'{a} {b}\n' for a,b in edges)
  result=subprocess.run([str(binary)],input=payload,text=True,capture_output=True)
  print(result.stdout,end='');assert result.returncode==0,result.stderr
print(f'PASS: 32 configured city/ground positions and {len(edges)*2} attack, retreat, reinforcement and defender fallback routes, using the dedicated raid query and native 74-poly/74-corner limits')
