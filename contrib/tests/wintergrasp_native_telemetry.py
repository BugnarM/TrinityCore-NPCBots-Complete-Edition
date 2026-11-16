#!/usr/bin/env python3
"""Execute native vehicle death/accounting even with player-controlled faction/AI."""
from pathlib import Path
import subprocess
import tempfile
repo=Path(__file__).resolve().parents[2]
source=(repo/'src/server/scripts/Battlefield/BattlefieldWG.cpp').read_text()
def function(signature):
    start=source.index(signature);end=source.index('{',start)+1;depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]
code=r'''
#include "src/server/scripts/Northrend/wintergrasp_tactics.h"
#include <array>
#include <cassert>
#include <iostream>
#include <set>
using uint32=std::uint32_t;
using namespace WintergraspBotWar;
enum{TEAM_ALLIANCE,TEAM_HORDE,PVP_TEAMS_COUNT};
enum{BATTLEFIELD_WG_DATA_VEHICLE_A,BATTLEFIELD_WG_DATA_VEHICLE_H};
struct Unit{uint32 guid;bool vehicle=true;bool IsVehicle() const{return vehicle;}uint32 GetGUID()const{return guid;}};
struct BattlefieldWG
{
 bool war=true;unsigned updates=0;
 std::array<std::set<uint32>,2> m_vehicles;
 std::array<int,2> count{};
 bool IsWarTime()const{return war;}
 void UpdateData(int id,int delta){count[id]+=delta;assert(count[id]>=0);}
 void UpdateVehicleCountWG(){++updates;}
 bool FindAndRemoveVehicleFromList(Unit*);
 void OnUnitDeath(Unit*);
};
REMOVE
DEATH
int main()
{
 BotBattleStats.Start(1,0);BattlefieldWG wg;Unit a{7},h{8},soldier{9,false};
 wg.m_vehicles[0].insert(a.guid);wg.m_vehicles[1].insert(h.guid);wg.count={1,1};
 // Only the registered team matters: control/faction changes cannot misattribute a loss.
 wg.OnUnitDeath(&a);assert(BotBattleStats.Teams[0].Lost==1&&BotBattleStats.Teams[1].Lost==0&&wg.count[0]==0);
 wg.OnUnitDeath(&a);assert(BotBattleStats.Teams[0].Lost==1&&wg.updates==1);
 wg.OnUnitDeath(&h);assert(BotBattleStats.Teams[1].Lost==1&&wg.count[1]==0);
 wg.OnUnitDeath(&soldier);assert(wg.updates==2);
 wg.m_vehicles[0].insert(a.guid);wg.count[0]=1;
 assert(wg.FindAndRemoveVehicleFromList(&a)); // a living despawn/battle cleanup is not a death
 assert(BotBattleStats.Teams[0].Lost==1);
 wg.m_vehicles[0].insert(a.guid);wg.count[0]=1;wg.war=false;
 wg.OnUnitDeath(&a);assert(wg.count[0]==1&&BotBattleStats.Teams[0].Lost==1);
 std::cout<<"PASS: native telemetry attributes human/bot vehicle deaths by registry, counts once and excludes live cleanup\n";
}
'''
for marker,signature in [('REMOVE','bool BattlefieldWG::FindAndRemoveVehicleFromList('),('DEATH','void BattlefieldWG::OnUnitDeath(')]:
    code=code.replace('\n'+marker+'\n','\n'+function(signature)+'\n')
with tempfile.TemporaryDirectory(prefix='wg-native-telemetry-') as temp:
    cpp,binary=Path(temp)/'test.cpp',Path(temp)/'test';cpp.write_text(code)
    subprocess.run(['c++','-std=c++20','-O2','-Wall','-Wextra','-Werror','-I',str(repo),str(cpp),'-o',str(binary)],check=True)
    subprocess.run([str(binary)],check=True)
