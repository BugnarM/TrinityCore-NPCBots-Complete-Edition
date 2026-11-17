#include "../../src/server/game/AI/NpcBots/bothealingpolicy.h"
#include <cassert>
#include <iostream>

using namespace NPCBots::Healing;
int main()
{
    for (unsigned hp = 0; hp <= 100; ++hp)
        for (unsigned personal : {80u, 90u, 95u, 100u})
            for (unsigned global : {85u, 95u, 100u})
            {
                bool const starts = StartTopOff(true, true, false, false, hp, personal, global);
                assert(!starts || !CancelDirectHeal(true, hp, personal, global));
                assert(!StartTopOff(true, true, true, false, hp, personal, global));
                assert(!StartTopOff(true, true, false, true, hp, personal, global));
                assert(!StartTopOff(false, true, false, false, hp, personal, global));
            }
    for (unsigned flags = 0; flags < 128; ++flags)
        for (unsigned other = 0; other < 128; ++other)
        {
            auto key = [](unsigned hp, unsigned f) {
                return Priority(hp, f&1, f&2, f&4, f&8, f&16, f&32);
            };
            assert(key(35,flags) > key(36,other));
            assert(key(60,flags) > key(61,other));
            assert(key(85,flags) > key(86,other));
        }
    assert(Priority(40,true,true,false,false,false,false) > Priority(40,false,true,false,false,false,false));
    auto const healing = Project(940,1000,20.,2.5);
    assert(healing.health==990 && healing.percent==99 && healing.missing==10);
    auto const damage = Project(10,1000,-10000.,2.5);
    assert(damage.health<0 && damage.missing>1000 && damage.percent<0);
    assert(Project(UINT32_MAX,UINT32_MAX,1e20,2.5).health==INT32_MAX);
    assert(Project(1,0,0,2).percent==100);
    assert(FiniteClamp(NAN,3.f,.1f,10.f)==3.f);
    assert(FiniteClamp(-3.f,3.f,.1f,10.f)==.1f);
    DecisionMetrics metrics;
    for (unsigned i=1;i<=100;++i) metrics.Record(i);
    assert(metrics.Percentile(50)==50 && metrics.Percentile(95)==100 && metrics.maximum==100);
    std::cout << "PASS: heal start/cancel boundaries, combat isolation, emergency tiers, safe projection and diagnostics\n";
}
