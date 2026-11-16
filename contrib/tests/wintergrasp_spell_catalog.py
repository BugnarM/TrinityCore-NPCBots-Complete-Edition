#!/usr/bin/env python3
"""Check the production profile spell catalog against the installed WotLK DBC."""
from pathlib import Path
import struct
import subprocess
import tempfile
repo=Path(__file__).resolve().parents[2]
dbc=Path('/home/trinity/server/data/dbc/Spell.dbc')
code=r'''
#include "src/server/scripts/Northrend/wintergrasp_tactics.h"
#include <iostream>
using namespace WintergraspBotWar;
int main(){for(unsigned p=0;p<ProfileCount;++p){std::cout<<p<<' '<<PrimarySpell(p);if(IsHealer(p))std::cout<<' '<<HealingSpell(p,false,true,true)<<' '<<HealingSpell(p,true,false,false);std::cout<<'\n';}}
'''
with tempfile.TemporaryDirectory(prefix='wg-spell-catalog-') as temp:
    cpp,binary=Path(temp)/'test.cpp',Path(temp)/'test';cpp.write_text(code)
    subprocess.run(['c++','-std=c++20','-O2','-I',str(repo),str(cpp),'-o',str(binary)],check=True)
    profiles=[list(map(int,line.split())) for line in subprocess.check_output([str(binary)],text=True).splitlines()]
data=dbc.read_bytes();n,fields,size,_=struct.unpack_from('<4I',data,4);strings=data[20+n*size:]
wanted={spell for row in profiles for spell in row[1:]}|{49284,49236,51722,498,1766,57994,6552,34490,2139,47860}
spells={}
for i in range(n):
    row=struct.unpack_from('<'+str(fields)+'I',data,20+i*size)
    if row[0] in wanted:spells[row[0]]=(strings[row[136]:].split(b'\0')[0].decode(),row[71:74])
expected=['Mortal Strike','Frostbolt','Smite','Aimed Shot','Exorcism','Lightning Bolt','Sinister Strike','Shadow Bolt']
for row in profiles:
    profile,primary,*heals=row
    assert spells[primary][0]==expected[profile],(profile,primary,spells[primary])
    if heals:
        assert 10 in spells[heals[0]][1],(profile,heals[0],spells[heals[0]]) # direct healing effect
        assert 10 in spells[heals[1]][1] or profile==2 # priest emergency shield is an aura
assert wanted==spells.keys()
print('PASS: production primary/healing spells for all eight profiles and their support abilities match the installed Spell.dbc')
