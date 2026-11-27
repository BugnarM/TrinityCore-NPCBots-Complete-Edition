#ifndef WINTERGRASP_TACTICS_H
#define WINTERGRASP_TACTICS_H

#include "wintergrasp_bot_war.h"
#include <array>
#include <limits>

namespace WintergraspBotWar
{
constexpr uint32_t ProfileData = 80502;
constexpr uint32_t RoutePointData = 80503;
constexpr uint32_t MovementLockedData = 80504;
constexpr int32_t FocusTargetGuid = 80510;
constexpr uint32_t MaxSquads = MaxParticipants / SquadSize;

enum SoldierProfile : uint8_t
{
    Warrior, FrostMage, HolyPriest, Marksman, HolyPaladin, RestorationShaman, Rogue, Warlock, ProfileCount
};
constexpr bool IsMelee(uint32_t profile) { return profile == Warrior || profile == Rogue; }
constexpr bool IsHealer(uint32_t profile) { return profile == HolyPriest || profile == HolyPaladin || profile == RestorationShaman; }
constexpr SoldierProfile ProfileForSlot(uint32_t slot)
{
    uint32_t squad = slot / SquadSize;
    switch (slot % SquadSize)
    {
        case 0: return Warrior;
        case 1: return squad % 2 ? Warlock : FrostMage;
        case 2: return squad % 3 == 0 ? HolyPriest : squad % 3 == 1 ? HolyPaladin : RestorationShaman;
        case 3: return squad % 3 == 0 ? Marksman : squad % 3 == 1 ? FrostMage : Warlock;
        default: return squad % 2 ? Rogue : Warrior;
    }
}
constexpr uint32_t StatTemplateForProfile(uint32_t profile)
{
    return IsMelee(profile) ? 0 : IsHealer(profile) ? 2 : profile == Marksman ? 3 : 1;
}
constexpr uint32_t PrimarySpell(uint32_t profile)
{
    constexpr uint32_t spells[] = {47486,42842,48123,49050,48801,49238,48638,47809};
    return profile < ProfileCount ? spells[profile] : 0;
}
constexpr uint32_t HealingSpell(uint32_t profile, bool emergency, bool hasHot, bool shieldBlocked)
{
    if (profile == HolyPaladin) return emergency ? 48821 : 48785;
    if (profile == RestorationShaman) return !hasHot ? 61301 : 49273;
    if (profile == HolyPriest) return emergency && !shieldBlocked ? 48066 : !hasHot ? 48068 : 48071;
    return 0;
}
constexpr float TargetPriority(float distance, float healthPct, bool healer, bool casting, bool threatensHealer,
    bool siegeThreat, bool focused, bool breakableControl, bool playerTarget = false, bool cannonTarget = false)
{
    return (healer ? 75.f : 0.f) + (casting ? 25.f : 0.f) + (threatensHealer ? 110.f : 0.f)
        + (siegeThreat ? 120.f : 0.f) + (focused ? 45.f : 0.f) + (playerTarget ? 30.f : 0.f)
        + (cannonTarget ? 40.f : 0.f) + (100.f - healthPct) * 0.5f
        - distance * 1.1f - (breakableControl ? 1000.f : 0.f);
}
constexpr bool ShouldSwitchTarget(float current, float candidate) { return candidate > current + 25.f; }
constexpr bool CanReserveAction(uint32_t now, uint32_t until) { return !until || int32_t(now - until) >= 0; }
constexpr bool PreserveVehicle(bool alive, bool soldierDied, bool humanPassenger, bool botDriver)
{
    return alive && (soldierDied || humanPassenger || botDriver);
}
constexpr uint32_t BreachScore(bool destroyed, uint32_t healthPct, uint32_t defenders)
{
    if (destroyed)
        return 0;
    uint32_t damageBonus = healthPct < 85 ? (85 - healthPct) * 2 : 0;
    uint32_t raw = 100 + std::min(healthPct, 100u) + std::min(defenders, 120u) * 6;
    return raw > damageBonus ? raw - damageBonus : 1u;
}

enum class VehicleRole { Catapult, Demolisher, SiegeEngine };

constexpr VehicleRole NextVehicle(bool attacking, uint32_t catapults, uint32_t demolishers, uint32_t siegeEngines, bool wallsStanding = true)
{
    uint32_t wallBreakers = demolishers + siegeEngines;
    uint32_t ratio = attacking ? (wallsStanding ? 5u : 2u) : 1u;
    if (wallBreakers >= ratio * (catapults + 1))
        return VehicleRole::Catapult;
    return demolishers <= siegeEngines ? VehicleRole::Demolisher : VehicleRole::SiegeEngine;
}
struct BattleCounters
{
    uint32_t Kills=0, BotDeaths=0, Built=0, Lost=0, RecoveredVehicles=0;
    uint32_t InfantryStalls=0, InfantryRecovered=0, VehicleStalls=0, VehicleRecovered=0;
    uint32_t Captures=0, FirstCaptureMs=0, Interrupts=0, Controls=0;
};
struct BattleTelemetry
{
    bool Active=false, Participated=false;
    uint32_t Serial=0, Attacker=0, Elapsed=0, FirstBreachMs=0, ArmyStartedMs=0;
    std::array<BattleCounters,2> Teams{};
    std::array<uint32_t,2> Battles{}, AttackerWins{};
    void Start(uint32_t serial, uint32_t attacker)
    {
        Active=true; Participated=false; Serial=serial; Attacker=attacker; Elapsed=FirstBreachMs=ArmyStartedMs=0; Teams={};
    }
    void Finish(uint32_t winner)
    {
        if (!Active) return;
        if (Participated)
        {
            ++Battles[Attacker];
            if (winner==Attacker) ++AttackerWins[Attacker];
        }
        Active=false;
    }
};
inline BattleTelemetry BotBattleStats;
}
#endif
