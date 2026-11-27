/* NPCBots siege battleground tactics. Class AIs continue to own normal PvP combat. */
#include "bot_ai.h"
#include "botbgtactics.h"
#include "botmgr.h"
#include "botspell.h"
#include "botwanderful.h"
#include "BattlegroundSA.h"
#include "BattlegroundIC.h"
#include "CellImpl.h"
#include "GameObject.h"
#include "GridNotifiersImpl.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "PathGenerator.h"
#include "Spell.h"
#include "SpellHistory.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Transport.h"
#include "Vehicle.h"
#include "Map.h"
#include "MovementTypedefs.h"
#include <algorithm>
#include <limits>
#include <queue>
#include <unordered_map>

namespace
{
using namespace NPCBots::SiegeBG;

uint32 GateMask(Battleground* bg)
{
    uint32 mask = 0;
    for (uint8 gate = 0; gate < 6; ++gate)
        if ((bg->GetTypeID() == BATTLEGROUND_SA && static_cast<BattlegroundSA*>(bg)->GetGateState(gate) == BG_SA_GATE_DESTROYED)
            || (bg->GetTypeID() == BATTLEGROUND_IC && static_cast<BattlegroundIC*>(bg)->GetGateState(gate) == BG_IC_GATE_DESTROYED))
            mask |= 1u << gate;
    return mask;
}

bool HumanNear(Unit* vehicle, float radius)
{
    for (auto const& ref : vehicle->GetMap()->GetPlayers())
    {
        Player* player = ref.GetSource();
        if (player && player->IsAlive() && vehicle->IsFriendlyTo(player) && vehicle->IsWithinDistInMap(player, radius))
            return true;
    }
    return false;
}

bool HumanRequestsVehicle(Unit* vehicle)
{
    for (auto const& ref : vehicle->GetMap()->GetPlayers())
        if (Player* player = ref.GetSource())
            if (player->IsAlive() && vehicle->IsFriendlyTo(player) && player->InSamePhase(vehicle) &&
                player->GetTarget() == vehicle->GetGUID() && player->IsWithinDistInMap(vehicle, INTERACTION_DISTANCE))
                return true;
    return false;
}

bool ControlledBy(ICNodePoint const& node, TeamId team)
{
    return node.faction == team && node.nodeState == (team == TEAM_ALLIANCE ? NODE_STATE_CONTROLLED_A : NODE_STATE_CONTROLLED_H);
}

// Keep teleporters use invisible world triggers as their native landing points.
// Resolve the actual paired platform explicitly for autonomous infantry instead
// of relying on the generic GO spell's implicit target search and GO-centre LOS.
bool UseICKeepTeleporter(Creature* bot, Battleground* bg, GameObject* entrance, TeamId team)
{
    if (bg->GetTypeID() != BATTLEGROUND_IC || bg->GetStatus() != STATUS_IN_PROGRESS ||
        !bot->IsAlive() || bot->GetVehicle() || bot->GetTransport() || bot->IsInCombat() ||
        bot->HasUnitState(UNIT_STATE_CASTING | UNIT_STATE_CONTROLLED) ||
        !entrance->isSpawned() || entrance->HasFlag(GO_FLAG_NOT_SELECTABLE) ||
        !bot->InSamePhase(entrance) || !bot->IsWithinDistInMap(entrance, INTERACTION_DISTANCE))
        return false;

    bool const leaving = ICPositionKeep(bot->GetPositionX(), bot->GetPositionY()) == team;
    uint32 const entry = leaving ? (team == TEAM_ALLIANCE ? GO_TELEPORTER_4 : GO_TELEPORTER_2)
        : (team == TEAM_ALLIANCE ? GO_TELEPORTER_3 : GO_TELEPORTER_1);
    if (entrance->GetEntry() != entry ||
        (ICPositionKeep(entrance->GetPositionX(), entrance->GetPositionY()) == team) != leaving)
        return false;

    uint32 const exitEntry = leaving ? (team == TEAM_ALLIANCE ? GO_TELEPORTER_3 : GO_TELEPORTER_1)
        : (team == TEAM_ALLIANCE ? GO_TELEPORTER_4 : GO_TELEPORTER_2);
    GameObject* exit = nullptr;
    for (auto const& spawn : BG_IC_Teleporters)
        if (spawn.entry == exitEntry)
            if (GameObject* go = bg->GetBGObject(spawn.type, true))
                if (go->isSpawned() && !go->HasFlag(GO_FLAG_NOT_SELECTABLE) && bot->InSamePhase(go) &&
                    (ICPositionKeep(go->GetPositionX(), go->GetPositionY()) == team) != leaving &&
                    (!exit || entrance->GetExactDist(go) < entrance->GetExactDist(exit)))
                    exit = go;
    if (!exit)
        return false;

    std::list<Creature*> triggers;
    exit->GetCreatureListWithEntryInGrid(triggers, 22515, 10.f);
    Creature* landing = nullptr;
    for (Creature* trigger : triggers)
        if (trigger->IsAlive() && bot->InSamePhase(trigger) &&
            (ICPositionKeep(trigger->GetPositionX(), trigger->GetPositionY()) == team) != leaving &&
            (!landing || exit->GetExactDist(trigger) < exit->GetExactDist(landing)))
            landing = trigger;
    if (!landing)
        return false;

    bot->RemoveAurasByType(SPELL_AURA_MOUNTED);
    bot->GetMotionMaster()->Clear(MOTION_PRIORITY_NORMAL);
    bot->StopMoving();
    bot->CastSpell(bot, SPELL_TELEPORT_VISUAL_ONLY, true);
    bot->NearTeleportTo(landing->GetPosition());
    return true;
}

bool SAPathClear(Unit* mover, Position const& destination, BattlegroundSA* sa)
{
    PathGenerator path(mover);
    path.SetUseStraightPath(true);
    if (!path.CalculatePath(destination.GetPositionX(), destination.GetPositionY(), destination.GetPositionZ(), false) ||
        !VerifiedSiegePath(path.GetPathType(), path.GetPath().size()))
        return false;
    auto const& points = path.GetPath();
    for (std::size_t i = 1; i < points.size(); ++i)
        if (!sa->IsBotGateSegmentClear(points[i - 1], points[i], mover->GetBoundingRadius(), mover->GetPhaseMask()))
            return false;
    return true;
}

bool SADescentClear(Creature* bot, BattlegroundSA* sa, Position const& from, Position const& to)
{
    float amplitude = 7.5f * 7.5f / (2.f * Movement::gravity);
    G3D::Vector3 previous(from.GetPositionX(), from.GetPositionY(), from.GetPositionZ());
    for (unsigned sample = 1; sample <= 16; ++sample)
    {
        float t = float(sample) / 16.f;
        G3D::Vector3 next(from.GetPositionX() + (to.GetPositionX() - from.GetPositionX()) * t,
            from.GetPositionY() + (to.GetPositionY() - from.GetPositionY()) * t,
            from.GetPositionZ() + (to.GetPositionZ() - from.GetPositionZ()) * t + 4.f * amplitude * t * (1.f - t));
        if (!sa->IsBotGateSegmentClear(previous, next, bot->GetBoundingRadius(), bot->GetPhaseMask()) ||
            !bot->GetMap()->isInLineOfSight(previous.x, previous.y, previous.z + 1.f, next.x, next.y, next.z + 1.f,
                bot->GetPhaseMask(), LINEOFSIGHT_ALL_CHECKS, VMAP::ModelIgnoreFlags::Nothing))
            return false;
        previous = next;
    }
    return true;
}

bool SAPathFromClear(Creature* bot, BattlegroundSA* sa, Position const& from, Position const& destination)
{
    G3D::Vector3 start(from.GetPositionX(), from.GetPositionY(), from.GetPositionZ());
    G3D::Vector3 goal(destination.GetPositionX(), destination.GetPositionY(), destination.GetPositionZ());
    for (unsigned prefix = 0; prefix < 16; ++prefix)
    {
        PathGenerator path(bot);
        path.SetUseStraightPath(true);
        if (!path.CalculatePathFrom(start, goal, false) || !VerifiedSiegePath(path.GetPathType(), path.GetPath().size()))
            return false;
        auto const& points = path.GetPath();
        for (std::size_t i = 1; i < points.size(); ++i)
            if (!sa->IsBotGateSegmentClear(points[i - 1], points[i], bot->GetBoundingRadius(), bot->GetPhaseMask()))
                return false;
        if ((path.GetActualEndPosition() - goal).length() <= 7.f)
            return true;
        if (path.GetPathType() != PATHFIND_INCOMPLETE || (path.GetActualEndPosition() - start).length() < 1.f)
            return false;
        start = path.GetActualEndPosition();
    }
    return false;
}

// Native wall stairs are navigable, but navmeshes have no downward jump link.
// A defender can descend from a cannon platform, as a player can, before using
// the next gate's real portal. Never leap from the ground over a closed gate.
bool TrySADefenderDescent(Creature* bot, BattlegroundSA* sa, Position const& destination)
{
    if (bot->HasUnitState(UNIT_STATE_JUMPING))
        return true;
    Position const* platform = nullptr;
    Position landing;
    float bestScore = std::numeric_limits<float>::max();
    for (unsigned gun = 0; gun < 10; ++gun)
    {
        Position const& from = BG_SA_NpcSpawnlocs[gun];
        if (!SAPathClear(bot, from, sa))
            continue;
        for (unsigned direction = 0; direction < 8; ++direction)
        {
            float angle = float(direction) * float(M_PI) / 4.f;
            Position to(from.GetPositionX() + std::cos(angle) * 12.f, from.GetPositionY() + std::sin(angle) * 12.f,
                from.GetPositionZ() - 12.f, from.GetOrientation());
            float ground = bot->GetMap()->GetHeight(bot->GetPhaseMask(), to.GetPositionX(), to.GetPositionY(),
                from.GetPositionZ() + 2.f, true, 30.f);
            if (ground <= INVALID_HEIGHT || from.GetPositionZ() - ground < 3.f || from.GetPositionZ() - ground > 20.f)
                continue;
            to.m_positionZ = ground;
            if (!SADescentClear(bot, sa, from, to))
                continue;
            if (!SAPathFromClear(bot, sa, to, destination))
                continue;
            float score = bot->GetExactDist(&from) + to.GetExactDist(&destination);
            if (score < bestScore)
            {
                bestScore = score;
                platform = &from;
                landing = to;
            }
        }
    }
    if (!platform)
        return false;
    if (bot->GetExactDist(platform) > 1.5f)
    {
        Position current;
        if (!bot->isMoving() || !bot->GetMotionMaster()->GetDestination(current.m_positionX, current.m_positionY, current.m_positionZ) ||
            current.GetExactDist(platform) >= 3.f)
            bot->GetMotionMaster()->MovePoint(607628, *platform, true);
    }
    else
    {
        if (!SADescentClear(bot, sa, bot->GetPosition(), landing))
            return false;
        bot->RemoveAurasByType(SPELL_AURA_MOUNTED);
        bot->GetMotionMaster()->MoveJump(landing, 7.f, 7.5f, 607631);
    }
    return true;
}

// Defenders must obey the same physical gates as attackers. Prefer a real
// navigable approach; otherwise reach a native defender platform and use its
// world-trigger landing. A destroyed gate needs no portal detour.
bool MoveSADefender(Creature* bot, BattlegroundSA* sa, Position const& destination)
{
    if (sa->GetStatus() != STATUS_IN_PROGRESS || sa->GetAttackersTeam() == sa->GetBotTeamId(bot->GetGUID()) ||
        !bot->IsAlive() || bot->GetVehicle() || bot->GetTransport() || bot->IsInCombat() ||
        bot->HasUnitState(UNIT_STATE_CASTING | UNIT_STATE_CONTROLLED))
        return false;
    auto move = [&](Position const& point)
    {
        Position current;
        if (bot->isMoving() && bot->GetMotionMaster()->GetDestination(current.m_positionX, current.m_positionY, current.m_positionZ) &&
            current.GetExactDist(&point) < 3.f)
            return;
        bot->GetMotionMaster()->MovePoint(607628, point, true);
    };
    if (SAPathFromClear(bot, sa, bot->GetPosition(), destination))
    {
        move(destination);
        return false;
    }
    if (TrySADefenderDescent(bot, sa, destination))
        return false;
    GameObject* platform = nullptr;
    Creature* landing = nullptr;
    float bestScore = std::numeric_limits<float>::max();
    for (uint32 type = BG_SA_PORTAL_DEFFENDER_BLUE; type <= BG_SA_PORTAL_DEFFENDER_RED; ++type)
        if (GameObject* go = sa->GetBGObject(type, true))
        {
            if (!go->isSpawned() || go->HasFlag(GO_FLAG_NOT_SELECTABLE) || !bot->InSamePhase(go) ||
                !SAPathClear(bot, go->GetPosition(), sa))
                continue;
            std::list<Creature*> triggers;
            go->GetCreatureListWithEntryInGrid(triggers, NPC_WORLD_TRIGGER_LARGE_AOI_NOT_IMMUNE_PC_NPC, 60.f);
            Creature* target = nullptr;
            for (Creature* trigger : triggers)
                if (trigger->IsAlive() && bot->InSamePhase(trigger) &&
                    (!target || go->GetExactDist(trigger) < go->GetExactDist(target)))
                    target = trigger;
            if (!target || target->GetExactDist(&destination) + 10.f >= bot->GetExactDist(&destination))
                continue;
            float score = target->GetExactDist(&destination) + bot->GetExactDist(go) * 0.2f;
            if (score < bestScore)
            {
                bestScore = score;
                platform = go;
                landing = target;
            }
        }
    if (!platform)
    {
        if (bot->isMoving())
            bot->StopMoving();
        return false;
    }
    if (!bot->IsWithinDistInMap(platform, INTERACTION_DISTANCE))
    {
        move(platform->GetPosition());
        return false;
    }
    if (bot->HasAura(54643))
        return false; // Native teleporter lockout; no repeated transfers.
    bot->RemoveAurasByType(SPELL_AURA_MOUNTED);
    bot->GetMotionMaster()->Clear(MOTION_PRIORITY_NORMAL);
    bot->StopMoving();
    bot->CastSpell(bot, SPELL_TELEPORT_VISUAL_ONLY, true);
    bot->NearTeleportTo(landing->GetPosition());
    bot->AddAura(54643, bot); // Apply the native lockout without a second teleport cast.
    return true;
}

Position ICExteriorApproach(Unit* mover, GameObject const* gate, float distance)
{
    Position point = gate->GetPosition();
    uint8 door;
    switch (gate->GetEntry())
    {
        case GO_HORDE_GATE_1: door = BG_IC_H_FRONT; break;
        case GO_HORDE_GATE_2: door = BG_IC_H_WEST; break;
        case GO_HORDE_GATE_3: door = BG_IC_H_EAST; break;
        case GO_ALLIANCE_GATE_3: door = BG_IC_A_FRONT; break;
        case GO_ALLIANCE_GATE_1: door = BG_IC_A_WEST; break;
        case GO_ALLIANCE_GATE_2: door = BG_IC_A_EAST; break;
        default: return point;
    }
    OffsetOutsideICGate(door, distance, point.m_positionX, point.m_positionY);
    mover->UpdateAllowedPositionZ(point.m_positionX, point.m_positionY, point.m_positionZ);
    return point;
}

bool ICLocalPathClear(Creature* bot, Position const& destination, uint32 gates, float tolerance)
{
    PathGenerator path(bot);
    path.SetUseStraightPath(true);
    if (!path.CalculatePath(destination.GetPositionX(), destination.GetPositionY(), destination.GetPositionZ()) ||
        !VerifiedSiegePath(path.GetPathType(), path.GetPath().size()))
        return false;
    auto const& points = path.GetPath();
    G3D::Vector3 const target(destination.GetPositionX(), destination.GetPositionY(), destination.GetPositionZ());
    if ((path.GetActualEndPosition() - target).length() > tolerance)
        return false;
    for (std::size_t i = 1; i < points.size(); ++i)
        if (!ICSegmentOpen(points[i - 1].x, points[i - 1].y, points[i].x, points[i].y, gates))
            return false;
    return true;
}

bool ICBoardingPathClear(Creature* bot, Creature* vehicle, uint32 gates)
{
    if (bot->IsWithinDistInMap(vehicle, INTERACTION_DISTANCE))
        return bot->IsWithinLOSInMap(vehicle) && ICSegmentOpen(bot->GetPositionX(), bot->GetPositionY(),
            vehicle->GetPositionX(), vehicle->GetPositionY(), gates);
    return ICLocalPathClear(bot, vehicle->GetPosition(), gates, INTERACTION_DISTANCE);
}

float GateHealthFraction(GameObject const* gate)
{
    if (!gate || !gate->IsDestructibleBuilding())
        return 1.f;
    auto const& building = gate->GetGOValue()->Building;
    return SiegeGateHealthFraction(building.Health, building.MaxHealth);
}

bool IsGroundSiegeVehicle(Creature const* vehicle)
{
    switch (vehicle->GetEntry())
    {
        case NPC_DEMOLISHER_SA:
        case NPC_DEMOLISHER:
        case NPC_SIEGE_ENGINE_A:
        case NPC_SIEGE_ENGINE_H:
        case NPC_GLAIVE_THROWER_A:
        case NPC_GLAIVE_THROWER_H:
        case NPC_CATAPULT:
            return true;
        default:
            return false;
    }
}

// Give an escort its own squad's driver when nearby. An allied player in
// control of a machine and other squads' drivers are valid fallback convoys.
Creature* FindSiegeEscortVehicle(Creature* bot, Battleground* bg, TeamId team, uint32 gates)
{
    Creature* escort = nullptr;
    float bestScore = std::numeric_limits<float>::max();
    auto consider = [&](Unit* driver)
    {
        if (!driver || !driver->IsAlive() || !bot->InSamePhase(driver))
            return;
        Creature* vehicle = driver->GetVehicleCreatureBase();
        if (!vehicle || !vehicle->IsAlive() || vehicle->GetTransport() || vehicle->GetHealthPct() < 15.f ||
            !IsGroundSiegeVehicle(vehicle) || vehicle->GetCharmerGUID() != driver->GetGUID() ||
            !bot->IsWithinDistInMap(vehicle, 65.f))
            return;

        if (bg->GetTypeID() == BATTLEGROUND_IC &&
            !ICSegmentOpen(bot->GetPositionX(), bot->GetPositionY(), vehicle->GetPositionX(), vehicle->GetPositionY(), gates))
            return;

        float score = bot->GetExactDist(vehicle);
        if (driver->IsNPCBot() && driver->GetGUID().GetCounter() / 5 == bot->GetGUID().GetCounter() / 5)
            score -= 20.f;
        if (!escort || score < bestScore || (score == bestScore && vehicle->GetGUID() < escort->GetGUID()))
        {
            escort = vehicle;
            bestScore = score;
        }
    };

    for (auto const& [guid, participant] : bg->GetBots())
        if (bg->GetBotTeamId(guid) == team)
            consider(bot->GetMap()->GetCreature(guid));
    for (auto const& ref : bot->GetMap()->GetPlayers())
        if (Player* player = ref.GetSource(); player && player->GetBGTeam() == (team == TEAM_ALLIANCE ? ALLIANCE : HORDE))
            consider(player);

    return escort;
}

Position SiegeEscortPosition(Creature const* bot, Creature* vehicle, bool healer, bool tank)
{
    bool const left = bot->GetGUID().GetCounter() % 2 == 0;
    Position const& center = vehicle->GetPosition();
    float const angle = center.GetOrientation();
    float const depth = healer ? -14.f : tank ? 9.f : -4.f;
    float const flank = (left ? -1.f : 1.f) * (healer ? 5.f : 10.f);
    Position point = center;
    point.m_positionX += std::cos(angle) * depth - std::sin(angle) * flank;
    point.m_positionY += std::sin(angle) * depth + std::cos(angle) * flank;
    float const height = vehicle->GetMap()->GetHeight(vehicle->GetPhaseMask(), point.GetPositionX(), point.GetPositionY(),
        center.GetPositionZ() + 3.f, true, 9.f);
    if (height <= INVALID_HEIGHT || std::fabs(height - center.GetPositionZ()) > 5.f)
        return center;
    point.m_positionZ = height;
    auto const liquid = vehicle->GetMap()->GetLiquidStatus(vehicle->GetPhaseMask(), point.GetPositionX(), point.GetPositionY(),
        point.GetPositionZ(), {}, nullptr, bot->GetCollisionHeight());
    return liquid & (LIQUID_MAP_IN_WATER | LIQUID_MAP_UNDER_WATER) ? center : point;
}

uint32 VehicleSpell(Creature* vehicle, std::initializer_list<uint32> candidates, bool missileFallback = true)
{
    for (uint32 candidate : candidates)
        for (uint32 spell : vehicle->m_spells)
            if (spell == candidate)
                return spell;
    if (!missileFallback)
        return 0;
    for (uint32 spell : vehicle->m_spells)
        if (SpellInfo const* info = sSpellMgr->GetSpellInfo(spell))
            if (info->HasEffect(SPELL_EFFECT_TRIGGER_MISSILE))
                return spell;
    return 0;
}

GameObject* SelectSAAttackObjective(Creature* bot, BattlegroundSA* sa)
{
    TeamId team = sa->GetBotTeamId(bot->GetGUID());
    if (team != sa->GetAttackersTeam())
        return nullptr;
    if (GameObject* relic = sa->GetBGObject(BG_SA_TITAN_RELIC, true))
        if (relic->isSpawned() && bot->InSamePhase(relic) && !relic->HasFlag(GO_FLAG_NOT_SELECTABLE) &&
            sa->CanInteractWithObject(BG_SA_TITAN_RELIC))
            return relic; // Victory has priority over every outstanding graveyard.

    if (bot->GetVehicle())
        return nullptr; // Attacking vehicles assault gates; only infantry captures flags.
    bool scout = SACaptureScout(bot->GetGUID().GetCounter());
    GameObject* closest = nullptr;
    for (auto [type, gy] : { std::pair{BG_SA_LEFT_FLAG, BG_SA_LEFT_CAPTURABLE_GY},
        std::pair{BG_SA_RIGHT_FLAG, BG_SA_RIGHT_CAPTURABLE_GY}, std::pair{BG_SA_CENTRAL_FLAG, BG_SA_CENTRAL_CAPTURABLE_GY} })
        if (GameObject* flag = sa->GetBGObject(type, true))
            if (flag->isSpawned() && bot->InSamePhase(flag) && !flag->HasFlag(GO_FLAG_NOT_SELECTABLE) &&
                sa->CanInteractWithObject(type) && sa->GetGraveyardStatus(gy) != team &&
                (scout || (!bot->GetVehicle() && bot->IsWithinDistInMap(flag, 20.f))) &&
                (!closest || bot->GetExactDist(flag) < bot->GetExactDist(closest)))
                closest = flag;
    return closest;
}
}

