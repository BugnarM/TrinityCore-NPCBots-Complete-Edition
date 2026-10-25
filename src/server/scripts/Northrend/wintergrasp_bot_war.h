#ifndef WINTERGRASP_BOT_WAR_H
#define WINTERGRASP_BOT_WAR_H

#include <algorithm>
#include <cstdint>

namespace WintergraspBotWar
{
constexpr uint32_t MaxParticipants = 120;
constexpr uint32_t SquadSize = 5;

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
    KeepCenter, KeepWest, KeepEast, WorkshopWest, WorkshopEast, LaneCount
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
        constexpr Lane lanes[] = { Center, West, East, Center, West, East, WorkshopWest, WorkshopEast };
        return lanes[squad % 8];
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
    return lane == WorkshopWest || lane == WorkshopEast;
}
}

#endif
