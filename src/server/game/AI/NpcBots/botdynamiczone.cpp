#include "botdynamiczone.h"
#include "botdynamiczonepolicy.h"
#include "botdatamgr.h"
#include "botconfig.h"
#include "botplayeraid.h"
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

#include <array>
#include <limits>
#include <unordered_set>

namespace NPCBots::DynamicZone
{
std::vector<uint8> GetSpawnRouteLevels(WanderNode const& node, uint32 spawnFaction, uint32 movementFaction)
{
    auto const [min, max] = node.GetLevels();
    auto const range = SpawnLevels(min, max, 1);
    if (!range || node.HasFlag(BotWPFlags::BOTWP_FLAG_NOT_A_START_POINT) ||
        !bot_ai::IsWanderNodeAvailableForBotFaction(&node, spawnFaction, false, true))
        return {};

    std::array<bool, DEFAULT_MAX_LEVEL + 1> reachable{};
    for (auto const& link : node.GetLinks())
    {
        if (!InScope(link.wp->GetMapId(), link.wp->GetZoneId(), node.GetMapId(), node.GetZoneId()) ||
            !bot_ai::IsWanderNodeAvailableForBotFaction(link.wp, movementFaction, false))
            continue;

        auto const [nextMin, nextMax] = link.wp->GetLevels();
        if (auto const overlap = TravelLevelOverlap(*range, nextMin, nextMax))
            std::fill(reachable.begin() + overlap->min, reachable.begin() + overlap->max + 1, true);
    }

    std::vector<uint8> levels;
    for (unsigned level = range->min; level <= range->max; ++level)
        if (reachable[level])
            levels.push_back(uint8(level));
    return levels;
}

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
    auto const scanElapsed = ScanElapsed(_scanTimer, diff, interval);
    if (!scanElapsed)
        return;
    uint32 const elapsed = *scanElapsed;

    struct Observer
    {
        Player const* player;
        WorldObject const* location;
    };
    std::map<uint64, std::vector<Observer>> active;
    bool const trackCorpses = BotCfg::GetHealingSettings().playerAid &&
        sConfigMgr->GetBoolDefault("NpcBot.Wanderer.PlayerAid.RescueEnable", true);
    for (auto const& [_, session] : sWorld->GetAllSessions())
    {
        Player* player = session ? session->GetPlayer() : nullptr;
        if (!player || !player->IsInWorld() || player->IsBeingTeleported() || player->IsInFlight() ||
            !player->FindMap() || !player->GetMap()->GetEntry()->IsContinent() || !player->GetZoneId())
            continue;
        // A released ghost can be far from the rescuers' destination.
        WorldObject const* location = player;
        uint32 zone = player->GetZoneId();
        if (trackCorpses && !player->IsResurrectRequested())
            if (WorldObject const* body = GetWandererRescueTarget(*player); body && body->IsCorpse())
            {
                if (uint32 const corpseZone = body->GetMap()->GetZoneId(body->GetPhaseMask(),
                    body->GetPositionX(), body->GetPositionY(), body->GetPositionZ()))
                {
                    location = body;
                    zone = corpseZone;
                }
            }

        if (location->GetMapId() == 571 && zone == 4197 &&
            sConfigMgr->GetBoolDefault("Wintergrasp.BotWar.Enable", false)) continue;
        if ((zone == 1519 || zone == 1637) && sConfigMgr->GetBoolDefault("CityRaid.Enable", true)) continue;
        active[ZoneKey(location->GetMapId(), zone)].push_back({player, location});
    }

    float const proximityRadius = NPCBots::Healing::FiniteClamp(sConfigMgr->GetFloatDefault("NpcBot.WanderingBots.DynamicZone.ProximityRadius", 400.0f), 400.f, 0.f, 5000.f);
    float const despawnRadius = NPCBots::Healing::FiniteClamp(sConfigMgr->GetFloatDefault("NpcBot.WanderingBots.DynamicZone.DespawnRadius", proximityRadius * 2.5f), proximityRadius * 2.5f, proximityRadius, 15000.f);

