#ifndef CITY_RAIDS_H
#define CITY_RAIDS_H

#include "Define.h"
#include "ObjectGuid.h"
#include "Position.h"
#include <array>
#include <vector>
#include <set>

enum CityRaidType
{
    CITY_RAID_STORMWIND = 0,
    CITY_RAID_ORGRIMMAR = 1,
    CITY_RAID_COUNT     = 2
};

enum CityRaidEventId
{
    EVENT_CITY_RAID_STORMWIND = 86,
    EVENT_CITY_RAID_ORGRIMMAR = 87
};

enum CityRaidBossEntry
{
    BOSS_VARIAN_WRYNN = 29611,
    BOSS_THRALL       = 4949
};

enum CityRaidRole
{
    ROLE_ATTACKER = 0,
    ROLE_DEFENDER = 1
};

struct CityRaidBotMember
{
    ObjectGuid Guid;
    uint32 Entry{0};
    uint8 RoleType{ROLE_ATTACKER};
    uint8 Index{0};
    uint8 Checkpoint{0};
    bool IsDead{false};
};

struct CityRaidConfig
{
    uint32 EventId;
    uint32 MapId;
    uint32 CityZoneId;
    uint32 NeighborZoneId;
    uint32 BossEntry;
    uint32 AttackerTeam;
    uint32 AttackerFaction;
    uint32 DefenderTeam;
    uint32 DefenderFaction;
    Position GraveyardPos;
    Position DefenderGraveyardPos;
    std::vector<Position> Waypoints; // WP 0 .. 10
    std::array<Position, 3> CheckpointSpawns; // CP 0, 1, 2
    char const* CityName;
    char const* InvadingFactionName;
    char const* DefendingFactionName;
    char const* BossName;
};

void AddSC_event_city_raids();

#endif // CITY_RAIDS_H
