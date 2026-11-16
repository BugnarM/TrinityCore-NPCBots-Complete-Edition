// c++ -std=c++20 -O2 -Wall -Wextra -Werror contrib/tests/city_raid_tactics.cpp -o /tmp/city-tactics && /tmp/city-tactics
#include "../../src/server/game/AI/NpcBots/botcityraid.h"
#include <array>
#include <cassert>
#include <iostream>
#include <vector>
using namespace NPCBots::CityRaid;
int main()
{
    std::array<unsigned,4> roles{};
    for (unsigned i=0;i<40;++i) ++roles[unsigned(AttackerRole(i))];
    assert((roles==std::array<unsigned,4>{4,8,14,14}));
    for (unsigned n=0;n<=40000;n+=17)
    {
        std::uint32_t timer=n;
        assert(!Tick(timer,0)||!n);
        assert(Tick(timer,n+1)&&timer==0); // no unsigned underflow
    }
    std::array<unsigned,8> subgroups{},healers{};
    for (unsigned i=0;i<40;++i) {++subgroups[AttackerSubgroup(i)];if(AttackerRole(i)==Role::Healer)++healers[AttackerSubgroup(i)];}
    for (unsigned i=0;i<8;++i) assert(subgroups[i]==5&&healers[i]==1);
    std::uint32_t wave=20000;unsigned waves=0,elapsed=0;
    while (elapsed<1800000) {unsigned diff=std::min(1800000-elapsed,1+(elapsed*7)%901);elapsed+=diff;waves+=WaveTick(wave,20000,diff);}
    assert(waves==90&&wave==20000);
    assert(WaveTick(wave,20000,65000)&&wave==15000); // one revive after a stall, preserve future phase
    assert(ShouldSwap(20,0,false,90));
    assert(ShouldSwap(70,26,false,90));
    assert(ShouldSwap(65,0,true,80));
    assert(!ShouldSwap(80,0,true,80));
    assert(!ShouldSwap(10,60,true,59));
    std::vector<Offset> spread;
    for (unsigned i=0;i<8;++i) spread.push_back(BossOffset(Role::Healer,i,false,3,false));
    for (unsigned i=0;i<14;++i) spread.push_back(BossOffset(Role::Ranged,i,false,3,false));
    for (unsigned i=0;i<spread.size();++i)
        for(unsigned j=i+1;j<spread.size();++j)
            assert(std::hypot(spread[i].X-spread[j].X,spread[i].Y-spread[j].Y)>=10.99f);
    for (unsigned i=0;i<14;++i)
    {
        auto p=BossOffset(Role::Melee,i,false,3,false);assert(p.X<0&&std::fabs(std::hypot(p.X,p.Y)-3)<0.01f);
        p=BossOffset(Role::Melee,i,false,3,true);assert(std::fabs(std::hypot(p.X,p.Y)-15)<0.01f);
    }
    assert(BossOffset(Role::Tank,0,true,3,false).X>0);
    assert(DispelPriority(true,80,true,false)>DispelPriority(false,80,true,false));
    assert(DispelPriority(true,80,true,false)>DispelPriority(true,10,false,false));
    assert(!Eligible(19,true,true,true));assert(Eligible(20,true,true,true));
    assert(!Eligible(30,false,true,true)&&!Eligible(30,true,false,true)&&!Eligible(30,true,true,false));
    for (unsigned mask=0;mask<8;++mask)
        assert(VictoryEligible(false,mask&1,mask&2,mask&4)==(mask==7));
    assert(VictoryEligible(true,false,false,false));
    assert(CombatCredit(3600,true)==3600);
    std::cout<<"PASS: exact 4/8/28 roster, non-underflowing timers, health/burst/debuff tank swaps, >10m spread, rear melee/Whirlwind evacuation, dispel ordering and victory/reward eligibility\n";
}
