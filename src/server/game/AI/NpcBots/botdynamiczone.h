#ifndef NPCBOT_DYNAMIC_ZONE_MGR_H
#define NPCBOT_DYNAMIC_ZONE_MGR_H

#include "Define.h"
#include <map>
#include <string>
#include <vector>

namespace NPCBots::DynamicZone
{
struct ZonePopulation
{
    uint32 mapId = 0;
    uint32 zoneId = 0;
    std::string zoneName;
    uint8 minLevel = 1;
    uint8 maxLevel = 80;
    uint32 players = 0;
    uint32 target = 0;
    uint32 darkKnightRespawnTimer = 0;
    std::vector<uint32> activeBots;
    std::vector<ObjectGuid> darkKnights;
};

class DynamicZoneMgr
{
public:
    static DynamicZoneMgr* Instance();
    void Update(uint32 diff);
    std::vector<ZonePopulation> GetActiveZonesStatus() const;
    void Reset();

private:
    DynamicZoneMgr() = default;
    std::map<uint64, ZonePopulation> _populatedZones;
    uint32 _scanTimer = 0;
    uint64 _lastSpawnZone = 0;
};
}
#endif
