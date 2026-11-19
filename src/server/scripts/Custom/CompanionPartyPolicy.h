#ifndef TRINITYCORE_COMPANION_PARTY_POLICY_H
#define TRINITYCORE_COMPANION_PARTY_POLICY_H

#include <cstdint>

namespace NPCBots::Companions
{
inline constexpr unsigned MaxCompanions = 4;

enum class Role { Healer, Tank, Damage };

constexpr Role NextRole(unsigned healers, unsigned tanks)
{
    if (!healers)
        return Role::Healer;
    if (!tanks)
        return Role::Tank;
    return Role::Damage;
}

constexpr bool CanFill(Role role, bool canHeal, bool canTank)
{
    switch (role)
    {
        case Role::Healer: return canHeal;
        case Role::Tank: return canTank;
        case Role::Damage: return true;
    }
    return false;
}

constexpr std::uint32_t CooldownRemaining(std::uint64_t now, std::uint64_t started, std::uint32_t duration)
{
    if (!started)
        return 0;
    std::uint64_t const elapsed = now > started ? now - started : 0;
    return elapsed >= duration ? 0 : duration - std::uint32_t(elapsed);
}
}

#endif
