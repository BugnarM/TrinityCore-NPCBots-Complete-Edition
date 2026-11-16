// c++ -std=c++20 -O2 -Wall -Wextra -pedantic contrib/tests/wintergrasp_tactics.cpp -o /tmp/wg-tactics && /tmp/wg-tactics
#include "../../src/server/scripts/Northrend/wintergrasp_tactics.h"
#include <array>
#include <cassert>
#include <iostream>
using namespace WintergraspBotWar;
int main()
{
    std::array<unsigned,ProfileCount> count{};
    for (unsigned i=0;i<MaxParticipants;i+=SquadSize)
    {
        unsigned healers=0,melee=0;
        for (unsigned j=i;j<i+SquadSize;++j)
        {
            auto p=ProfileForSlot(j);
            assert(p<ProfileCount && PrimarySpell(p));
            assert(StatTemplateForProfile(p)<4);
            ++count[p];
            healers+=IsHealer(p);melee+=IsMelee(p);
        }
        assert(healers==1 && melee==2 && ProfileForSlot(i)==Warrior);
    }
    for(auto n:count)assert(n>0);
    assert(HealingSpell(HolyPriest,true,true,false)==48066);
    assert(HealingSpell(HolyPriest,true,true,true)==48071);
    assert(HealingSpell(HolyPaladin,true,false,false)==48821);
    assert(HealingSpell(RestorationShaman,false,false,false)==61301);
    assert(HealingSpell(RestorationShaman,true,true,false)==49273);
    float ordinary=TargetPriority(15,100,false,false,false,false,false,false);
    assert(TargetPriority(20,100,true,false,false,false,false,false)>ordinary);
    assert(TargetPriority(20,100,false,false,true,false,false,false)>ordinary);
    assert(TargetPriority(10,10,true,true,true,false,true,true)<ordinary);
    assert(!ShouldSwitchTarget(100,120)&&ShouldSwitchTarget(100,130));
    assert(CanReserveAction(10,0)&&!CanReserveAction(10,1000)&&CanReserveAction(1000,1000));
    for(bool alive:{false,true})for(bool dead:{false,true})for(bool human:{false,true})for(bool bot:{false,true})
        assert(PreserveVehicle(alive,dead,human,bot)==(alive&&(dead||human||bot)));
    assert(BreachScore(true,100,120)<BreachScore(false,1,0));
    assert(BreachScore(false,20,0)<BreachScore(false,80,0));
    assert(BreachScore(false,20,20)>BreachScore(false,80,0));
    BattleTelemetry t;
    t.Start(1,0);t.Finish(0);assert(t.Battles[0]==0); // unattended battle excluded
    t.Start(2,0);t.Participated=true;t.Finish(1);assert(t.Battles[0]==1&&t.AttackerWins[0]==0);
    t.Finish(0);assert(t.Battles[0]==1); // duplicate end is harmless
    t.Start(3,1);t.Participated=true;t.Finish(1);assert(t.Battles[1]==1&&t.AttackerWins[1]==1);
    t.Start(4,0);assert(t.Teams[0].Built==0&&t.Elapsed==0&&!t.Participated);
    assert(t.Battles[0]==1&&t.Battles[1]==1); // process totals survive a new battle
    std::cout<<"PASS: eight profiles, balanced squads, healing spells, healer protection/focus hysteresis, control reservations, vehicle preservation, breach scoring and battle statistics\n";
}
