#!/usr/bin/env python3
"""Execute production charm callbacks and ranged pursuit against core-like stubs."""
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
source = (repo / 'src/server/scripts/Northrend/zone_wintergrasp.cpp').read_text()

def function(text, signature):
    start = text.index(signature)
    end = text.index('{', start) + 1
    depth = 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end].replace(' override', '')

vehicle = source[source.index('struct npc_wg_bot_vehicle'):source.index('struct npc_wg_bot_soldier')]
turret = source[source.index('struct npc_wg_bot_siege_turret'):source.index('struct npc_wg_tower_cannon')]
soldier = source[source.index('struct npc_wg_bot_soldier'):source.index('struct npc_wg_bot_siege_turret')]
helper = function(source, 'bool IsBotWarSoldier(')
# Match the core contract: OnCharmed(false) schedules replacement with PossessedAI.
code = r'''
#include "src/server/scripts/Northrend/wintergrasp_tactics.h"
#include <cassert>
#include <cstdint>
#include <iostream>
#include <list>
#include <vector>
using uint32 = std::uint32_t;
using namespace WintergraspBotWar;
enum { UNIT_STATE_FLEEING=1,UNIT_STATE_CONFUSED=2 };
enum { TYPEID_UNIT, TYPEID_PLAYER, EVADE_REASON_NO_HOSTILES };
struct Creature;
struct Vehicle;
struct Unit
{
    int type=TYPEID_UNIT;
    uint32 entry=80501;
    bool player=false,exited=false,native=false;
    unsigned state=0;
    bool HasUnitState(unsigned mask) const{return (state&mask)!=0;}
    Creature* creature=nullptr;
    Vehicle* seatedIn=nullptr;
    Creature* ToCreature(){return creature;}
    Creature const* ToCreature() const{return creature;}
    bool IsCreature() const{return type==TYPEID_UNIT;}
    bool IsNPCBot() const{return native;}
    void ExitVehicle();
    int GetTypeId() const { return type; }
    uint32 GetEntry() const { return entry; }
};
struct Guid { bool cleared=false; void Clear(){cleared=true;} };
struct Motion
{
    unsigned chases=0;
    void MoveChase(Unit*,float){++chases;}
};
struct BotControl { bool battlefield=false;bool IsBattlefieldBot()const{return battlefield;} unsigned actions=0;void DoAction(int action){assert(action==1);++actions;} };
struct Creature
{
    bool charmed=false,moving=false,sight=true;
    float distance=40;
    Unit* charmer=nullptr;
    Vehicle* kit=nullptr;
    BotControl control;
    Vehicle* GetVehicleKit(){return kit;}
    BotControl* AI(){return &control;}
    BotControl const* GetBotAI()const{return &control;}
    Guid LastCharmerGUID;
    Motion motion;
    bool IsCharmed() const{return charmed;}
    Unit* GetCharmer(){return charmer;}
    bool IsWithinLOSInMap(Unit*) const{return sight;}
    bool IsWithinDistInMap(Unit*,float range) const{return distance<=range;}
    bool isMoving() const{return moving;}
    Motion* GetMotionMaster(){return &motion;}
    void SetFacingToObject(Unit*){}
};
struct Player : Unit
{
    bool alive=true,friendly=true,aboard=false,combat=false,mounted=false;
    float distance=4;
    Player(){type=TYPEID_PLAYER;}
    bool IsAlive() const{return alive;}
    bool IsFriendlyTo(Creature*) const{return friendly;}
    bool GetVehicle() const{return aboard;}
    bool IsInCombat() const{return combat;}
    bool IsMounted() const{return mounted;}
};
struct Vehicle { Unit* passenger=nullptr;Unit* GetPassenger(int){return passenger;} };
void Unit::ExitVehicle(){exited=true;seatedIn->passenger=nullptr;seatedIn=nullptr;}
std::vector<Player*> players;
void GetPlayerListInGrid(std::list<Player*>& out,Creature*,float radius)
{for(Player* player:players)if(player->distance<=radius)out.push_back(player);}
HELPER
YIELD
struct UnitAI
{
    unsigned scheduled=0;
    void OnCharmed(bool isNew){if(!isNew)++scheduled;}
};
struct Route
{
    bool Arrived=true;
    unsigned moves=0;
    void Move(Creature*){++moves;}
};
struct VehicleAI : UnitAI
{
    Creature* me;
    Route _route;
    uint32 _takeoverTimer=2000;
    explicit VehicleAI(Creature* c):me(c){}
VEHICLE
};
struct TurretAI : UnitAI
{
    Creature* me;
    explicit TurretAI(Creature* c):me(c){}
TURRET
};
struct SoldierAI
{
    Creature* me;
    uint32 _lostSightTimer=0;
    unsigned _profile=1,evades=0,attacks=0;
    explicit SoldierAI(Creature* c):me(c){}
    void EnterEvadeMode(int){++evades;}
    void Update(Unit* victim,uint32 diff)
    {
PURSUIT
        ++attacks;
    }
};
int main()
{
    Unit bot,player,unrelated;
    player.type=TYPEID_PLAYER;player.entry=80501;
    unrelated.entry=123;
    assert(IsBotWarSoldier(&bot)&&!IsBotWarSoldier(&player)&&!IsBotWarSoldier(&unrelated)&&!IsBotWarSoldier(nullptr));
    Creature nativeBody;Unit named;named.entry=900001;named.native=true;named.creature=&nativeBody;
    assert(!IsBotWarSoldier(&named)); // ordinary wanderer is never commandeered
    nativeBody.control.battlefield=true;assert(IsBotWarSoldier(&named));
    for(uint32 entry=80501;entry<=80508;++entry)
    {
        bot.entry=entry;
        Creature vehicle;vehicle.charmed=true;vehicle.charmer=&bot;
        VehicleAI driver(&vehicle);TurretAI gunner(&vehicle);
        driver.OnCharmed(false);gunner.OnCharmed(false);
        assert(driver.scheduled==0&&gunner.scheduled==0);
        vehicle.charmed=false;vehicle.charmer=nullptr;
        driver.OnCharmed(false);gunner.OnCharmed(false);
        assert(driver.scheduled==0&&gunner.scheduled==0);
        assert(driver._takeoverTimer==0&&!driver._route.Arrived&&driver._route.moves==0);
        assert(vehicle.LastCharmerGUID.cleared);
        vehicle.charmed=true;vehicle.charmer=&player;
        driver.OnCharmed(false);gunner.OnCharmed(false);
        assert(driver.scheduled==1&&gunner.scheduled==1);
        // Core restores the script after the player leaves PossessedAI.
        vehicle.charmed=false;vehicle.charmer=nullptr;
        driver.OnCharmed(true);gunner.OnCharmed(true);
        assert(driver._route.moves==0&&driver.scheduled==1&&gunner.scheduled==1);
    }
    Creature transport,botBody;
    Unit occupant;occupant.creature=&botBody;
    Vehicle seats;seats.passenger=&occupant;occupant.seatedIn=&seats;transport.kit=&seats;
    Player ally;players={&ally};
    ally.friendly=false;assert(!YieldBotWarSeat(&transport));ally.friendly=true;
    ally.alive=false;assert(!YieldBotWarSeat(&transport));ally.alive=true;
    ally.combat=true;assert(!YieldBotWarSeat(&transport));ally.combat=false;
    ally.mounted=true;assert(!YieldBotWarSeat(&transport));ally.mounted=false;
    ally.aboard=true;assert(!YieldBotWarSeat(&transport));ally.aboard=false;
    ally.distance=9;assert(!YieldBotWarSeat(&transport));ally.distance=4;
    transport.sight=false;assert(!YieldBotWarSeat(&transport));transport.sight=true;
    assert(YieldBotWarSeat(&transport)&&occupant.exited&&botBody.control.actions==1);
    assert(seats.passenger==nullptr&&!YieldBotWarSeat(&transport));
    seats.passenger=&ally;assert(!YieldBotWarSeat(&transport)); // never eject a human
    Creature soldier;SoldierAI ranged(&soldier);
    Unit target;
    ranged.Update(&target,500);
    assert(soldier.motion.chases==1&&ranged.attacks==0);
    soldier.moving=true;ranged.Update(&target,500);
    assert(soldier.motion.chases==1&&ranged.attacks==0);
    soldier.distance=29;ranged.Update(&target,500);
    assert(ranged.attacks==1);
    soldier.sight=false;soldier.moving=false;
    for(int i=0;i<15;++i)ranged.Update(&target,500);
    assert(ranged.evades==0&&ranged.attacks==1);
    ranged.Update(&target,500);
    assert(ranged.evades==1&&ranged._lostSightTimer==0);
    soldier.sight=true;ranged.Update(&target,500);
    assert(ranged.attacks==2);
    SoldierAI melee(&soldier);melee._profile=0;soldier.distance=40;
    melee.Update(&target,500);assert(melee.attacks==1);
    std::cout<<"PASS: NPC driver/gunner retain AI; nearby eligible humans receive a vacant seat; humans never ejected; ranged and lost-sight recovery\n";
}
'''
start = soldier.index('        if (!victim)\n')
end = soldier.index('        if (_route.Blocked(wg))', start)
for key, val in [('HELPER', helper), ('VEHICLE', function(vehicle, '    void OnCharmed(')),
                 ('TURRET', function(turret, '    void OnCharmed(')), ('YIELD', function(source, 'bool YieldBotWarSeat(')), ('PURSUIT', soldier[start:end])]:
    code = code.replace(key, val)
with tempfile.TemporaryDirectory(prefix='wg-combat-control-') as tmp:
    cpp, binary = Path(tmp) / 'test.cpp', Path(tmp) / 'test'
    cpp.write_text(code)
    subprocess.run(['c++', '-std=c++20', '-O2', '-Wall', '-Wextra', '-Werror', '-I', str(repo), str(cpp), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
