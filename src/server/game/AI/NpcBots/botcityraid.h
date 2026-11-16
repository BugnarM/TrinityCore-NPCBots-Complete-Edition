#ifndef NPCBOTS_CITY_RAID_TACTICS_H
#define NPCBOTS_CITY_RAID_TACTICS_H

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace NPCBots::CityRaid
{
// CreatureAI protocol shared by the script controller and native class AI.
enum Data : std::uint32_t { FormationOwned = 908600, BossFormation, AllowTaunt };
enum Guid : std::int32_t { FocusTarget = 908600 };
enum class State : std::uint8_t { Idle, Spawning, Marching, Boss, Retreating, Celebrating };
enum class Role : std::uint8_t { Tank, Healer, Melee, Ranged };
constexpr float Pi = 3.14159265358979323846f;
constexpr unsigned AttackerCount = 40, DefenderCount = 15;
constexpr unsigned DecisionMs = 250, PathBudget = 64;
constexpr unsigned QueryNodes = 8192;

inline bool Tick(std::uint32_t& remaining, std::uint32_t diff)
{
    if (remaining > diff) { remaining -= diff; return false; }
    remaining = 0;
    return true;
}
// Distance to the finite march segment: never extend the line into another zone.
inline bool NearRouteSegment(float x,float y,float ax,float ay,float bx,float by,float radius)
{
    float dx=bx-ax,dy=by-ay,length=dx*dx+dy*dy;
    float t=length>0.f?std::clamp(((x-ax)*dx+(y-ay)*dy)/length,0.f,1.f):0.f;
    float ex=x-(ax+t*dx),ey=y-(ay+t*dy);
    return ex*ex+ey*ey<=radius*radius;
}
constexpr Role AttackerRole(unsigned i)
{
    return i < 4 ? Role::Tank : i < 12 ? Role::Healer : i < 26 ? Role::Melee : Role::Ranged;
}
// Preserve global wave phase across irregular world ticks; coalesce missed waves.
inline bool WaveTick(std::uint32_t& remaining, std::uint32_t period, std::uint32_t diff)
{
    if (remaining>diff) { remaining-=diff; return false; }
    remaining=period-(diff-remaining)%period;
    return true;
}
constexpr unsigned AttackerSubgroup(unsigned index)
{
    if (index<4) return index;
    if (index<12) return index-4; // one healer in every five-member subgroup
    unsigned dps=index-12;
    return dps<12?dps/3:4+(dps-12)/4;
}
constexpr bool ShouldSwap(float health, float burst, bool mortalStrike, float replacementHealth)
{
    return replacementHealth >= 60.f && (health < 35.f || burst >= 25.f || (mortalStrike && health < 70.f));
}
constexpr float DispelPriority(bool tank, float health, bool removableControl, bool healingDebuff)
{
    return (tank?200.f:0.f)+(removableControl?800.f:0.f)+(healingDebuff?400.f:0.f)+100.f-health;
}
constexpr bool VictoryEligible(bool killedByRaid, bool dead, bool seenAlive, bool engaged)
{
    return killedByRaid || (dead && seenAlive && engaged);
}
struct Offset { float X, Y; };
inline Offset BossOffset(Role role, unsigned index, bool mainTank, float meleeRadius, bool whirlwind)
{
    float a = 0.f, radius;
    switch (role)
    {
        case Role::Tank:
            a = mainTank ? 0.f : (index % 2 ? -1.25f : 1.25f);
            radius = mainTank ? meleeRadius + 3.f : meleeRadius + 5.f + float(index / 2);
            break;
        case Role::Melee:
            a = Pi - 0.65f + 1.3f * float(index % 14) / 13.f;
            radius = whirlwind ? 15.f : meleeRadius;
            break;
        case Role::Healer:
            a = 2.f * Pi * float(index % 8) / 8.f + Pi / 8.f;
            radius = 19.f;
            break;
        default:
            a = 2.f * Pi * float(index % 14) / 14.f;
            radius = 30.f;
            break;
    }
    return { radius * std::cos(a), radius * std::sin(a) };
}
constexpr unsigned CombatCredit(unsigned previous, bool engaged)
{
    return std::min(3600u, previous + unsigned(engaged));
}
constexpr bool Eligible(unsigned activeSeconds, bool level80, bool attackingTeam, bool nearby)
{
    return activeSeconds >= 20 && level80 && attackingTeam && nearby;
}
}
#endif