bool bot_ai::IsSiegeBattlegroundBot() const
{
    Battleground* bg = GetBG();
    return IsWanderer() && bg && (bg->GetTypeID() == BATTLEGROUND_SA || bg->GetTypeID() == BATTLEGROUND_IC);
}

void bot_ai::ResetSiegeBattlegroundState()
{
    _siegeThinkTimer = _siegeWeaponTimer = _siegeBoardTimer = _siegeMountTimer = _siegeBombTimer = _siegeTravelNode = 0;
    _siegeSARole = 2;
    _siegeBombCarrier = false;
    RemoveBotCommandState(BOT_COMMAND_STAY | BOT_COMMAND_FOLLOW | BOT_COMMAND_ATTACK);
    me->RemoveAurasDueToSpell(52418);
    me->RemoveAurasDueToSpell(66271);
    me->RemoveAurasDueToSpell(68377);
}

// Dijkstra over the actual wandering-node mesh, with closed gate edges removed.
// No direct-movement fallback is used when a mesh route is unavailable.
bool bot_ai::MoveToSiegeObjective(Position const& destination, float radius, uint32 gates, bool attacker)
{
    Battleground* bg = GetBG();
    Unit* mover = me->GetVehicle() && CanBotMoveVehicle() ? me->GetVehicleBase() : me;
    if (mover == me && mover->GetExactDist(&destination) < 35.f)
        _siegeMountTimer = 2500;
    if (mover->GetExactDist(&destination) <= radius)
    {
        mover->StopMoving();
        return true;
    }
    if (mover->HasUnitState(UNIT_STATE_CASTING | UNIT_STATE_CONTROLLED) || (mover == me && me->IsInCombat()))
        return false;
    TeamId team = bg->GetBotTeamId(me->GetGUID());
    bool const icMap = bg->GetTypeID() == BATTLEGROUND_IC;
    uint8 const originKeep = ICPositionKeep(mover->GetPositionX(), mover->GetPositionY());
    uint8 const destinationKeep = ICPositionKeep(destination.GetPositionX(), destination.GetPositionY());
    // A friendly gate is still a solid gate. Infantry uses the real keep
    // teleporter when no friendly breach is available; vehicles must wait.
    if (icMap && mover == me && !me->GetTransport() &&
        ((originKeep == team) != (destinationKeep == team)) && !(gates & (team == TEAM_ALLIANCE ? 56u : 7u)))
    {
        uint32 const portalEntry = originKeep == team
            ? (team == TEAM_ALLIANCE ? GO_TELEPORTER_4 : GO_TELEPORTER_2)
            : (team == TEAM_ALLIANCE ? GO_TELEPORTER_3 : GO_TELEPORTER_1);
        GameObject* portal = nullptr;
        for (auto const& spawn : BG_IC_Teleporters)
            if (spawn.entry == portalEntry)
                if (GameObject* go = bg->GetBGObject(spawn.type, true))
                    if (go->isSpawned() && !go->HasFlag(GO_FLAG_NOT_SELECTABLE) && me->InSamePhase(go) &&
                        ICPositionKeep(go->GetPositionX(), go->GetPositionY()) == originKeep &&
                        (!portal || me->GetExactDist(go) < me->GetExactDist(portal))) portal = go;
        if (!portal) return false;
        if (me->IsWithinDistInMap(portal, INTERACTION_DISTANCE))
        {
            if (UseICKeepTeleporter(me, bg, portal, team))
            {
                _siegeTravelNode = 0;
                _siegeThinkTimer = 2000;
            }
        }
        else
            mover->GetMotionMaster()->MovePoint(607628, portal->GetPosition(), true);
        return false;
    }
    if (bg->GetTypeID() == BATTLEGROUND_SA && !attacker && mover == me && !me->GetVehicle() && !me->GetTransport())
    {
        _siegeTravelNode = 0;
        if (MoveSADefender(me, static_cast<BattlegroundSA*>(bg), destination))
            _siegeThinkTimer = 2000;
        return false;
    }
    auto allowed = [&](WanderNode const* node) {
        if (bg->GetTypeID() == BATTLEGROUND_SA)
            return node->GetWPId() >= 5200 && node->GetWPId() <= 5226
                && (!attacker || SANodeAccessible(node->GetWPId(), gates));
        if (node->GetWPId() < 5300 || node->GetWPId() > 5322)
            return false;
        uint8 const gate = ICCheckpointGate(node->GetWPId());
        return gate == 6 || Broken(gates, gate);
    };
    auto edgeOpen = [&](WanderNode const* from, WanderNode const* to) {
        return bg->GetTypeID() == BATTLEGROUND_SA ? SAEdgeOpen(from->GetWPId(), to->GetWPId(), gates)
            : ICEdgeOpen(from->GetWPId(), to->GetWPId(), uint8(team), gates);
    };
    WanderNode const* start = nullptr;
    WanderNode const* finish = nullptr;
    float startDistance = std::numeric_limits<float>::max(), finishDistance = startDistance;
    WanderNode::DoForAllMapWPs(me->GetMapId(), [&](WanderNode const* node) {
        if (!allowed(node))
            return;
        float distance = mover->GetExactDistSq(node);
        if (distance < startDistance && (!icMap || (ICNodeKeep(node->GetWPId()) == originKeep &&
            ICSegmentOpen(mover->GetPositionX(), mover->GetPositionY(), node->GetPositionX(), node->GetPositionY(), gates))))
        {
            startDistance = distance;
            start = node;
        }
        distance = destination.GetExactDistSq(node);
        if (distance < finishDistance && (!icMap || (ICNodeKeep(node->GetWPId()) == destinationKeep &&
            ICSegmentOpen(destination.GetPositionX(), destination.GetPositionY(), node->GetPositionX(), node->GetPositionY(), gates))))
        {
            finishDistance = distance;
            finish = node;
        }
    });
    if (!start || !finish)
        return false;
    WanderNode const* pendingCheckpoint = nullptr;
    if (_siegeTravelNode)
        if (WanderNode const* current = WanderNode::FindInMapWPs(me->GetMapId(), [this](WanderNode const* node) {
            return node->GetWPId() == _siegeTravelNode;
        }))
            if (allowed(current) &&
                (!icMap || ICSegmentOpen(mover->GetPositionX(), mover->GetPositionY(),
                    current->GetPositionX(), current->GetPositionY(), gates)))
            {
                float distance = mover->GetExactDist(current);
                bool const gateCheckpoint = icMap && ICCheckpointGate(current->GetWPId()) != 6;
                if (gateCheckpoint ? distance > 3.f : KeepSiegeCheckpoint(distance))
                    pendingCheckpoint = current;
                else
                    start = current;
            }

    Position point = destination;
    if (pendingCheckpoint)
        point = *pendingCheckpoint;
    else if (start != finish)
    {
        using Step = std::pair<float, WanderNode const*>;
        std::priority_queue<Step, std::vector<Step>, std::greater<Step>> pending;
        std::unordered_map<WanderNode const*, float> cost;
        std::unordered_map<WanderNode const*, WanderNode const*> previous;
        cost[start] = 0.f;
        pending.emplace(0.f, start);
        while (!pending.empty())
        {
            auto [distance, node] = pending.top();
            pending.pop();
            if (node == finish)
                break;
            if (distance > cost[node])
                continue;
            for (auto const& link : node->GetLinks())
            {
                WanderNode const* next = link.wp;
                if (!allowed(next) || !edgeOpen(node, next))
                    continue;
                float nextCost = distance + node->GetExactDist(next);
                auto old = cost.find(next);
                if (old == cost.end() || nextCost < old->second)
                {
                    cost[next] = nextCost;
                    previous[next] = node;
                    pending.emplace(nextCost, next);
                }
            }
        }
        if (!cost.contains(finish))
            return false;
        WanderNode const* step = finish;
        while (previous.contains(step) && previous[step] != start)
            step = previous[step];
        point = *step;
        _siegeTravelNode = step->GetWPId();
        // Reach the current road checkpoint before selecting an outgoing edge.
        if (mover->GetExactDist(start) > (icMap && ICCheckpointGate(start->GetWPId()) != 6 ? 3.f : 18.f))
        {
            point = *start;
            _siegeTravelNode = start->GetWPId();
        }
    }
    else
        _siegeTravelNode = 0; // The graph endpoint is reached; finish the local approach.
    if (mover == me && !me->GetTransport())
    {
        // Stable small offsets spread squads without moving the interaction point.
        if (point.GetExactDist(&destination) > 10.f && !(icMap && ICCheckpointGate(_siegeTravelNode) != 6))
        {
            float offset = float(int32(me->GetGUID().GetCounter() % 5) - 2) * 1.5f;
            point.m_positionY += offset;
            me->UpdateAllowedPositionZ(point.m_positionX, point.m_positionY, point.m_positionZ);
        }
    }
    Position currentDestination;
    if (mover->isMoving() && mover->GetMotionMaster()->GetDestination(currentDestination.m_positionX,
        currentDestination.m_positionY, currentDestination.m_positionZ) && currentDestination.GetExactDist(&point) < 3.f)
        return false;
    if (me->IsMounted() && mover->GetExactDist(&destination) < 25.f)
        DismountBot();
    mover->GetMotionMaster()->MovePoint(607628, point, true);
    return false;
}

