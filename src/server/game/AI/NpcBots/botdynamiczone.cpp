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
#include "MapManager.h"
#include "TemporarySummon.h"
#include "DBCStores.h"
#include "Containers.h"
#include "Config.h"

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

    std::map<uint64, std::vector<Player*>> active;
    for (auto const& [_, session] : sWorld->GetAllSessions())
    {
        Player* player = session ? session->GetPlayer() : nullptr;
        if (!player || !player->IsInWorld() || player->IsBeingTeleported() || player->IsInFlight() ||
            !player->FindMap() || !player->GetMap()->GetEntry()->IsContinent() || !player->GetZoneId())
            continue;
        if (player->GetMapId() == 571 && player->GetZoneId() == 4197 &&
            sConfigMgr->GetBoolDefault("Wintergrasp.BotWar.Enable", false)) continue;
        if ((player->GetZoneId() == 1519 || player->GetZoneId() == 1637) &&
            sConfigMgr->GetBoolDefault("CityRaid.Enable", true)) continue;
        active[ZoneKey(player->GetMapId(), player->GetZoneId())].push_back(player);
    }

    float const proximityRadius = sConfigMgr->GetFloatDefault("NpcBot.WanderingBots.DynamicZone.ProximityRadius", 400.0f);
    float const despawnRadius = sConfigMgr->GetFloatDefault("NpcBot.WanderingBots.DynamicZone.DespawnRadius", proximityRadius > 0.0f ? proximityRadius * 2.5f : 0.0f);

    // Queue removals through BotDataMgr: never delete map-owned creatures here.
    for (auto it = _populatedZones.begin(); it != _populatedZones.end();)
    {
        auto& pop = it->second;
        auto const itActive = active.find(it->first);
        bool const occupied = (itActive != active.end());
        pop.target = 0; // Also retire bots if this zone loses its usable nodes.

        Map* map = sMapMgr->FindBaseNonInstanceMap(pop.mapId);
        if (pop.darkKnightRespawnTimer > diff)
            pop.darkKnightRespawnTimer -= diff;
        else
            pop.darkKnightRespawnTimer = 0;

        uint32 const dkRespawnSecs = sConfigMgr->GetIntDefault("NpcBot.DarkKnight.RespawnTime", 300);
        std::erase_if(pop.darkKnights, [&](ObjectGuid guid) {
            if (!map) return true;
            Creature* dk = map->GetCreature(guid);
            if (!dk || !dk->IsInWorld() || !dk->IsAlive() || !occupied)
            {
                if (dk)
                {
                    if (!dk->IsAlive())
                        pop.darkKnightRespawnTimer = dkRespawnSecs * 1000;
                    dk->DespawnOrUnsummon();
                }
                return true;
            }
            return false;
        });

        uint32 const maxDk = sConfigMgr->GetIntDefault("NpcBot.DarkKnight.CountPerZone", 1);
        while (pop.darkKnights.size() > maxDk)
        {
            if (map)
            {
                if (Creature* dk = map->GetCreature(pop.darkKnights.back()))
                    dk->DespawnOrUnsummon();
            }
            pop.darkKnights.pop_back();
        }

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
            if (despawnRadius > 0.0f && occupied && bot->IsInWorld())
            {
                bool nearAnyPlayer = false;
                for (Player const* pl : itActive->second)
                {
                    if (pl->IsInDist2d(bot, despawnRadius))
                    {
                        nearAnyPlayer = true;
                        break;
                    }
                }
                if (!nearAnyPlayer)
                {
                    BotDataMgr::DespawnWandererBot(id);
                    return true;
                }
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
    for (auto const& [key, playersList] : active)
    {
        uint32 const map = uint32(key >> 32), zone = uint32(key);
        ZoneNodes entry{key, {}};
        std::vector<WanderNode const*> spawns;
        WanderNode::DoForAllZoneWPs(zone, [&](WanderNode const* wp) {
            auto const [min, max] = wp->GetLevels();
            if (wp->GetMapId() != map || wp->HasFlag(BotWPFlags::BOTWP_FLAG_NOT_A_START_POINT) ||
                wp->GetLinks().empty() || !SpawnLevels(min, max, 1)) return;

            if (proximityRadius > 0.0f)
            {
                bool nearPlayer = false;
                for (Player const* pl : playersList)
                {
                    if (pl->IsInDist2d(wp, proximityRadius))
                    {
                        nearPlayer = true;
                        break;
                    }
                }
                if (!nearPlayer)
                    return;
            }

            entry.nodes.push_back(wp);
            if (wp->HasFlag(BotWPFlags::BOTWP_FLAG_SPAWN)) spawns.push_back(wp);
        });

        // Fallback: If no waypoints were within proximityRadius, select closest to avoid empty zone
        if (entry.nodes.empty() && proximityRadius > 0.0f)
        {
            WanderNode const* closestWP = nullptr;
            float minD = 50000.0f;
            WanderNode::DoForAllZoneWPs(zone, [&](WanderNode const* wp) {
                auto const [min, max] = wp->GetLevels();
                if (wp->GetMapId() != map || wp->HasFlag(BotWPFlags::BOTWP_FLAG_NOT_A_START_POINT) ||
                    wp->GetLinks().empty() || !SpawnLevels(min, max, 1)) return;

                for (Player const* pl : playersList)
                {
                    float d = pl->GetExactDist2d(wp);
                    if (d < minD)
                    {
                        minD = d;
                        closestWP = wp;
                    }
                }
            });
            if (closestWP)
            {
                entry.nodes.push_back(closestWP);
                if (closestWP->HasFlag(BotWPFlags::BOTWP_FLAG_SPAWN))
                    spawns.push_back(closestWP);
            }
        }

        // Keep all eligible road/wander waypoints in entry.nodes so bots spawn naturally distributed
        if (entry.nodes.empty()) continue;

        auto& pop = _populatedZones[key];
        pop.mapId = map;
        pop.zoneId = zone;
        pop.players = uint32(playersList.size());
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

    // Track global Alliance and Horde population across all populated zones
    uint32 globalAllianceCount = 0;
    uint32 globalHordeCount = 0;
    for (auto const& [_, pop] : _populatedZones)
    {
        for (uint32 id : pop.activeBots)
        {
            if (Creature const* bot = BotDataMgr::FindBot(id))
            {
                uint32 team = BotDataMgr::GetTeamForFaction(bot->GetFaction());
                if (team == ALLIANCE)
                    ++globalAllianceCount;
                else if (team == HORDE)
                    ++globalHordeCount;
            }
        }
    }

    uint32 const maxGlobalBots = BotCfg::GetDynamicZoneMaxBots();
    uint32 const maxGlobalAlliance = (maxGlobalBots + 1) / 2;
    uint32 const maxGlobalHorde = (maxGlobalBots + 1) / 2;

    for (auto& entry : candidates)
    {
        if (!budget) break;
        auto& pop = _populatedZones.at(entry.key);
        if (pop.activeBots.size() >= pop.target) continue;
        _lastSpawnZone = entry.key;
        Bcore::Containers::RandomShuffle(entry.nodes);
        // Spawn up to budget across candidate nodes in this zone with strict 50/50 Alliance/Horde balance
        size_t nodeIdx = 0;
        size_t const numNodes = entry.nodes.size();
        uint32 consecutiveFailures = 0;

        uint32 allianceCount = 0;
        uint32 hordeCount = 0;
        for (uint32 id : pop.activeBots)
        {
            if (Creature const* bot = BotDataMgr::FindBot(id))
            {
                uint32 team = BotDataMgr::GetTeamForFaction(bot->GetFaction());
                if (team == ALLIANCE)
                    ++allianceCount;
                else if (team == HORDE)
                    ++hordeCount;
            }
        }

        uint32 const maxZoneAlliance = (pop.target + 1) / 2;
        uint32 const maxZoneHorde = (pop.target + 1) / 2;

        while (budget > 0 && pop.activeBots.size() < pop.target && numNodes > 0)
        {
            bool const canAlliance = (globalAllianceCount < maxGlobalAlliance) && (allianceCount < maxZoneAlliance);
            bool const canHorde = (globalHordeCount < maxGlobalHorde) && (hordeCount < maxZoneHorde);

            if (!canAlliance && !canHorde)
                break;

            int32 preferredTeam = -1;
            if (canAlliance && !canHorde)
                preferredTeam = ALLIANCE;
            else if (!canAlliance && canHorde)
                preferredTeam = HORDE;
            else if (allianceCount < hordeCount)
                preferredTeam = ALLIANCE;
            else if (hordeCount < allianceCount)
                preferredTeam = HORDE;
            else if (globalAllianceCount < globalHordeCount)
                preferredTeam = ALLIANCE;
            else if (globalHordeCount < globalAllianceCount)
                preferredTeam = HORDE;
            else
                preferredTeam = urand(0, 1) ? ALLIANCE : HORDE;

            WanderNode const* wp = entry.nodes[nodeIdx % numNodes];
            nodeIdx++;
            --budget;
            if (uint32 id = BotDataMgr::SpawnDynamicZoneBot(0, wp, preferredTeam))
            {
                pop.activeBots.push_back(id);
                if (Creature const* newBot = BotDataMgr::FindBot(id))
                {
                    uint32 team = BotDataMgr::GetTeamForFaction(newBot->GetFaction());
                    if (team == ALLIANCE)
                    {
                        ++allianceCount;
                        ++globalAllianceCount;
                    }
                    else if (team == HORDE)
                    {
                        ++hordeCount;
                        ++globalHordeCount;
                    }
                }
                consecutiveFailures = 0;
            }
            else
            {
                consecutiveFailures++;
                if (consecutiveFailures >= numNodes)
                    break;
            }
        }
    }

    // Spawn Dark Knight Nemesis bosses (2 per active zone)
    bool const enableDarkKnights = sConfigMgr->GetBoolDefault("NpcBot.DarkKnight.Enable", true);
    uint32 const darkKnightsPerZone = sConfigMgr->GetIntDefault("NpcBot.DarkKnight.CountPerZone", 1);

    if (enableDarkKnights && darkKnightsPerZone > 0)
    {
        for (auto const& [key, playersList] : active)
        {
            auto itPop = _populatedZones.find(key);
            if (itPop == _populatedZones.end()) continue;
            auto& pop = itPop->second;

            if (pop.darkKnightRespawnTimer > 0)
                continue;

            uint32 const mapId = pop.mapId;
            uint32 const zoneId = pop.zoneId;
            Map* map = sMapMgr->FindBaseNonInstanceMap(mapId);
            if (!map) continue;

            while (pop.darkKnights.size() < darkKnightsPerZone)
            {
                std::vector<WanderNode const*> zoneWps;
                WanderNode::DoForAllZoneWPs(zoneId, [&](WanderNode const* wp) {
                    if (wp->GetMapId() == mapId && !wp->GetLinks().empty())
                        zoneWps.push_back(wp);
                });
                if (zoneWps.empty())
                    break;

                // Prefer waypoints near player if available, else random zone waypoint
                WanderNode const* spawnLoc = nullptr;
                auto itCand = std::find_if(candidates.begin(), candidates.end(), [key](auto const& zn) { return zn.key == key; });
                if (itCand != candidates.end() && !itCand->nodes.empty())
                    spawnLoc = Trinity::Containers::SelectRandomContainerElement(itCand->nodes);
                else
                    spawnLoc = Trinity::Containers::SelectRandomContainerElement(zoneWps);

                if (!spawnLoc)
                    break;

                Position spawnPos = spawnLoc->GetPosition();
                map->LoadGrid(spawnPos.GetPositionX(), spawnPos.GetPositionY());
                TempSummon* dk = map->SummonCreature(70900, spawnPos, nullptr, 0, nullptr, 0, 0, ObjectGuid::Empty);
                if (dk)
                {
                    dk->setActive(true);
                    uint8 maxPlayerLvl = 1;
                    for (Player const* pl : playersList)
                    {
                        if (pl && pl->GetLevel() > maxPlayerLvl)
                            maxPlayerLvl = pl->GetLevel();
                    }
                    uint8 targetLvl = std::min<uint8>(pop.maxLevel + 2, maxPlayerLvl + 3);
                    uint8 dkLevel = std::min<uint8>(80, std::max<uint8>(pop.minLevel, targetLvl));
                    dk->SetLevel(dkLevel);
                    dk->UpdateLevelDependantStats();
                    dk->SetHealth(dk->GetMaxHealth());
                    pop.darkKnights.push_back(dk->GetGUID());
                }
                else
                    break;
            }
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
    {
        for (uint32 id : pop.activeBots)
            if (BotDataMgr::FindBot(id)) BotDataMgr::DespawnWandererBot(id);

        Map* map = sMapMgr->FindBaseNonInstanceMap(pop.mapId);
        for (ObjectGuid guid : pop.darkKnights)
            if (map)
                if (Creature* dk = map->GetCreature(guid))
                    dk->DespawnOrUnsummon();
    }
    _populatedZones.clear();
    _scanTimer = 0;
    _lastSpawnZone = 0;
}
}
