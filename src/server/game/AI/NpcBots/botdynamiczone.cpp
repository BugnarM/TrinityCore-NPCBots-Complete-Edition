#include "botdynamiczone.h"
#include "botdynamiczonepolicy.h"
#include "botdatamgr.h"
#include "botconfig.h"
#include "botwanderful.h"
#include "bot_ai.h"
#include "World.h"
#include "WorldSession.h"
#include "Player.h"
#include "Map.h"
#include "DBCStores.h"
#include "Containers.h"

namespace NPCBots::DynamicZone
{
DynamicZoneMgr* DynamicZoneMgr::Instance()
{
    static DynamicZoneMgr instance;
    return &instance;
}

void DynamicZoneMgr::Update(uint32 diff)
{
    if (!BotCfg::IsDynamicZoneWanderersEnabled())
    {
        if (!_populatedZones.empty()) Reset();
        return;
    }
    uint32 const interval = BotCfg::GetDynamicZoneUpdateInterval() * 1000;
    if (diff < interval - std::min(_scanTimer, interval))
    {
        _scanTimer += diff;
        return;
    }
    _scanTimer = 0;

    std::map<uint64, uint32> active;
    for (auto const& [_, session] : sWorld->GetAllSessions())
    {
        Player* player = session ? session->GetPlayer() : nullptr;
        if (!player || !player->IsInWorld() || player->IsBeingTeleported() || player->IsInFlight() ||
            !player->FindMap() || !player->GetMap()->GetEntry()->IsContinent() || !player->GetZoneId())
            continue;
        ++active[ZoneKey(player->GetMapId(), player->GetZoneId())];
    }

    // Queue removals through BotDataMgr: never delete map-owned creatures here.
    for (auto it = _populatedZones.begin(); it != _populatedZones.end();)
    {
        auto& pop = it->second;
        bool const occupied = active.contains(it->first);
        pop.target = 0; // Also retire bots if this zone loses its usable nodes.
        std::erase_if(pop.activeBots, [&](uint32 id) {
            Creature const* bot = BotDataMgr::FindBot(id);
            if (!bot) return true;
            // Retain entries while a teleport temporarily removes the creature.
            if (!occupied || (bot->IsInWorld() &&
                (bot->GetMapId() != pop.mapId || bot->GetZoneId() != pop.zoneId)))
            {
                BotDataMgr::DespawnWandererBot(id);
                return true;
            }
            return false;
        });
        if (!occupied) it = _populatedZones.erase(it);
        else ++it;
    }

    struct ZoneNodes
    {
        uint64 key;
        std::vector<WanderNode const*> nodes;
    };
    std::vector<ZoneNodes> candidates;
    for (auto const& [key, count] : active)
    {
        uint32 const map = uint32(key >> 32), zone = uint32(key);
        ZoneNodes entry{key, {}};
        std::vector<WanderNode const*> spawns;
        WanderNode::DoForAllZoneWPs(zone, [&](WanderNode const* wp) {
            auto const [min, max] = wp->GetLevels();
            if (wp->GetMapId() != map || wp->HasFlag(BotWPFlags::BOTWP_FLAG_NOT_A_START_POINT) ||
                wp->GetLinks().empty() || !SpawnLevels(min, max, 1)) return;
            entry.nodes.push_back(wp);
            if (wp->HasFlag(BotWPFlags::BOTWP_FLAG_SPAWN)) spawns.push_back(wp);
        });
        // Ordinary waypoints are safe fallbacks, excluding jump-only/no-start nodes.
        if (!spawns.empty()) entry.nodes = std::move(spawns);
        if (entry.nodes.empty()) continue;

        auto& pop = _populatedZones[key];
        pop.mapId = map;
        pop.zoneId = zone;
        pop.players = count;
        pop.minLevel = 80;
        pop.maxLevel = 1;
        for (auto* wp : entry.nodes)
        {
            auto const range = *SpawnLevels(wp->GetLevels().first, wp->GetLevels().second, 1);
            pop.minLevel = std::min(pop.minLevel, range.min);
            pop.maxLevel = std::max(pop.maxLevel, range.max);
        }
        if (pop.zoneName.empty())
        {
            auto const* area = sAreaTableStore.LookupEntry(zone);
            pop.zoneName = area && area->AreaName[0] ? area->AreaName[0] : "Zone " + std::to_string(zone);
        }
        candidates.push_back(std::move(entry));
    }

    // Apply reductions before additions, including configuration reloads to zero.
    for (std::size_t i = 0; i < candidates.size(); ++i)
    {
        auto& pop = _populatedZones.at(candidates[i].key);
        pop.target = ZoneTarget(i, candidates.size(), BotCfg::GetDynamicZoneBotsPerZone(), BotCfg::GetDynamicZoneMaxBots());
    }
    for (auto& [_, pop] : _populatedZones)
    {
        while (pop.activeBots.size() > pop.target)
        {
            BotDataMgr::DespawnWandererBot(pop.activeBots.back());
            pop.activeBots.pop_back();
        }
    }

    // Bound grid loads and equipment generation per scan; rotate between zones.
    uint32 budget = BotCfg::GetDynamicZoneSpawnBudget();
    auto start = std::upper_bound(candidates.begin(), candidates.end(), _lastSpawnZone,
        [](uint64 key, ZoneNodes const& zone) { return key < zone.key; });
    std::rotate(candidates.begin(), start, candidates.end());
    for (auto& entry : candidates)
    {
        if (!budget) break;
        auto& pop = _populatedZones.at(entry.key);
        if (pop.activeBots.size() >= pop.target) continue;
        _lastSpawnZone = entry.key;
        Bcore::Containers::RandomShuffle(entry.nodes);
        // At most one attempt per node, even if the template pool is exhausted.
        for (auto* wp : entry.nodes)
        {
            if (!budget || pop.activeBots.size() >= pop.target) break;
            --budget;
            if (uint32 id = BotDataMgr::SpawnDynamicZoneBot(0, wp))
                pop.activeBots.push_back(id); // 0 selects a class-compatible node level.
        }
    }
}

std::vector<ZonePopulation> DynamicZoneMgr::GetActiveZonesStatus() const
{
    std::vector<ZonePopulation> result;
    for (auto const& [_, pop] : _populatedZones) result.push_back(pop);
    return result;
}

void DynamicZoneMgr::Reset()
{
    for (auto const& [_, pop] : _populatedZones)
        for (uint32 id : pop.activeBots)
            if (BotDataMgr::FindBot(id)) BotDataMgr::DespawnWandererBot(id);
    _populatedZones.clear();
    _scanTimer = 0;
    _lastSpawnZone = 0;
}
}
