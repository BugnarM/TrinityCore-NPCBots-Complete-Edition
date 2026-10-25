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
#include "Spell.h"
#include "SpellHistory.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "Transport.h"
#include "Vehicle.h"
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

bool ControlledBy(ICNodePoint const& node, TeamId team)
{
    return node.faction == team && node.nodeState == (team == TEAM_ALLIANCE ? NODE_STATE_CONTROLLED_A : NODE_STATE_CONTROLLED_H);
}

uint32 VehicleSpell(Creature* vehicle, std::initializer_list<uint32> candidates)
{
    for (uint32 candidate : candidates)
        for (uint32 spell : vehicle->m_spells)
            if (spell == candidate)
                return spell;
    for (uint32 spell : vehicle->m_spells)
        if (SpellInfo const* info = sSpellMgr->GetSpellInfo(spell))
            if (info->HasEffect(SPELL_EFFECT_TRIGGER_MISSILE))
                return spell;
    return 0;
}
}

bool bot_ai::IsSiegeBattlegroundBot() const
{
    Battleground* bg = GetBG();
    return IsWanderer() && bg && (bg->GetTypeID() == BATTLEGROUND_SA || bg->GetTypeID() == BATTLEGROUND_IC);
}

void bot_ai::ResetSiegeBattlegroundState()
{
    _siegeThinkTimer = _siegeWeaponTimer = _siegeBoardTimer = _siegeBombTimer = _siegeTravelNode = 0;
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
                    if (go->isSpawned() && !go->HasFlag(GO_FLAG_NOT_SELECTABLE) &&
                        (!portal || me->GetExactDist(go) < me->GetExactDist(portal))) portal = go;
        if (!portal) return false;
        if (me->IsWithinDistInMap(portal, INTERACTION_DISTANCE) && me->IsWithinLOSInMap(portal))
        {
            DismountBot();
            me->GetMotionMaster()->Clear(MOTION_PRIORITY_NORMAL);
            me->StopMoving();
            portal->Use(me);
            _siegeTravelNode = 0;
            _siegeThinkTimer = 2000;
        }
        else
            mover->GetMotionMaster()->MovePoint(607628, portal->GetPosition(), true);
        return false;
    }
    auto allowed = [&](WanderNode const* node) {
        if (bg->GetTypeID() == BATTLEGROUND_SA)
            return node->GetWPId() >= 5200 && node->GetWPId() <= 5226
                && (!attacker || SANodeAccessible(node->GetWPId(), gates));
        if (node->GetWPId() < 5300 || node->GetWPId() > 5322)
            return false;
        return true; // Gate edges, physical attachment and spline validation enforce access.
    };
    auto edgeOpen = [&](WanderNode const* from, WanderNode const* to) {
        return bg->GetTypeID() == BATTLEGROUND_SA ? (!attacker || SAEdgeOpen(from->GetWPId(), to->GetWPId(), gates))
            : ICEdgeOpen(from->GetWPId(), to->GetWPId(), uint8(team), gates);
    };
    WanderNode const* start = nullptr;
    WanderNode const* finish = nullptr;
    float startDistance = std::numeric_limits<float>::max(), finishDistance = startDistance;
    WanderNode::DoForAllMapWPs(me->GetMapId(), [&](WanderNode const* node) {
        if (!allowed(node))
            return;
        float distance = mover->GetExactDistSq(node);
        if (distance < startDistance && (!icMap || ICNodeKeep(node->GetWPId()) == originKeep))
        {
            startDistance = distance;
            start = node;
        }
        distance = destination.GetExactDistSq(node);
        if (distance < finishDistance && (!icMap || ICNodeKeep(node->GetWPId()) == destinationKeep))
        {
            finishDistance = distance;
            finish = node;
        }
    });
    if (!start || !finish)
        return false;
    if (_siegeTravelNode)
        if (WanderNode const* current = WanderNode::FindInMapWPs(me->GetMapId(), [this](WanderNode const* node) {
            return node->GetWPId() == _siegeTravelNode;
        }))
            if (allowed(current) && mover->GetExactDist(current) < 18.f &&
                (!icMap || ICNodeKeep(current->GetWPId()) == originKeep))
                start = current;

    Position point = destination;
    if (start != finish)
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
        if (mover->GetExactDist(start) > 18.f)
        {
            point = *start;
            _siegeTravelNode = start->GetWPId();
        }
    }
    if (mover == me && !me->GetTransport())
    {
        // Stable small offsets spread squads without moving the interaction point.
        if (point.GetExactDist(&destination) > 10.f)
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
    if (defender && member != 1)
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
    float maxSearchDist = (squad == 0 || squad == 1 || squad == 5 || squad == 6 || squad == 7 || sa) ? 140.f : 45.f;

    for (uint32 entry : entries)
    {
        std::list<Creature*> vehicles;
        me->GetCreatureListWithEntryInGrid(vehicles, entry, maxSearchDist);
        for (Creature* vehicle : vehicles)
        {
            Vehicle* kit = vehicle->GetVehicleKit();
            if (!vehicle->IsAlive() || !kit || vehicle->HasUnitFlag(UNIT_FLAG_UNINTERACTIBLE)
                || vehicle->IsHostileTo(me) || HumanNear(vehicle, 10.f))
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
        me->GetMotionMaster()->MovePoint(607629, best->GetPosition(), true);
        return true;
    }
    DismountBot();
    removeShapeshiftForm();
    me->CombatStop(true);
    me->EnterVehicle(best, bestSeat);
    _siegeBoardTimer = 2000;
    return true;
}

void bot_ai::UpdateSiegeVehicle(uint32 /*diff*/, GameObject* building, Position const& destination, uint32 gates, bool attacker)
{
    Creature* vehicle = me->GetVehicleCreatureBase();
    if (!vehicle)
        return;
    if (vehicle->GetHealthPct() < 15.0f || HumanNear(vehicle, 8.f))
    {
        vehicle->StopMoving();
        me->ExitVehicle();
        _siegeBoardTimer = 20000;
        return;
    }
    if (!CanBotMoveVehicle())
        return;
    bool cannon = vehicle->GetEntry() == NPC_ANTI_PERSONNAL_CANNON || vehicle->GetEntry() == NPC_KEEP_CANNON
        || vehicle->GetEntry() == NPC_HORDE_GUNSHIP_CANNON || vehicle->GetEntry() == NPC_ALLIANCE_GUNSHIP_CANNON
        || vehicle->GetVehicleBase();
    bool catapult = vehicle->GetEntry() == NPC_CATAPULT;
    bool engine = vehicle->GetEntry() == NPC_SIEGE_ENGINE_A || vehicle->GetEntry() == NPC_SIEGE_ENGINE_H;
    bool glaive = vehicle->GetEntry() == NPC_GLAIVE_THROWER_A || vehicle->GetEntry() == NPC_GLAIVE_THROWER_H;
    if (!cannon)
    {
        float range = engine ? 8.f : glaive ? 55.f : catapult ? 28.f : 30.f;
        Position approach = destination;
        if (building)
        {
            approach = building->GetPosition();
            float angle = building->GetAbsoluteAngle(vehicle);
            approach.m_positionX += std::cos(angle) * range;
            approach.m_positionY += std::sin(angle) * range;
            vehicle->UpdateAllowedPositionZ(approach.m_positionX, approach.m_positionY, approach.m_positionZ);
        }
        MoveToSiegeObjective(approach, 4.f, gates, attacker);
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
            Position landing = building->GetPosition();
            landing.m_positionX += GetBG()->GetBotTeamId(me->GetGUID()) == TEAM_ALLIANCE ? -35.f : 35.f;
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
        && vehicle->GetExactDist2d(building) <= (engine ? 14.f : cannon ? 200.f : glaive ? 75.f : 45.f);
    if (!siege && !enemy)
        return;
    uint32 spell = 0;
    if (engine)
        spell = VehicleSpell(vehicle, {67796, 67442, 67441, 67816, 54109, 51678});
    else if (glaive)
        spell = VehicleSpell(vehicle, {66456, 67195, 67034, 68826, 68827});
    else if (cannon)
        spell = VehicleSpell(vehicle, {49872, 67452, 67461, 51362, 51421, 66518, 66529});
    else if (catapult)
        spell = VehicleSpell(vehicle, {66186, 68832, 50989, 57606, 66296});
    else
    {
        if (siege && vehicle->GetExactDist2d(building) <= 12.f)
            spell = VehicleSpell(vehicle, {60206, 67441, 54107});
        if (!spell)
            spell = VehicleSpell(vehicle, {52338, 67440, 50896, 68068});
    }
    if (!spell)
        return;
    if (siege)
        vehicle->SetFacingToObject(building);
    else
        vehicle->SetFacingToObject(enemy);
    SpellCastResult result = SPELL_FAILED_BAD_TARGETS;
    if (engine)
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
}

bool bot_ai::UpdateSiegeBattleground(uint32 diff)
{
    using namespace NPCBots::SiegeBG;
    Battleground* bg = GetBG();
    auto tick = [diff](uint32& timer) { timer = timer > diff ? timer - diff : 0; };
    tick(_siegeThinkTimer); tick(_siegeWeaponTimer); tick(_siegeBoardTimer); tick(_siegeBombTimer);
    bool saMap = bg->GetTypeID() == BATTLEGROUND_SA;
    BattlegroundSA* sa = saMap ? static_cast<BattlegroundSA*>(bg) : nullptr;
    if (bg->GetStatus() != STATUS_IN_PROGRESS || (sa && sa->GetRoundStatus() != BG_SA_ROUND_ONE && sa->GetRoundStatus() != BG_SA_ROUND_TWO))
    {
        if (sa && sa->GetAttackersTeam() != bg->GetBotTeamId(me->GetGUID()))
        {
            Position defense = BG_SA_ObjSpawnlocs[(me->GetGUID().GetCounter() / 5) % 2 ? BG_SA_GREEN_GATE : BG_SA_BLUE_GATE];
            defense.m_positionX -= 25.f;
            MoveToSiegeObjective(defense, 7.f, GateMask(bg), false);
        }
        return true;
    }
    RemoveBotCommandState(BOT_COMMAND_STAY);
    me->RemoveAurasDueToSpell(SPELL_PREPARATION);
    if (Spell* cast = me->GetCurrentSpell(CURRENT_GENERIC_SPELL))
        if (cast->m_spellInfo->Id == OPEN_FLAG_BG)
            return true; // Real interruptible banner cast; class rotations must not cancel it.
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
    if (_siegeBombCarrier && !me->HasAura(saMap ? 52418 : 66271))
        _siegeBombCarrier = false;

    if (sa)
    {
        uint8 role = attacker ? 1 : 0;
        if (_siegeSARole != role)
        {
            _siegeBombCarrier = false;
            _siegeSARole = role;
        }
        Gate gate = AssaultGate(gates, (id / 5) % 2 == 0);
        if (gate != NoGate)
        {
            building = sa->GetBGObject(uint32(gate), true);
            if (building)
            {
                destination = building->GetPosition();
                destination.m_positionX += attacker ? 30.f : -25.f;
                if (!attacker && id % 5 == 1 && !HasRole(BOT_ROLE_HEAL))
                {
                    static uint8 const guns[] = { 0, 2, 4, 6, 8, 2 };
                    if (Creature* cannon = sa->GetBGCreature(guns[uint8(gate)] + id % 2, true))
                        destination = cannon->GetPosition();
                }
                me->UpdateAllowedPositionZ(destination.m_positionX, destination.m_positionY, destination.m_positionZ);
            }
        }
        else
        {
            objective = sa->GetBGObject(BG_SA_TITAN_RELIC, true);
            finalAssault = true;
        }
        if (attacker && !me->GetVehicle())
            for (auto [flagType, gy] : { std::pair{BG_SA_LEFT_FLAG, BG_SA_LEFT_CAPTURABLE_GY},
                std::pair{BG_SA_RIGHT_FLAG, BG_SA_RIGHT_CAPTURABLE_GY}, std::pair{BG_SA_CENTRAL_FLAG, BG_SA_CENTRAL_CAPTURABLE_GY} })
            {
                GameObject* flag = sa->GetBGObject(flagType, true);
                if (flag && !flag->HasFlag(GO_FLAG_NOT_SELECTABLE) && sa->GetGraveyardStatus(gy) != team
                    && (id % 3 == 2 || me->IsWithinDistInMap(flag, 20.f)))
                {
                    objective = flag;
                    break;
                }
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
            std::list<Creature*> cannons;
            me->GetCreatureListWithEntryInGrid(cannons,
                team == TEAM_ALLIANCE ? NPC_ALLIANCE_GUNSHIP_CANNON : NPC_HORDE_GUNSHIP_CANNON, 60.f);
            for (Creature* cannon : cannons)
                if (cannon->GetVehicleKit() && cannon->GetVehicleKit()->HasEmptySeat(0) && !HumanNear(cannon, 10.f))
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
            if (GameObject* portal = ic->GetBGObject(BG_IC_GO_HANGAR_TELEPORTER_1, true))
            {
                if (me->IsWithinDistInMap(portal, INTERACTION_DISTANCE))
                    me->CastSpell(me, team == TEAM_ALLIANCE ? 66630 : 66637, false);
                else
                    MoveToSiegeObjective(portal->GetPosition(), 3.f, gates, true);
                return true;
            }
        uint32 enemyMask = team == TEAM_ALLIANCE ? 7u : 56u;
        uint32 ownMask = team == TEAM_ALLIANCE ? 56u : 7u;
        defend = (gates & ownMask) && (squad == 3 || squad == 6);
        if ((gates & enemyMask) && !defend && squad != 4)
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
                if (!me->GetVehicle() && me->IsWithinDistInMap(commander, 45.f) && me->IsWithinLOSInMap(commander))
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
            uint8 node = ICPrimaryNode(squad, uint8(team));
            // Keep resource sentries; the remainder reinforce vehicle-producing bases.
            if (node <= NODE_TYPE_QUARRY && ControlledBy(nodes[node], team) && id % 5 != 4)
                node = id % 2 ? NODE_TYPE_DOCKS : NODE_TYPE_WORKSHOP;
            if (squad == 6)
                for (uint8 i = NODE_TYPE_DOCKS; i < MAX_NODE_TYPES; ++i)
                    if (nodes[i].needChange && nodes[i].faction != team
                        && nodes[i].last_entry == nodes[i].banners[team == TEAM_ALLIANCE ? BANNER_A_CONTROLLED : BANNER_H_CONTROLLED])
                    {
                        node = i;
                        break;
                    }
            objective = ic->GetBGObject(nodes[node].gameobject_type, true);
            if (objective)
                destination = objective->GetPosition();
            bool ownsVehicleBase = ControlledBy(nodes[node], team) && (node == NODE_TYPE_WORKSHOP || node == NODE_TYPE_DOCKS);
            if (ownsVehicleBase && (id % 5 != 4 || squad == 7))
            {
                uint32 gateType = team == TEAM_ALLIANCE ? BG_IC_GO_HORDE_GATE_1 : BG_IC_GO_ALLIANCE_GATE_3;
                building = ic->GetBGObject(gateType, true);
                if (building)
                {
                    destination = building->GetPosition();
                    destination.m_positionX += team == TEAM_ALLIANCE ? -35.f : 35.f;
                    objective = nullptr;
                }
            }
            else if (objective && (ControlledBy(nodes[node], team) || nodes[node].faction == team))
                objective = nullptr; // Defend the banner during its capture timer.
        }
        if (me->GetTransport() && me->GetVehicle())
            building = ic->GetBGObject(team == TEAM_ALLIANCE ? BG_IC_GO_HORDE_GATE_1 : BG_IC_GO_ALLIANCE_GATE_3, true);
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
    if (!finalAssault && TrySiegeVehicle(defend, squad))
        return true;
    if (me->IsInCombat())
        return false;
    if (objective && me->IsWithinDistInMap(objective, INTERACTION_DISTANCE) && me->IsWithinLOSInMap(objective))
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
                me->StopMoving();
                me->CastSpell(objective, OPEN_FLAG_BG, false);
                return true;
            }
        }
    }
    // Carry a real bomb payload from an existing supply pile, then plant the native charge.
    if (attacker && !_siegeBombTimer && !HasRole(BOT_ROLE_HEAL))
    {
        if (!_siegeBombCarrier)
        {
            std::list<GameObject*> supplies;
            me->GetGameObjectListWithEntryInGrid(supplies, saMap ? BG_SA_ObjEntries[BG_SA_BOMB] : GO_SEAFORIUM_BOMBS, building ? 80.f : 8.f);
            GameObject* closestSupply = nullptr;
            for (GameObject* supply : supplies)
                if (supply->isSpawned() && (!closestSupply || me->GetExactDist(supply) < me->GetExactDist(closestSupply)))
                    closestSupply = supply;
            if (closestSupply)
            {
                if (me->IsWithinDistInMap(closestSupply, INTERACTION_DISTANCE))
                {
                    me->CastSpell(me, saMap ? 52418 : 66271, true);
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
            if (me->CastSpell(me, saMap ? 52410 : 66268, false) == SPELL_CAST_OK)
            {
                _siegeBombCarrier = false;
                me->RemoveAurasDueToSpell(saMap ? 52418 : 66271);
                _siegeBombTimer = 15000;
            }
            return true;
        }
        if (_siegeBombCarrier && building)
        {
            destination = building->GetPosition();
            float angle = building->GetAbsoluteAngle(me);
            destination.m_positionX += std::cos(angle) * 7.f;
            destination.m_positionY += std::sin(angle) * 7.f;
        }
    }
    MoveToSiegeObjective(destination, objective ? 3.f : _siegeBombCarrier && building ? 1.5f : 7.f, gates, attacker);
    return false;
}