bool bot_ai::TrySiegeVehicle(bool defender, uint8 squad)
{
    if (_siegeBoardTimer || me->GetVehicle() || me->IsInCombat() || IsCasting() || HasRole(BOT_ROLE_HEAL) || _siegeBombCarrier)
        return false;
    uint32 member = me->GetGUID().GetCounter() % 5;
    bool sa = GetBG()->GetTypeID() == BATTLEGROUND_SA;
    uint32 const gates = sa ? 0 : GateMask(GetBG());
    if (!defender && member == 4)
        return false; // Keep a capture runner in each five-bot squad on foot.
    if (defender && (sa ? !SADefenderGunner(me->GetGUID().GetCounter()) : member != 1))
        return false;
    std::vector<uint32> entries;
    if (sa)
        entries = defender ? std::vector<uint32>{ NPC_ANTI_PERSONNAL_CANNON } : std::vector<uint32>{ NPC_DEMOLISHER_SA };
    else if (defender)
        entries = { NPC_KEEP_CANNON };
    else if (squad == 1 || squad == 6)
        entries = { NPC_GLAIVE_THROWER_A, NPC_GLAIVE_THROWER_H, NPC_CATAPULT, NPC_DEMOLISHER, NPC_SIEGE_ENGINE_A, NPC_SIEGE_ENGINE_H };
    else if (squad == 0 || squad == 5 || squad == 7)
        entries = { NPC_DEMOLISHER, NPC_SIEGE_ENGINE_A, NPC_SIEGE_ENGINE_H, NPC_GLAIVE_THROWER_A, NPC_GLAIVE_THROWER_H, NPC_CATAPULT };
    else
        entries = { NPC_DEMOLISHER, NPC_SIEGE_ENGINE_A, NPC_SIEGE_ENGINE_H, NPC_GLAIVE_THROWER_A, NPC_GLAIVE_THROWER_H, NPC_CATAPULT };

    Creature* best = nullptr;
    int8 bestSeat = 0;
    float maxSearchDist = sa && !defender ? 300.f :
        (squad == 0 || squad == 1 || squad == 5 || squad == 6 || squad == 7 || sa) ? 140.f : 45.f;

    for (uint32 entry : entries)
    {
        std::list<Creature*> vehicles;
        me->GetCreatureListWithEntryInGrid(vehicles, entry, maxSearchDist);
        for (Creature* vehicle : vehicles)
        {
            Vehicle* kit = vehicle->GetVehicleKit();
            if (!vehicle->IsAlive() || vehicle->GetHealthPct() < 15.f || !kit || vehicle->HasUnitFlag(UNIT_FLAG_UNINTERACTIBLE)
                || (sa ? vehicle->IsHostileTo(me) : !me->InSamePhase(vehicle) || !vehicle->IsFriendlyTo(me))
                || HumanNear(vehicle, 10.f))
                continue;
            if (sa && defender)
            {
                auto* battleground = static_cast<BattlegroundSA*>(GetBG());
                Gate gate = AssaultGate(GateMask(battleground), (me->GetGUID().GetCounter() / 5) % 2 == 0);
                uint8 cannon = SACannonIndex(gate, me->GetGUID().GetCounter());
                if (cannon >= 10 || (vehicle != battleground->GetBGCreature(cannon, true) &&
                    vehicle != battleground->GetBGCreature(cannon ^ 1, true)))
                    continue;
            }
            if (sa && !me->IsWithinDistInMap(vehicle, INTERACTION_DISTANCE) &&
                !SAPathFromClear(me, static_cast<BattlegroundSA*>(GetBG()), me->GetPosition(), vehicle->GetPosition()))
                continue;

            if (!sa && !ICBoardingPathClear(me, vehicle, gates))
                continue;

            if (kit->HasEmptySeat(0))
            {
                // Unpiloted vehicle: any qualified non-healer bot can claim seat 0 as driver!
                if (defender && std::fabs(vehicle->GetPositionZ() - me->GetPositionZ()) > 8.f)
                    continue;
                if (!best || bestSeat != 0 || me->GetExactDist(vehicle) < me->GetExactDist(best))
                {
                    best = vehicle;
                    bestSeat = 0;
                }
            }
            else if (!defender && !best)
            {
                // Seat 0 already occupied. Check for gunner/turret passenger seat!
                for (auto const& seat : kit->Seats)
                {
                    if (seat.first <= 0) continue;
                    // A turret already occupies its parent seat, so inspect it
                    // before testing whether that parent seat is empty.
                    if (Unit* passenger = kit->GetPassenger(seat.first))
                    {
                        Creature* turret = passenger->ToCreature();
                        if (turret && turret->GetVehicleKit() && turret->GetVehicleKit()->HasEmptySeat(0) && turret->m_spells[0])
                        {
                            best = turret;
                            bestSeat = 0;
                            break;
                        }
                        continue;
                    }
                    if (kit->HasEmptySeat(seat.first) && seat.second.SeatInfo->CanEnterOrExit())
                    {
                        best = vehicle;
                        bestSeat = seat.first;
                        break;
                    }
                }
            }
        }
    }
    if (!best)
        return false;
    if (!me->IsWithinDistInMap(best, INTERACTION_DISTANCE))
    {
        _siegeMountTimer = 2500;
        Position current;
        if (!me->isMoving() || !me->GetMotionMaster()->GetDestination(current.m_positionX, current.m_positionY, current.m_positionZ) ||
            current.GetExactDist(best) >= 3.f)
            me->GetMotionMaster()->MovePoint(607629, best->GetPosition(), true);
        return true;
    }
    DismountBot();
    removeShapeshiftForm();
    me->CombatStop(true);
    me->EnterVehicle(best, bestSeat);
    // VehicleJoinEvent attaches the passenger on a later update, not here.
    _siegeBoardTimer = 2000;
    return true;
}