    float const spawnSeparation = NPCBots::Healing::FiniteClamp(
        sConfigMgr->GetFloatDefault("NpcBot.WanderingBots.DynamicZone.SpawnSpacingRadius", 50.f), 50.f, 0.f, 250.f);
    unsigned const nearbyLimit = Normalize(sConfigMgr->GetIntDefault(
        "NpcBot.WanderingBots.DynamicZone.MaxNearbyBots", 8), 0, 100);

    // Queue removals through BotDataMgr: never delete map-owned creatures here.
    for (auto it = _populatedZones.begin(); it != _populatedZones.end();)
    {
        auto& pop = it->second;
        auto const itActive = active.find(it->first);
        bool const occupied = (itActive != active.end());
        pop.target = 0; // Also retire bots if this zone loses its usable nodes.

        Map* map = sMapMgr->FindBaseNonInstanceMap(pop.mapId);
        if (pop.darkKnightRespawnTimer > elapsed)
            pop.darkKnightRespawnTimer -= elapsed;
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
                for (Observer const& observer : itActive->second)
                {
                    if (observer.location->IsInDist2d(bot, despawnRadius))
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

    struct SpawnCandidate
    {
        WanderNode const* node;
        float playerDistance;
        std::array<bool, 2> teamAllowed;
        LevelRange levels;
        unsigned nearbyBots = 0;
    };
    struct ZoneNodes
    {
        uint64 key;
        std::vector<SpawnCandidate> nodes;
    };
    std::vector<ZoneNodes> candidates;
    for (auto const& [key, playersList] : active)
    {
        uint32 const map = uint32(key >> 32), zone = uint32(key);
        ZoneNodes entry{key, {}};
        std::vector<SpawnCandidate> fallback;
        WanderNode::DoForAllZoneWPs(zone, [&](WanderNode const* wp) {
            if (wp->GetMapId() != map || wp->HasFlag(BotWPFlags::BOTWP_FLAG_NOT_A_START_POINT))
                return;

            float distance = std::numeric_limits<float>::max();
            for (Observer const& observer : playersList)
                distance = std::min(distance, observer.location->GetExactDist2d(wp));
            if (proximityRadius > 0.f && distance > despawnRadius)
                return;

            auto routeLevels = [&](uint32 faction) {
                uint32 const spawnFaction = sWorld->IsFFAPvPRealm() ? uint32(FACTION_TEMPLATE_NEUTRAL_HOSTILE) : faction;
                return GetSpawnRouteLevels(*wp, spawnFaction, faction);
            };
            auto const alliance = routeLevels(FACTION_TEMPLATE_ALLIANCE_DEFAULT);
            auto const horde = routeLevels(FACTION_TEMPLATE_HORDE_DEFAULT);
            if (alliance.empty() && horde.empty())
                return;
            LevelRange const levels{
                std::min(alliance.empty() ? uint8(DEFAULT_MAX_LEVEL) : alliance.front(), horde.empty() ? uint8(DEFAULT_MAX_LEVEL) : horde.front()),
                std::max(alliance.empty() ? uint8(0) : alliance.back(), horde.empty() ? uint8(0) : horde.back())};
            SpawnCandidate candidate{wp, distance, {!alliance.empty(), !horde.empty()}, levels};
            if (proximityRadius == 0.f || distance <= proximityRadius)
                entry.nodes.push_back(candidate);
            else
                fallback.push_back(candidate);
        });
        if (entry.nodes.empty())
            entry.nodes = std::move(fallback);
        if (entry.nodes.empty())
            continue;

        auto& pop = _populatedZones[key];
        pop.mapId = map;
        pop.zoneId = zone;
        pop.players = uint32(playersList.size());
        pop.minLevel = 80;
        pop.maxLevel = 1;
        for (auto const& candidate : entry.nodes)
        {
            pop.minLevel = std::min(pop.minLevel, candidate.levels.min);
            pop.maxLevel = std::max(pop.maxLevel, candidate.levels.max);
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
        std::array<std::unordered_set<WanderNode const*>, 2> failed;

        uint32 allianceCount = 0;
        uint32 hordeCount = 0;
        for (uint32 id : pop.activeBots)
        {
            if (Creature const* bot = BotDataMgr::FindBot(id))
            {
                if (bot->IsInWorld() && bot->GetMapId() == pop.mapId)
                    for (auto& candidate : entry.nodes)
                        candidate.nearbyBots += bot->IsInDist2d(candidate.node, spawnSeparation);
                uint32 team = BotDataMgr::GetTeamForFaction(bot->GetFaction());
                if (team == ALLIANCE)
                    ++allianceCount;
                else if (team == HORDE)
                    ++hordeCount;
            }
        }

        uint32 const maxZoneAlliance = (pop.target + 1) / 2;
        uint32 const maxZoneHorde = (pop.target + 1) / 2;

        auto pick = [&](std::size_t team) -> SpawnCandidate* {
            SpawnCandidate* best = nullptr;
            auto priority = [](SpawnCandidate const& candidate) {
                return SpawnPriority(candidate.nearbyBots, candidate.node->HasFlag(BotWPFlags::BOTWP_FLAG_SPAWN),
                    candidate.playerDistance);
            };
            for (auto& candidate : entry.nodes)
                if (candidate.teamAllowed[team] && !failed[team].contains(candidate.node) &&
                    (!spawnSeparation || !nearbyLimit || candidate.nearbyBots < nearbyLimit) &&
                    (!best || priority(candidate) < priority(*best)))
                    best = &candidate;
            return best;
        };

        while (budget && pop.activeBots.size() < pop.target)
        {
            SpawnCandidate* alliance = globalAllianceCount < maxGlobalAlliance && allianceCount < maxZoneAlliance ? pick(0) : nullptr;
            SpawnCandidate* horde = globalHordeCount < maxGlobalHorde && hordeCount < maxZoneHorde ? pick(1) : nullptr;
            if (!alliance && !horde)
                break;

            int32 preferredTeam = -1;
            if (alliance && !horde)
                preferredTeam = ALLIANCE;
            else if (!alliance && horde)
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

            WanderNode const* wp = (preferredTeam == ALLIANCE ? alliance : horde)->node;
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
                for (auto& candidate : entry.nodes)
                    candidate.nearbyBots += wp->GetExactDist2d(candidate.node) <= spawnSeparation;
            }
            else
                failed[preferredTeam == ALLIANCE ? 0 : 1].insert(wp);
        }
    }

    // Spawn Dark Knight Nemesis bosses
    bool const enableDarkKnights = sConfigMgr->GetBoolDefault("NpcBot.DarkKnight.Enable", false);
    uint32 const darkKnightsPerZone = enableDarkKnights ? sConfigMgr->GetIntDefault("NpcBot.DarkKnight.CountPerZone", 1) : 0;

    if (!enableDarkKnights || darkKnightsPerZone == 0)
    {
        for (auto& [key, pop] : _populatedZones)
        {
            if (!pop.darkKnights.empty())
            {
                Map* map = sMapMgr->FindBaseNonInstanceMap(pop.mapId);
                for (ObjectGuid guid : pop.darkKnights)
                {
                    if (map)
                        if (Creature* dk = map->GetCreature(guid))
                            dk->DespawnOrUnsummon();
                }
                pop.darkKnights.clear();
            }
        }
    }
    else
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
                    spawnLoc = Trinity::Containers::SelectRandomContainerElement(itCand->nodes).node;
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
                    for (Observer const& observer : playersList)
                        maxPlayerLvl = std::max(maxPlayerLvl, observer.player->GetLevel());
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
