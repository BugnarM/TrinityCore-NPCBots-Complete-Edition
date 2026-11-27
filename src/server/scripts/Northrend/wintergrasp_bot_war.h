#ifndef WINTERGRASP_BOT_WAR_H
#define WINTERGRASP_BOT_WAR_H

#include <algorithm>
#include <cstdint>

namespace WintergraspBotWar
{
constexpr uint32_t MaxParticipants = 120;
constexpr uint32_t SquadSize = 5;
constexpr uint32_t RankCreditData = 80501;

// Recruit starts with one aura stack: five nearby kills promote to Corporal,
// five further kills to Lieutenant, matching BattlefieldWG::PromotePlayer.
constexpr uint32_t RankForCredits(uint32_t credits) { return credits >= 10 ? 2 : credits >= 5 ? 1 : 0; }
constexpr uint32_t MaxConcurrentBuilders = 2;
constexpr uint32_t ConcurrentBuilderLimit(uint32_t ownedWorkshops, bool attacker = false)
{
    return attacker ? std::clamp(ownedWorkshops + 1, 3u, 4u)
                    : std::clamp(ownedWorkshops, MaxConcurrentBuilders, 3u);
}
// Wintergrasp BotWar uses its own isolated rapid respawn cadence instead of
// the 5-minute open-world wanderer respawn timer.
constexpr uint32_t BotRespawnDelayMs = 30000;
constexpr uint32_t WorkshopCaptureReserveSquads = 8;
constexpr uint32_t SquadStagingTimeoutMs = 8000;
constexpr uint32_t SquadStagingMinReady = 3;
constexpr bool ShouldHoldSquadStaging(uint32_t now, uint32_t rallyUntil, uint32_t readyCount, bool inCombat, bool hasVehicle)
{
    return !inCombat && !hasVehicle && rallyUntil > now && readyCount < SquadStagingMinReady;
}
constexpr bool CanMobilizeKeepDefender(bool attacker, float posX)
{
    return !attacker || posX < 5150.f;
}
constexpr uint32_t FactoryPhaseMask(bool alliance, bool keep)
{
    return keep ? (alliance ? 128u : 64u) : (alliance ? 32u : 16u);
}
constexpr bool HasVehicleBuildRank(uint32_t /*credits*/, bool /*attacker*/)
{
    // Bots can build siege vehicles without rank; only human players require Corporal / Lieutenant rank.
    return true;
}
constexpr uint32_t CastRetryDelay(bool success, uint32_t cooldown) { return success ? cooldown : 500; }
constexpr uint32_t WorkshopResponseSquads(bool owned, uint32_t allies, uint32_t enemies, bool attacker = false)
{
    if (!owned && allies >= enemies + 5)
        return 0;
    uint32_t cap = (!owned && attacker) ? 3u : 2u;
    if (!owned)
        return std::min(cap, std::max(attacker ? 2u : 1u, (enemies + 5 - allies + SquadSize - 1) / SquadSize));
    if (!enemies || allies >= enemies + 5)
        return 0;
    return std::min(2u, (enemies + 5 - allies + SquadSize - 1) / SquadSize);
}


// Despawn runs OnCreatureRemove synchronously and can erase any live registry
// entry. Iterators into that registry must not survive a removal callback.
template<class Registry, class Visitor>
void VisitVehicleSnapshot(Registry const& vehicles, Visitor&& visit)
{
    Registry const snapshot = vehicles;
    for (auto const& guid : snapshot)
        visit(guid);
}

enum Lane : uint8_t
{
    Center, West, East, South, SouthWest, SouthEast,
    KeepCenter, KeepWest, KeepEast, WorkshopWest, WorkshopEast,
    GuardSouth, GuardSouthWest, GuardSouthEast, WorkshopSouthWest, WorkshopSouthEast, LaneCount
};

constexpr uint32_t BotBudget(uint32_t target, uint32_t capacity, uint32_t humans)
{
    uint32_t total = std::min(std::min(target, capacity), MaxParticipants);
    return humans < total ? total - humans : 0;
}

constexpr Lane SquadLane(bool attacker, uint32_t slot)
{
    uint32_t squad = slot / SquadSize;
    if (attacker)
    {
        // 105 assault troops and 15 tower guards at a full roster (3 squads keep
        // southern towers watched and can flex forward when towers are safe).
        constexpr Lane lanes[] = {
            Center, GuardSouth, West, GuardSouthWest, East, GuardSouthEast, Center, West,
            East, Center, Center, West, West, East, East, Center,
            West, Center, East, West, Center, East, West, East
        };
        return lanes[squad % 24];
    }
    constexpr Lane lanes[] = { KeepCenter, KeepWest, KeepEast, KeepCenter, South, SouthWest, SouthEast, KeepCenter };
    return lanes[squad % 8];
}

constexpr bool IsCounterAttack(Lane lane)
{
    return lane == South || lane == SouthWest || lane == SouthEast;
}

constexpr bool IsAssault(Lane lane)
{
    return lane == Center || lane == West || lane == East;
}

constexpr bool IsWorkshop(Lane lane)
{
    return lane == WorkshopWest || lane == WorkshopEast || lane == WorkshopSouthWest || lane == WorkshopSouthEast;
}

constexpr bool IsTowerDefense(Lane lane)
{
    return lane == GuardSouth || lane == GuardSouthWest || lane == GuardSouthEast;
}

constexpr uint32_t GuardedTowerEntry(Lane lane)
{
    return lane == GuardSouth ? 190357 : lane == GuardSouthWest ? 190356 : lane == GuardSouthEast ? 190358 : 0;
}

constexpr Lane ResolveTowerDefense(Lane lane, bool towerDestroyed)
{
    if (!towerDestroyed)
        return lane;
    return lane == GuardSouth ? Center : lane == GuardSouthWest ? West : lane == GuardSouthEast ? East : lane;
}
constexpr Lane VehicleLaneForWorkshop(bool attacker, Lane fallback, bool westSide, bool eastSide, bool keepWest, bool keepEast)
{
    if (westSide)
        return attacker ? West : SouthWest;
    if (eastSide)
        return attacker ? East : SouthEast;
    if (keepWest)
        return KeepWest;
    if (keepEast)
        return KeepEast;
    return fallback;
}

// WotLK siege vehicles in creature_template carry Classic base health (expansion == 0: 35k/55k/85k)
// before player gear scaling. Scale them to WotLK siege durability (~105k/165k/255k) for BotWar.
constexpr uint32_t BotWarVehicleMaxHealth(uint32_t baseMaxHealth, uint32_t expansion)
{
    return expansion == 0 ? baseMaxHealth * 3u : baseMaxHealth;
}
}

#endif