void bot_ai::UpdateSiegeVehicle(uint32 /*diff*/, GameObject* building, Position const& destination, uint32 gates, bool attacker)
{
    Creature* vehicle = me->GetVehicleCreatureBase();
    if (!vehicle)
        return;
    if (vehicle->GetHealthPct() < 15.0f || HumanRequestsVehicle(vehicle))
    {
        vehicle->StopMoving();
        me->ExitVehicle();
        _siegeBoardTimer = 20000;
        return;
    }
    bool const canMove = CanBotMoveVehicle();
    bool const icMap = GetBG()->GetTypeID() == BATTLEGROUND_IC;
    bool cannon = vehicle->GetEntry() == NPC_ANTI_PERSONNAL_CANNON || vehicle->GetEntry() == NPC_KEEP_CANNON
        || vehicle->GetEntry() == NPC_HORDE_GUNSHIP_CANNON || vehicle->GetEntry() == NPC_ALLIANCE_GUNSHIP_CANNON
        || vehicle->GetVehicleBase();
    bool catapult = vehicle->GetEntry() == NPC_CATAPULT;
    bool engine = vehicle->GetEntry() == NPC_SIEGE_ENGINE_A || vehicle->GetEntry() == NPC_SIEGE_ENGINE_H;
    bool demolisher = vehicle->GetEntry() == NPC_DEMOLISHER || vehicle->GetEntry() == NPC_DEMOLISHER_SA;
    bool glaive = vehicle->GetEntry() == NPC_GLAIVE_THROWER_A || vehicle->GetEntry() == NPC_GLAIVE_THROWER_H;
    bool const isRamVehicle = engine || demolisher;
    uint32 const ram = isRamVehicle
        ? VehicleSpell(vehicle, {67441, 67796, 67442, 67816, 60206, 54107, 54109, 51678}, false) : 0;
    float ramRange = 14.f;
    if (icMap)
        if (SpellInfo const* info = sSpellMgr->GetSpellInfo(ram))
            for (SpellEffectInfo const& effect : info->GetEffects())
                if (effect.IsEffect(SPELL_EFFECT_GAMEOBJECT_DAMAGE))
                    ramRange = effect.CalcRadius(vehicle);
    if (canMove && !cannon)
    {
        float range = (engine || demolisher) ? 8.f : glaive ? 45.f : catapult ? 28.f : 30.f;
        Position approach = destination;
        float tolerance = 4.f;
        if (building && icMap)
        {
            if (glaive)
                range = 35.f;
            if (ram)
            {
                range = ramRange - 2.f;
                tolerance = 1.5f;
            }
            approach = ICExteriorApproach(vehicle, building, range);
        }
        else if (building)
        {
            approach = building->GetPosition();
            float angle = building->GetAbsoluteAngle(vehicle);
            approach.m_positionX += std::cos(angle) * range;
            approach.m_positionY += std::sin(angle) * range;
            vehicle->UpdateAllowedPositionZ(approach.m_positionX, approach.m_positionY, approach.m_positionZ);
        }
        MoveToSiegeObjective(approach, tolerance, gates, attacker);
    }
    if (_siegeWeaponTimer || vehicle->HasUnitState(UNIT_STATE_CASTING))
        return;
    if (catapult && building && vehicle->GetExactDist2d(building) < 120.f)
    {
        bool hasTroop = false;
        for (auto const& seat : me->GetVehicle()->Seats)
            if (seat.first != 0)
                if (Unit* passenger = me->GetVehicle()->GetPassenger(seat.first))
                    hasTroop |= passenger->IsNPCBot();
        if (hasTroop && VehicleSpell(vehicle, {66218}) == 66218)
        {
            Position landing = ICExteriorApproach(vehicle, building, 35.f);
            vehicle->CastSpell(landing, 66218, false);
            _siegeWeaponTimer = 15000;
        }
        else if (!hasTroop && vehicle->GetExactDist2d(building) < 45.f)
        {
            uint32 catSpell = VehicleSpell(vehicle, {66186, 66296, 68832, 50989, 57606});
            if (catSpell)
            {
                vehicle->SetFacingToObject(building);
                vehicle->CastSpell(building, catSpell, false);
                _siegeWeaponTimer = 3500;
                return;
            }
            me->ExitVehicle();
            _siegeBoardTimer = 30000;
        }
        return;
    }
    Unit* enemy = vehicle->SelectNearestTarget(cannon ? 80.f : 45.f);
    if (cannon)
    {
        Creature* siegeTarget = nullptr;
        for (uint32 entry : { uint32(NPC_DEMOLISHER_SA), uint32(NPC_DEMOLISHER), uint32(NPC_SIEGE_ENGINE_A),
            uint32(NPC_SIEGE_ENGINE_H), uint32(NPC_GLAIVE_THROWER_A), uint32(NPC_GLAIVE_THROWER_H) })
        {
            std::list<Creature*> vehicles;
            vehicle->GetCreatureListWithEntryInGrid(vehicles, entry, 80.f);
            for (Creature* other : vehicles)
                if (other->IsAlive() && vehicle->IsValidAttackTarget(other) && vehicle->IsWithinLOSInMap(other)
                    && (!siegeTarget || vehicle->GetExactDist(other) < vehicle->GetExactDist(siegeTarget)))
                    siegeTarget = other;
        }
        if (siegeTarget)
            enemy = siegeTarget;
    }
    if (enemy && !vehicle->IsWithinLOSInMap(enemy))
        enemy = nullptr;
    bool siege = building && building->GetDestructibleState() != GO_DESTRUCTIBLE_DESTROYED && !catapult
        && vehicle->GetExactDist2d(building) <= (isRamVehicle ? 15.f : cannon ? 200.f : glaive ? 75.f : 45.f);
    if (!siege && !enemy)
        return;
    uint32 spell = 0;
    bool isRamSpell = false;
    if (isRamVehicle)
    {
        if (siege && ram && vehicle->GetExactDist2d(building) <= ramRange)
        {
            spell = ram;
            isRamSpell = true;
        }
        if (!spell)
            spell = VehicleSpell(vehicle, {67440, 52338, 50896, 68068});
    }
    else if (glaive)
        spell = VehicleSpell(vehicle, {66456, 67034, 67195, 68826, 68827});
    else if (cannon)
        spell = VehicleSpell(vehicle, {49872, 67452, 67461, 51362, 51421, 66518, 66529});
    else if (catapult)
        spell = VehicleSpell(vehicle, {66186, 68832, 50989, 57606, 66296});
    else
    {
        spell = VehicleSpell(vehicle, {66203, 67461, 66186, 60206, 67441, 54107, 52338, 67440});
    }
    if (!spell)
        return;
    if (siege && building)
        vehicle->SetFacingToObject(building);
    else if (enemy)
        vehicle->SetFacingToObject(enemy);
    SpellCastResult result = SPELL_FAILED_BAD_TARGETS;
    if (isRamSpell)
        result = vehicle->CastSpell(vehicle, spell, false);
    else if (siege && building)
    {
        result = vehicle->CastSpell(building, spell, false);
        if (result != SPELL_CAST_OK)
            result = vehicle->CastSpell(building->GetPosition(), spell, false);
    }
    else
    {
        Position target = enemy ? enemy->GetPosition() : destination;
        if (enemy && enemy->isMoving())
        {
            float lead = std::min(1.f, vehicle->GetExactDist(enemy) / 60.f) * enemy->GetSpeed(MOVE_RUN);
            target.m_positionX += std::cos(enemy->GetOrientation()) * lead;
            target.m_positionY += std::sin(enemy->GetOrientation()) * lead;
        }
        result = vehicle->CastSpell(target, spell, false);
    }
    if (result == SPELL_CAST_OK)
    {
        SpellInfo const* info = sSpellMgr->GetSpellInfo(spell);
        _siegeWeaponTimer = std::max(2000u, std::max(info ? info->RecoveryTime : 2000u, info ? info->CategoryRecoveryTime : 2000u));
    }
    else if (icMap)
        _siegeWeaponTimer = 1000;
}

