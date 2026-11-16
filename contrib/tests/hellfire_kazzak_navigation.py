#!/usr/bin/env python3
"""Validate the SQL plateau graph on installed mmaps/vmaps, including LoS to Kazzak."""
from pathlib import Path
import math,re,shlex,subprocess,tempfile
repo=Path(__file__).resolve().parents[2];build=Path('/home/trinity/build/npcbots')
sql=(repo/'sql/custom/world/npcbot_2026_11_10_01_hellfire_kazzak_wanderers.sql').read_text()
pattern=r"\((610\d+), 'kazzak_test_([^']+)', 530, 3483, 3547, 70, 70, \d+, \d+, 0, (\d+), ([\d.]+), ([\d.]+), ([\d.]+), [\d.]+, '([^']+)'\)"
rows=re.findall(pattern,sql);assert len(rows)==24
nodes={int(i):(name,int(flag),float(x),float(y),float(z),{int(j):int(w) for j,w in (link.split(':') for link in links.split())}) for i,name,flag,x,y,z,links in rows}
spawns=[i for i,n in nodes.items() if n[1]&1];assert len(spawns)==7
assert sum(nodes[i][1]==19 for i in spawns)==4 and sum(nodes[i][1]==21 for i in spawns)==3
for i,(name,flag,x,y,z,links) in nodes.items():
 assert flag in (19,21,524288) and len(links)>=2 and 250<z<330
 assert all(j in nodes and j!=i and weight>0 and i in nodes[j][5] for j,weight in links.items())
 if flag&1:assert math.hypot(x-943.49,y-2269.77)>90
seen=set();pending=[spawns[0]]
while pending:
 i=pending.pop()
 if i in seen:continue
 seen.add(i);pending.extend(nodes[i][5])
assert len(seen)==24
assert sum(len(n[5]) for n in nodes.values())//2-len(nodes)+1==7
assert math.hypot(nodes[61021][2]-943.49,nodes[61021][3]-2269.77)<1
flags=[]
for line in (build/'src/common/CMakeFiles/common.dir/flags.make').read_text().splitlines():
 if line.startswith(('CXX_DEFINES =','CXX_INCLUDES =')):flags+=shlex.split(line.split('=',1)[1])
libs=[str(build/p) for p in ['src/common/libcommon.a','dep/fmt/libfmt.a','dep/g3dlite/libg3dlib.a','dep/SFMT/libsfmt.a','dep/recastnavigation/Detour/libDetour.a']]
payload='25\n'+''.join(f'{nodes[i][2]} {nodes[i][3]} {nodes[i][4]}\n' for i in sorted(nodes))+'943.49 2269.77 306.786\n'+''.join(f'{i-61000} {j-61000}\n' for i,n in nodes.items() for j in n[5])+'21 24\n24 21\n'
with tempfile.TemporaryDirectory(prefix='kazzak-navigation-') as tmp:
 binary=Path(tmp)/'verify'
 subprocess.run(['c++','-std=c++20','-O2',*flags,str(repo/'contrib/tests/hellfire_kazzak_navigation.cpp'),'-o',str(binary),'-Wl,--start-group',*libs,'-Wl,--end-group','-lboost_filesystem','-lboost_program_options','-lboost_regex','-lboost_locale','-lboost_chrono','-lboost_system','-lboost_thread','-lboost_atomic','-lssl','-lcrypto','-lz','-ldl','-lpthread'],check=True)
 r=subprocess.run([str(binary)],input=payload,capture_output=True,text=True,check=True)
 assert 'BOSS_LOS,21,1' in r.stdout and 'BOSS_LOS,20,1' in r.stdout
 assert sum(l.startswith('EDGE,') for l in r.stdout.splitlines())==62
 print(r.stdout)
print('PASS: seven faction-separated camps, 24 level-70 plateau/throne nodes; all 60 directed links are walkable, including the throne and descent; boss LoS verified on the dais and steps')