bool bot_ai::UpdateSiegeBattleground(uint32 diff)
{
    using namespace NPCBots::SiegeBG;
    Battleground* bg = GetBG();
    auto tick = [diff](uint32& timer) { timer = timer > diff ? timer - diff : 0; };
    tick(_siegeThinkTimer); tick(_siegeWeaponTimer); tick(_siegeBoardTimer); tick(_siegeMountTimer); tick(_siegeBombTimer);
    bool saMap = bg->GetTypeID() == BATTLEGROUND_SA;
    BattlegroundSA* sa = saMap ? static_cast<BattlegroundSA*>(bg) : nullptr;
    if (bg->GetStatus() != STATUS_IN_PROGRESS || (sa && sa->GetRoundStatus() != BG_SA_ROUND_ONE && sa->GetRoundStatus() != BG_SA_ROUND_TWO))
    {
        if (sa)
        {
            // Both teams stay at their round spawn during preparation. Reissuing
            // road movement here on every AI tick makes defenders visibly shake.
            if (!HasBotCommandState(BOT_COMMAND_STAY))
                SetBotCommandState(BOT_COMMAND_STAY);
            if (me->isMoving())
                me->BotStopMovement();
        }
        return true;
    }
    RemoveBotCommandState(BOT_COMMAND_STAY);
    me->RemoveAurasDueToSpell(SPELL_PREPARATION);
    if (Spell* cast = me->GetCurrentSpell(CURRENT_GENERIC_SPELL))
        if (cast->m_spellInfo->Id == OPEN_FLAG_BG)
            return true; // Real interruptible banner cast; class rotations must not cancel it.
    if (!me->GetVehicle() && (me->HasUnitState(UNIT_STATE_JUMPING) || (me->IsSplineEnabled() && me->IsFalling())))
        return true;
    if (SiegeControlBlocked(me->GetVehicle() != nullptr, IsCasting(), CCed(me, true)))
        return me->GetVehicle() != nullptr;
    if (_siegeThinkTimer)
        return me->GetVehicle() != nullptr;
    _siegeThinkTimer = 1000;
    TeamId team = bg->GetBotTeamId(me->GetGUID());
    uint32 id = me->GetGUID().GetCounter();
    uint32 gates = GateMask(bg);
    bool attacker = sa ? sa->GetAttackersTeam() == team : true;
    uint8 squad = sa ? uint8(id % 3) : ICSquad(id);
    Position destination = me->GetPosition();
    GameObject* objective = nullptr;
    GameObject* building = nullptr;
    bool defend = !attacker;
    bool finalAssault = false;
    if (_siegeBombCarrier && !me->HasAura(saMap ? 52418 : 66271) && !me->HasAura(68377))
        _siegeBombCarrier = false;

    if (sa)
    {
        uint8 role = attacker ? 1 : 0;
        if (_siegeSARole != role)
        {
            _siegeBombCarrier = false;
            _siegeSARole = role;
        }
        bool const west = (id / 5) % 2 == 0;
        Gate gate = AssaultGate(gates, west);
        if (!attacker && id % 5 == 3)
        {
            Gate const other = AssaultGate(gates, !west);
            if (gate != other && SAReinforceOtherLane(GateHealthFraction(sa->GetBGObject(gate, false)),
                GateHealthFraction(sa->GetBGObject(other, false))))
                gate = other;
        }

        if (gate != NoGate)
        {
            building = sa->GetBGObject(uint32(gate), true);
            if (building)
            {
                destination = building->GetPosition();
                destination.m_positionX += attacker ? 30.f : -20.f;
                if (!attacker)
                    if (WanderNode const* cover = WanderNode::FindInMapWPs(me->GetMapId(), [gate](WanderNode const* node) {
                        return node->GetWPId() == SAGateDefenseNode(gate);
                    }))
                        destination = cover->GetPosition();

                if (!attacker && gate != Ancient && SADefenderGunner(id) && !HasRole(BOT_ROLE_HEAL))
                {
                    uint8 preferred = SACannonIndex(gate, id);
                    for (uint8 slot : {preferred, uint8(preferred ^ 1)})
                        if (Creature* cannon = sa->GetBGCreature(slot, true))
                            if (cannon->IsAlive() && cannon->GetHealthPct() >= 15.f && cannon->GetVehicleKit() &&
                                (me->GetVehicleBase() == cannon ||
                                    (cannon->GetVehicleKit()->HasEmptySeat(0) && !HumanNear(cannon, 10.f))))
                            {
                                destination = cannon->GetPosition();
                                break;
                            }
                }
                me->UpdateAllowedPositionZ(destination.m_positionX, destination.m_positionY, destination.m_positionZ);
            }
        }
        else if (attacker)
        {
            objective = sa->GetBGObject(BG_SA_TITAN_RELIC, true);
            finalAssault = true;
        }
        else
        {
            // Fall back to the chamber entrance after the final door is breached.
            destination = BG_SA_ObjSpawnlocs[BG_SA_ANCIENT_GATE];
            destination.m_positionX -= 22.f;
        }
        if (attacker)
            if (GameObject* selected = SelectSAAttackObjective(me, sa))
            {
                objective = selected;
                building = nullptr; // A capture runner must not detour for gate bombs.
                finalAssault = objective == sa->GetBGObject(BG_SA_TITAN_RELIC, true);
            }
    }
    else
    {
        BattlegroundIC* ic = static_cast<BattlegroundIC*>(bg);
        auto nodes = ic->GetNodePoints();
        for (auto [node, spell] : { std::pair{NODE_TYPE_REFINERY, SPELL_OIL_REFINERY}, std::pair{NODE_TYPE_QUARRY, SPELL_QUARRY} })
        {
            if (ControlledBy(nodes[node], team))
            {
                if (!me->HasAura(spell))
                    me->CastSpell(me, spell, true);
            }
            else
                me->RemoveAurasDueToSpell(spell);
        }
        if (me->GetTransport() && !me->GetVehicle())
        {
            if (me->GetTransport() != ic->GetGunship(team) || !ControlledBy(nodes[NODE_TYPE_HANGAR], team))
                return true;
            std::list<Creature*> cannons;
            me->GetCreatureListWithEntryInGrid(cannons,
                team == TEAM_ALLIANCE ? NPC_ALLIANCE_GUNSHIP_CANNON : NPC_HORDE_GUNSHIP_CANNON, 60.f);
            for (Creature* cannon : cannons)
                if (cannon->IsAlive() && cannon->GetTransport() == me->GetTransport() && me->InSamePhase(cannon)
                    && !cannon->HasUnitFlag(UNIT_FLAG_UNINTERACTIBLE) && cannon->GetVehicleKit()
                    && cannon->GetVehicleKit()->HasEmptySeat(0) && !HumanNear(cannon, 10.f))
                {
                    if (me->IsWithinDistInMap(cannon, INTERACTION_DISTANCE))
                        me->EnterVehicle(cannon, 0);
                    else
                        me->GetMotionMaster()->MovePoint(607630, cannon->GetPosition(), true);
                    return true;
                }
            return true;
        }
        if (squad == 2 && id % 5 < 2 && !me->GetVehicle() && !me->IsInCombat()
            && ControlledBy(nodes[NODE_TYPE_HANGAR], team))
            if (GameObject* portal = ic->GetBGObject(BG_IC_GO_HANGAR_TELEPORTER_1, true);
                portal && portal->isSpawned() && !portal->HasFlag(GO_FLAG_NOT_SELECTABLE) && me->InSamePhase(portal))
            {
                if (me->IsWithinDistInMap(portal, INTERACTION_DISTANCE))
                    me->CastSpell(me, team == TEAM_ALLIANCE ? 66630 : 66637, false);
                else
                    MoveToSiegeObjective(portal->GetPosition(), 3.f, gates, true);
                return true;
            }
        auto selectICEnemyGate = [&](BattlegroundIC* bgIC, TeamId botTeam, Position const& moverPos) -> GameObject* {
            uint32 const gateEntries[2][3] = {
                { BG_IC_GO_HORDE_GATE_1, BG_IC_GO_HORDE_GATE_2, BG_IC_GO_HORDE_GATE_3 },
                { BG_IC_GO_ALLIANCE_GATE_3, BG_IC_GO_ALLIANCE_GATE_1, BG_IC_GO_ALLIANCE_GATE_2 }
            };
            uint8 const doorIndices[2][3] = {
                { BG_IC_H_FRONT, BG_IC_H_WEST, BG_IC_H_EAST },
                { BG_IC_A_FRONT, BG_IC_A_WEST, BG_IC_A_EAST }
            };
            uint8 t = botTeam == TEAM_ALLIANCE ? 0 : 1;
            GameObject* best = nullptr;
            float bestScore = std::numeric_limits<float>::max();
            for (uint8 i = 0; i < 3; ++i)
            {
                if (!Broken(gates, doorIndices[t][i]))
                {
                    if (GameObject* go = bgIC->GetBGObject(gateEntries[t][i], true))
                    {
                        if (go->isSpawned() && go->GetDestructibleState() != GO_DESTRUCTIBLE_DESTROYED)
                        {
                            Position approach = ICExteriorApproach(me, go, 28.f);
                            // Keep pressure on a weakened gate unless another approach is appreciably closer.
                            float score = moverPos.GetExactDist(&approach) + 90.f * GateHealthFraction(go);
                            if (i == 0)
                                score -= 15.f;
                            if (score < bestScore)
                            {
                                bestScore = score;
                                best = go;
                            }
                        }
                    }
                }
            }
            return best;
        };

        uint32 enemyMask = team == TEAM_ALLIANCE ? 7u : 56u;
        uint32 ownMask = team == TEAM_ALLIANCE ? 56u : 7u;
        Creature* ownCommander = ic->GetBGCreature(team == TEAM_ALLIANCE ? BG_IC_NPC_HIGH_COMMANDER_HALFORD_WYRMBANE : BG_IC_NPC_OVERLORD_AGMAR, true);
        bool commanderAttacked = ownCommander && (ownCommander->IsInCombat() || ownCommander->GetHealthPct() < 95.f);
        defend = ((gates & ownMask) || commanderAttacked) && (squad == 3 || squad == 6);
        uint8 reserveNode = MAX_NODE_TYPES;
        if (squad == 6 && !defend)
            for (uint8 node : {NODE_TYPE_WORKSHOP, NODE_TYPE_DOCKS, NODE_TYPE_HANGAR})
                if (nodes[node].needChange && nodes[node].faction != team
                    && nodes[node].last_entry == nodes[node].banners[team == TEAM_ALLIANCE ? BANNER_A_CONTROLLED : BANNER_H_CONTROLLED])
                {
                    reserveNode = node;
                    break;
                }

        uint8 const primaryNode = ICPrimaryNode(squad, uint8(team));
        bool const keepSentry = SACaptureScout(id) && squad <= 2 && ControlledBy(nodes[primaryNode], team);
        if ((gates & enemyMask) && !defend && squad != 4 && reserveNode == MAX_NODE_TYPES && !keepSentry)
        {
            finalAssault = true;
            uint8 keepNode = team == TEAM_ALLIANCE ? NODE_TYPE_GRAVEYARD_H : NODE_TYPE_GRAVEYARD_A;
            GameObject* keepBanner = ic->GetBGObject(nodes[keepNode].gameobject_type, true);
            if (id % 5 == 4 && keepBanner && !ControlledBy(nodes[keepNode], team))
            {
                destination = keepBanner->GetPosition();
                if (nodes[keepNode].faction != team)
                    objective = keepBanner;
            }
            else if (Creature* commander = ic->GetBGCreature(team == TEAM_ALLIANCE ? BG_IC_NPC_OVERLORD_AGMAR : BG_IC_NPC_HIGH_COMMANDER_HALFORD_WYRMBANE, true))
            {
                destination = commander->GetPosition();
                if (!me->GetVehicle() && me->IsWithinDistInMap(commander, 45.f) && me->IsWithinLOSInMap(commander)
                    && (HasRole(BOT_ROLE_HEAL) || CanBotAttack(commander)))
                {
                    if (me->IsMounted()) DismountBot();
                    if (!HasRole(BOT_ROLE_HEAL) && !me->GetVictim())
                        AttackStart(commander);
                    return false;
                }
            }
        }
        else if (defend)
        {
            auto const& spawn = BG_IC_NpcSpawnlocs[team == TEAM_ALLIANCE ? BG_IC_NPC_HIGH_COMMANDER_HALFORD_WYRMBANE : BG_IC_NPC_OVERLORD_AGMAR];
            destination.Relocate(spawn.x, spawn.y, spawn.z, spawn.o);
        }
        else
        {
            uint8 node = reserveNode == MAX_NODE_TYPES ? primaryNode : reserveNode;
            // Keep resource sentries; the remainder reinforce vehicle-producing bases.
            if (node <= NODE_TYPE_QUARRY && ControlledBy(nodes[node], team) && id % 5 != 4)
                node = id % 2 ? NODE_TYPE_DOCKS : NODE_TYPE_WORKSHOP;
            objective = ic->GetBGObject(nodes[node].gameobject_type, true);
            if (objective)
                destination = objective->GetPosition();
            bool ownsVehicleBase = ControlledBy(nodes[node], team) && (node == NODE_TYPE_WORKSHOP || node == NODE_TYPE_DOCKS);
            if (ownsVehicleBase && (id % 5 != 4 || squad == 7))
            {
                building = selectICEnemyGate(ic, team, me->GetPosition());
                if (building)
                {
                    destination = ICExteriorApproach(me, building, 28.f);
                    objective = nullptr;
                }
            }
            else if (objective && nodes[node].faction == team)
            {
                // Guard both sides of the flag while a capture completes or a base is held.
                if (id % 5 != 4)
                    destination = objective->GetFirstCollisionPosition(10.f, float(id % 5) * float(M_PI / 2));
                objective = nullptr;
            }
            if (!objective && !me->GetVehicle())
            {
                for (uint8 i = 0; i < MAX_NODE_TYPES; ++i)
                {
                    if (nodes[i].faction != team && !ControlledBy(nodes[i], team))
                    {
                        if (GameObject* banner = ic->GetBGObject(nodes[i].gameobject_type, true))
                        {
                            if (banner->isSpawned() && me->InSamePhase(banner) && !banner->HasFlag(GO_FLAG_NOT_SELECTABLE) &&
                                me->IsWithinDistInMap(banner, 25.f) && ICSegmentOpen(me->GetPositionX(), me->GetPositionY(),
                                    banner->GetPositionX(), banner->GetPositionY(), gates))
                            {
                                objective = banner;
                                destination = banner->GetPosition();
                                break;
                            }
                        }
                    }
                }
            }
        }
        if (me->GetTransport() && me->GetVehicle())
            building = selectICEnemyGate(ic, team, me->GetPosition());
        if (me->GetVehicle())
        {
            Creature* veh = me->GetVehicleCreatureBase();
            if (veh && IsGroundSiegeVehicle(veh))
            {
                objective = nullptr;
                if (GameObject* targetGate = selectICEnemyGate(ic, team, me->GetPosition()))
                {
                    building = targetGate;
                    destination = ICExteriorApproach(me, building, 28.f);
                }
            }
        }
    }
    if (objective)
        destination = objective->GetPosition();
    if (finalAssault && me->GetVehicle() && !me->GetVehicleBase()->GetTransport())
    {
        me->ExitVehicle();
        _siegeBoardTimer = 30000;
        _siegeTravelNode = 0;
    }
    if (me->GetVehicle())
    {
        if (sa && !attacker && me->GetVehicleCreatureBase()
            && me->GetVehicleCreatureBase()->GetExactDist2d(&destination) > 120.f)
        {
            me->ExitVehicle();
            _siegeBoardTimer = 10000;
            return true;
        }
        UpdateSiegeVehicle(diff, attacker ? building : nullptr, destination, gates, attacker);
        return true;
    }
    bool const healer = HasRole(BOT_ROLE_HEAL);
    if (attacker && !defend && building && !finalAssault && !objective && !_siegeBombCarrier &&
        !me->GetTransport() && !me->IsInCombat() && !SACaptureScout(id) && (SiegeEscortMember(id) || healer))
        if (Creature* vehicle = FindSiegeEscortVehicle(me, bg, team, gates))
        {
            Position const cover = SiegeEscortPosition(me, vehicle, healer, IsTank());
            float const radius = cover.GetExactDist(vehicle) < 1.f ? 12.f : 5.f;
            _siegeMountTimer = 2500;
            if (me->GetExactDist(&cover) <= radius)
            {
                if (me->isMoving())
                    me->StopMoving();
                return false;
            }

            // Follow the formation only along a verified local route and let
            // combat or the next objective take over when the escort breaks up.
            bool const reachable = sa ? SAPathFromClear(me, sa, me->GetPosition(), cover)
                : ICLocalPathClear(me, cover, gates, radius);
            if (reachable)
            {
                _siegeTravelNode = 0;
                Position current;
                if (!me->isMoving() || !me->GetMotionMaster()->GetDestination(current.m_positionX, current.m_positionY, current.m_positionZ)
                    || current.GetExactDist(&cover) > 4.f)
                    me->GetMotionMaster()->MovePoint(607628, cover, true);
                return false;
            }
        }

    if (!finalAssault && !objective && TrySiegeVehicle(defend, squad))
        return true;
    if (!objective && me->IsInCombat())
        return false;
    if (objective && me->IsWithinDistInMap(objective, INTERACTION_DISTANCE) && (!sa || me->IsWithinLOSInMap(objective)))
    {
        if (!objective->HasFlag(GO_FLAG_NOT_SELECTABLE))
        {
            bool anotherCaster = false;
            for (Unit* ally : BotMgr::GetAllGroupMembers(me))
                if (ally != me)
                    if (Spell const* cast = ally->GetCurrentSpell(CURRENT_GENERIC_SPELL))
                        if (cast->m_spellInfo->Id == OPEN_FLAG_BG && cast->m_targets.GetGOTargetGUID() == objective->GetGUID())
                            anotherCaster = true;
            if (!anotherCaster)
            {
                DismountBot();
                removeShapeshiftForm();
                me->StopMoving();
                me->AttackStop();
                if (me->CastSpell(objective, OPEN_FLAG_BG, false) == SPELL_CAST_OK)
                    return true;
            }
        }
    }
    if (me->IsInCombat())
        return false;
    // Carry a real bomb payload from an existing supply pile, then plant the native charge.
    if (attacker && !objective && (saMap || building) && !_siegeBombTimer && !HasRole(BOT_ROLE_HEAL))
    {
        if (!_siegeBombCarrier)
        {
            std::list<GameObject*> supplies;
            if (saMap)
                me->GetGameObjectListWithEntryInGrid(supplies, BG_SA_ObjEntries[BG_SA_BOMB], building ? 80.f : 8.f);
            else
            {
                me->GetGameObjectListWithEntryInGrid(supplies, GO_SEAFORIUM_BOMBS, building ? 80.f : 8.f);
                me->GetGameObjectListWithEntryInGrid(supplies, GO_HUGE_SEAFORIUM_BOMB_A, 80.f);
                me->GetGameObjectListWithEntryInGrid(supplies, GO_HUGE_SEAFORIUM_BOMB_H, 80.f);
            }
            GameObject* closestSupply = nullptr;
            for (GameObject* supply : supplies)
                if (supply->isSpawned() && (!closestSupply || me->GetExactDist(supply) < me->GetExactDist(closestSupply)))
                    closestSupply = supply;
            if (closestSupply)
            {
                if (me->IsWithinDistInMap(closestSupply, INTERACTION_DISTANCE))
                {
                    bool isHuge = closestSupply->GetEntry() == GO_HUGE_SEAFORIUM_BOMB_A || closestSupply->GetEntry() == GO_HUGE_SEAFORIUM_BOMB_H;
                    me->CastSpell(me, saMap ? 52418 : isHuge ? 68377 : 66271, true);
                    _siegeBombCarrier = true;
                    _siegeBombTimer = 2000;
                }
                else if (building)
                    destination = closestSupply->GetPosition();
            }
        }
        else if (building && me->GetExactDist2d(building) < 9.f && me->IsWithinLOSInMap(building))
        {
            DismountBot();
            me->StopMoving();
            me->SetFacingToObject(building);
            uint32 plantSpell = saMap ? 52410 : me->HasAura(68377) ? 66674 : 66268;
            if (me->CastSpell(me, plantSpell, false) == SPELL_CAST_OK)
            {
                _siegeBombCarrier = false;
                me->RemoveAurasDueToSpell(saMap ? 52418 : 66271);
                me->RemoveAurasDueToSpell(68377);
                _siegeBombTimer = 15000;
            }
            return true;
        }
        if (_siegeBombCarrier && building)
        {
            if (saMap)
            {
                destination = building->GetPosition();
                float angle = building->GetAbsoluteAngle(me);
                destination.m_positionX += std::cos(angle) * 7.f;
                destination.m_positionY += std::sin(angle) * 7.f;
            }
            else
                destination = ICExteriorApproach(me, building, 7.f);
        }
    }
    MoveToSiegeObjective(destination, objective ? 3.f : _siegeBombCarrier && building ? 1.5f : 7.f, gates, attacker);
    return false;
}
