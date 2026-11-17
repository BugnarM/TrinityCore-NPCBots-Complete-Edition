/*
 * NPCBot City Raids (Stormwind & Orgrimmar)
 * Dynamic 40-man Wanderer Raid vs 15-man Defenders + City Boss
 */

#include "city_raids.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "GameEventMgr.h"
#include "bot_ai.h"
#include "botdatamgr.h"
#include "botmgr.h"
#include "Config.h"
#include "Chat.h"
#include "MotionMaster.h"
#include "Player.h"
#include "World.h"
#include "ObjectAccessor.h"
#include "MapManager.h"
#include <cmath>
#include "botcityraid.h"
#include "ChatPackets.h"
#include "DBCStores.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "Group.h"
#include "GroupMgr.h"
#include "ObjectMgr.h"
#include "PathGenerator.h"
#include "MMapFactory.h"
#include "MoveSplineInit.h"
#include "SpellAuras.h"
#include "SpellAuraEffects.h"
#include "SpellInfo.h"
#include "Spell.h"
#include "ThreatManager.h"
#include "WorldSession.h"
#include "Log.h"
#include <atomic>
#include <deque>
#include <memory>
#include <limits>
#include <mutex>
#include <unordered_map>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace CityRaidConstants
{
    struct BotRoleProfile
    {
        uint8 botClass;
        uint8 spec;
        uint32 roles;
    };

    static const BotRoleProfile AttackerRoster[40] = {
        // 4 Tanks
        { BOT_CLASS_WARRIOR,      BOT_SPEC_WARRIOR_PROTECTION, BOT_ROLE_TANK }, // Protection Warrior (Raid Leader)
        { BOT_CLASS_PALADIN,      BOT_SPEC_PALADIN_PROTECTION, BOT_ROLE_TANK }, // Protection Paladin
        { BOT_CLASS_DEATH_KNIGHT, BOT_SPEC_DK_BLOOD, BOT_ROLE_TANK }, // Blood DK
        { BOT_CLASS_DRUID,        BOT_SPEC_DRUID_FERAL, BOT_ROLE_TANK }, // Feral Bear

        // 8 Healers
        { BOT_CLASS_PALADIN,      BOT_SPEC_PALADIN_HOLY, BOT_ROLE_HEAL | BOT_ROLE_RANGED }, // Holy Paladin
        { BOT_CLASS_SHAMAN,       BOT_SPEC_SHAMAN_RESTORATION, BOT_ROLE_HEAL | BOT_ROLE_RANGED }, // Restoration Shaman
        { BOT_CLASS_PRIEST,       BOT_SPEC_PRIEST_HOLY, BOT_ROLE_HEAL | BOT_ROLE_RANGED }, // Holy Priest
        { BOT_CLASS_DRUID,        BOT_SPEC_DRUID_RESTORATION, BOT_ROLE_HEAL | BOT_ROLE_RANGED }, // Restoration Druid
        { BOT_CLASS_PRIEST,       BOT_SPEC_PRIEST_DISCIPLINE, BOT_ROLE_HEAL | BOT_ROLE_RANGED }, // Discipline Priest
        { BOT_CLASS_SHAMAN,       BOT_SPEC_SHAMAN_RESTORATION, BOT_ROLE_HEAL | BOT_ROLE_RANGED }, // Restoration Shaman
        { BOT_CLASS_PALADIN,      BOT_SPEC_PALADIN_HOLY, BOT_ROLE_HEAL | BOT_ROLE_RANGED }, // Holy Paladin
        { BOT_CLASS_DRUID,        BOT_SPEC_DRUID_RESTORATION, BOT_ROLE_HEAL | BOT_ROLE_RANGED }, // Restoration Druid

        // 28 DPS (14 Melee, 14 Ranged)
        { BOT_CLASS_ROGUE,        BOT_SPEC_ROGUE_ASSASINATION, BOT_ROLE_DPS },  // Assassination Rogue
        { BOT_CLASS_ROGUE,        BOT_SPEC_ROGUE_COMBAT, BOT_ROLE_DPS },  // Combat Rogue
        { BOT_CLASS_WARRIOR,      BOT_SPEC_WARRIOR_ARMS, BOT_ROLE_DPS },  // Arms Warrior
        { BOT_CLASS_WARRIOR,      BOT_SPEC_WARRIOR_FURY, BOT_ROLE_DPS },  // Fury Warrior
        { BOT_CLASS_PALADIN,      BOT_SPEC_PALADIN_RETRIBUTION, BOT_ROLE_DPS },  // Retribution Paladin
        { BOT_CLASS_PALADIN,      BOT_SPEC_PALADIN_RETRIBUTION, BOT_ROLE_DPS },  // Retribution Paladin
        { BOT_CLASS_DEATH_KNIGHT, BOT_SPEC_DK_UNHOLY, BOT_ROLE_DPS },  // Unholy DK
        { BOT_CLASS_DEATH_KNIGHT, BOT_SPEC_DK_FROST, BOT_ROLE_DPS },  // Frost DK
        { BOT_CLASS_SHAMAN,       BOT_SPEC_SHAMAN_ENHANCEMENT, BOT_ROLE_DPS },  // Enhancement Shaman
        { BOT_CLASS_DRUID,        BOT_SPEC_DRUID_FERAL, BOT_ROLE_DPS },  // Feral Cat
        { BOT_CLASS_ROGUE,        BOT_SPEC_ROGUE_SUBTLETY, BOT_ROLE_DPS },  // Subtlety Rogue
        { BOT_CLASS_ROGUE,        BOT_SPEC_ROGUE_ASSASINATION, BOT_ROLE_DPS },  // Assassination Rogue
        { BOT_CLASS_WARRIOR,      BOT_SPEC_WARRIOR_ARMS, BOT_ROLE_DPS },  // Arms Warrior
        { BOT_CLASS_DEATH_KNIGHT, BOT_SPEC_DK_UNHOLY, BOT_ROLE_DPS },  // Unholy DK

        { BOT_CLASS_MAGE,         BOT_SPEC_MAGE_ARCANE, BOT_ROLE_DPS | BOT_ROLE_RANGED }, // Arcane Mage
        { BOT_CLASS_MAGE,         BOT_SPEC_MAGE_FIRE, BOT_ROLE_DPS | BOT_ROLE_RANGED }, // Fire Mage
        { BOT_CLASS_MAGE,         BOT_SPEC_MAGE_FROST, BOT_ROLE_DPS | BOT_ROLE_RANGED }, // Frost Mage
        { BOT_CLASS_WARLOCK,      BOT_SPEC_WARLOCK_AFFLICTION, BOT_ROLE_DPS | BOT_ROLE_RANGED }, // Affliction Warlock
        { BOT_CLASS_WARLOCK,      BOT_SPEC_WARLOCK_DEMONOLOGY, BOT_ROLE_DPS | BOT_ROLE_RANGED }, // Demonology Warlock
        { BOT_CLASS_WARLOCK,      BOT_SPEC_WARLOCK_DESTRUCTION, BOT_ROLE_DPS | BOT_ROLE_RANGED }, // Destruction Warlock
        { BOT_CLASS_HUNTER,       BOT_SPEC_HUNTER_BEASTMASTERY, BOT_ROLE_DPS | BOT_ROLE_RANGED }, // Beast Mastery Hunter
        { BOT_CLASS_HUNTER,       BOT_SPEC_HUNTER_MARKSMANSHIP, BOT_ROLE_DPS | BOT_ROLE_RANGED }, // Marksmanship Hunter
        { BOT_CLASS_HUNTER,       BOT_SPEC_HUNTER_SURVIVAL, BOT_ROLE_DPS | BOT_ROLE_RANGED }, // Survival Hunter
        { BOT_CLASS_PRIEST,       BOT_SPEC_PRIEST_SHADOW, BOT_ROLE_DPS | BOT_ROLE_RANGED }, // Shadow Priest
        { BOT_CLASS_PRIEST,       BOT_SPEC_PRIEST_SHADOW, BOT_ROLE_DPS | BOT_ROLE_RANGED }, // Shadow Priest
        { BOT_CLASS_SHAMAN,       BOT_SPEC_SHAMAN_ELEMENTAL, BOT_ROLE_DPS | BOT_ROLE_RANGED }, // Elemental Shaman
        { BOT_CLASS_DRUID,        BOT_SPEC_DRUID_BALANCE, BOT_ROLE_DPS | BOT_ROLE_RANGED }, // Balance Druid
        { BOT_CLASS_MAGE,         BOT_SPEC_MAGE_FROST, BOT_ROLE_DPS | BOT_ROLE_RANGED }  // Frost Mage
    };

    static const BotRoleProfile DefenderRoster[5] = {
        { BOT_CLASS_WARRIOR, BOT_SPEC_WARRIOR_PROTECTION, BOT_ROLE_TANK },                     // Checkpoint Tank
        { BOT_CLASS_PRIEST,  BOT_SPEC_PRIEST_HOLY, BOT_ROLE_HEAL | BOT_ROLE_RANGED },                     // Checkpoint Healer
        { BOT_CLASS_MAGE,    BOT_SPEC_MAGE_FROST, BOT_ROLE_DPS | BOT_ROLE_RANGED },   // Checkpoint Ranged DPS
        { BOT_CLASS_ROGUE,   BOT_SPEC_ROGUE_COMBAT, BOT_ROLE_DPS },                     // Checkpoint Melee DPS
        { BOT_CLASS_HUNTER,  BOT_SPEC_HUNTER_MARKSMANSHIP, BOT_ROLE_DPS | BOT_ROLE_RANGED }    // Checkpoint Ranged DPS
    };

    static const CityRaidConfig CityConfigs[CITY_RAID_COUNT] = {
        // Stormwind
        {
            EVENT_CITY_RAID_STORMWIND,
            0,                      // Map 0 (Eastern Kingdoms)
            1519,                   // Stormwind City
            12,                     // Elwynn Forest
            BOSS_VARIAN_WRYNN,      // King Varian Wrynn (#29611)
            HORDE,                  // Attacker Team: Horde
            85,                     // Attacker Faction: 85 (Orgrimmar Grunt template)
            ALLIANCE,               // Defender Team: Alliance
            11,                     // Defender Faction: 11 (Stormwind Guard template)
            Position(-9151.98f, 410.944f, 92.697f, 1.57f), // DBC safe location 107: Stormwind gate GY (not Northshire)
            Position(-8430.0f, 620.0f, 95.5f, 0.0f),    // Stormwind Cathedral GY (Defender GY)
            {
                Position(-9115.0f, 423.0f, 96.0f, 1.0f),         // WP 0: Stormwind entrance (DBC safe location 1)
                Position(-8953.95f, 521.02f, 96.54f, 1.0f), // WP 1: Valley of Heroes Bridge
                Position(-8918.7f, 495.7f, 93.9f, 0.8f),    // WP 2: Valley of Heroes Court
                Position(-8847.28f, 569.96f, 94.77f, 0.5f), // WP 3: Trade District Arch
                Position(-8831.8f, 596.4f, 95.8f, 0.4f),    // WP 4: Trade District Center
                Position(-8750.0f, 560.0f, 97.0f, 6.0f),    // WP 5: Canal Bridge to Old Town
                Position(-8680.0f, 480.0f, 100.0f, 5.8f),   // WP 6: Old Town Main Street
                Position(-8604.27f, 396.53f, 108.05f, 5.5f),   // WP 7: Keep Ramp
                Position(-8534.0f, 367.13f, 108.57f, 5.3f), // WP 8: Stormwind Keep Courtyard
                Position(-8480.0f, 340.0f, 115.0f, 5.2f),   // WP 9: Throne Room Corridor
                Position(-8441.42f, 333.10f, 122.58f, 5.2f) // WP 10: Varian Wrynn Throne
            },
            {
                Position(-8953.95f, 521.02f, 96.54f, 4.14f), // CP 0: Valley of Heroes Bridge
                Position(-8831.8f, 596.4f, 95.8f, 3.54f),    // CP 1: Trade District Center
                Position(-8441.42f, 333.10f, 122.58f, 5.2f)  // CP 2: King Varian Wrynn Throne Room
            },
            "Stormwind City",
            "Horde",
            "Alliance",
            "King Varian Wrynn"
        },
        // Orgrimmar
        {
            EVENT_CITY_RAID_ORGRIMMAR,
            1,                      // Map 1 (Kalimdor)
            1637,                   // Orgrimmar
            14,                     // Durotar
            BOSS_THRALL,            // Thrall (#4949)
            ALLIANCE,               // Attacker Team: Alliance
            11,                     // Attacker Faction: 11 (Stormwind Guard template)
            HORDE,                  // Defender Team: Horde
            85,                     // Defender Faction: 85 (Orgrimmar Grunt template)
            Position(1178.15f, -4467.0f, 21.32f, 1.57f), // Northern Durotar GY (Attacker GY)
            Position(1935.78f, -4168.26f, 41.31f, 0.0f),    // Orgrimmar Graveyard (Defender GY)
            {
                Position(1352.45f, -4395.70f, 29.27f, 0.0f),  // WP 0: Durotar Gates Staging
                Position(1479.39f, -4406.25f, 25.55f, 0.0f),  // WP 1: Orgrimmar Front Gate
                Position(1537.4f, -4380.8f, 16.8f, 0.1f),     // WP 2: Valley of Strength Plaza
                Position(1607.92f, -4376.44f, 9.88f, 0.1f),   // WP 3: Valley of Strength Center
                Position(1750.38f, -4320.91f, 6.22f, 0.2f),   // WP 4: Tunnel to The Drag
                Position(1800.66f, -4374.51f, -17.12f, 4.8f), // WP 5: The Drag Center
                Position(1844.5f, -4400.1f, 4.7f, 4.5f),      // WP 6: Valley of Wisdom Junction
                Position(1905.66f, -4166.21f, 41.31f, 2.8f),  // WP 7: Valley of Wisdom Alley
                Position(1922.2f, -4204.4f, 42.3f, 2.5f),     // WP 8: Grommash Hold Courtyard
                Position(1912.25f, -4124.20f, 43.34f, 1.57f), // WP 9: Grommash Hold Entrance
                Position(1920.01f, -4123.95f, 43.60f, 1.57f)  // WP 10: Thrall Throne
            },
            {
                Position(1537.4f, -4380.8f, 16.8f, 3.14f),    // CP 0: Valley of Strength Plaza
                Position(1800.66f, -4374.51f, -17.12f, 1.57f), // CP 1: The Drag Center
                Position(1920.01f, -4123.95f, 43.60f, 1.57f)  // CP 2: Grommash Hold (Thrall Throne)
            },
            "Orgrimmar",
            "Alliance",
            "Horde",
            "Warchief Thrall"
        }
    };
}

namespace
{
namespace Tactics = NPCBots::CityRaid;
using State = Tactics::State;
constexpr uint32 RewardEntryBase = 990000;

struct RaidRuntime
{
    std::atomic<bool> Running{true};
    std::atomic<bool> BossKilled{false};
    std::atomic<uint32> PathTokens{0};
    // All controllers for this raid run serially on the same continent map.
    // Separate query storage avoids changing the core's global 1024-node budget.
    std::unique_ptr<dtNavMeshQuery> Navigation;
    bool TakePathToken()
    {
        uint32 n = PathTokens.load(std::memory_order_relaxed);
        while (n && !PathTokens.compare_exchange_weak(n, n-1, std::memory_order_relaxed)) { }
        return n != 0;
    }
};

struct RaidOrder
{
    State Mode = State::Spawning;
    Position Destination;
    ObjectGuid Focus;
    uint32 Frontline = 0;
    bool Engage = true;
    bool Taunt = true;
};

// Orders are published only by WorldScript::OnUpdate after MapManager's worker
// barrier. Controllers consume them on their creature's map thread. No map-thread
// callback mutates CityRaidInstance, Group, events or the prototype registry.
class CityRaidBotAI final : public CreatureAI
{
public:
    CityRaidBotAI(Creature* bot, CityRaidConfig const& cfg, CityRaidRole side, uint8 index,
        uint8 checkpoint, std::shared_ptr<RaidRuntime> runtime)
        : CreatureAI(bot), _cfg(cfg), _side(side), _index(index), _checkpoint(checkpoint), _runtime(std::move(runtime)),
          _think(37 * index % 500), _pathTimer(0) { }

    void Order(RaidOrder const& order) { _order = order; }
    void Reinforce() { _reinforcing = true; _cursor = 0; _pathTimer = 0; }
    bool Reinforcing() const { return _reinforcing; }
    bool Retreated() const { return _retreated; }
    uint32 GetData(uint32 id) const override
    {
        if (id == Tactics::FormationOwned) return _reinforcing || _order.Mode == State::Boss || (me->GetBotAI() && me->GetBotAI()->HasRole(BOT_ROLE_HEAL));
        if (id == Tactics::BossFormation) return _order.Mode == State::Boss ? 1 : 0;
        if (id == Tactics::AllowTaunt) return _order.Taunt ? 1 : 0;
        return 0;
    }
    ObjectGuid GetGUID(int32 id) const override { return id == Tactics::FocusTarget ? _order.Focus : ObjectGuid::Empty; }
    bool CanAIAttack(Unit const* target) const override
    {
        if (!_runtime->Running.load() || !me || !target || !target->IsInWorld() || !target->IsAlive() || target->IsCritter()
            || !me->IsInWorld() || target->GetMap() != me->GetMap() || target->IsFriendlyTo(me)) return false;
        if (_order.Mode == State::Retreating || _order.Mode == State::Celebrating || _order.Mode == State::Spawning) return false;
        // Reinforcements keep running toward the destination instead of starting fights
        // around the graveyard. Defenders engage immediately if near the boss or attacked.
        if (_reinforcing)
        {
            if (_side == ROLE_DEFENDER && (me->GetExactDist2d(&_cfg.CheckpointSpawns[2]) < 35.f || target->GetVictim() == me))
                const_cast<CityRaidBotAI*>(this)->_reinforcing = false;
            else
                return false;
        }
        if (!_order.Engage && target->GetVictim() != me) return false;
        if (_side == ROLE_DEFENDER)
        {
            if (target->GetVictim() && target->GetVictim()->GetEntry() == _cfg.BossEntry)
                return true;
            if (!_order.Focus.IsEmpty() && target->GetGUID() == _order.Focus)
                return true;
            return target->GetExactDist2d(&_order.Destination) <= 65.f;
        }
        if (_order.Mode == State::Boss) return target->GetExactDist2d(&_order.Destination) <= 65.f;
        size_t current=std::min<size_t>(_order.Frontline,_cfg.Waypoints.size()-1);
        size_t previous=current?current-1:0;
        auto const& a=_cfg.Waypoints[previous];
        auto const& b=_cfg.Waypoints[current];
        return Tactics::NearRouteSegment(target->GetPositionX(),target->GetPositionY(),
            a.GetPositionX(),a.GetPositionY(),b.GetPositionX(),b.GetPositionY(),60.f);
    }
    void KilledUnit(Unit* victim) override
    {
        if (_side == ROLE_ATTACKER && victim && victim->GetEntry() == _cfg.BossEntry && victim->GetMap() == me->GetMap())
            _runtime->BossKilled.store(true);
    }
    void UpdateAI(uint32 diff) override
    {
        if (!me || !me->IsInWorld() || !me->IsAlive() || me->GetMapId() != _cfg.MapId || !me->GetBotAI()) return;
        if (!_runtime->Running.load()) return;
        Tactics::Tick(_pathTimer, diff);
        if (!Tactics::Tick(_think, diff)) return;
        _think = 450 + 17 * _index % 100;
        bot_ai* native = me->GetBotAI();
        bool halted = _order.Mode == State::Retreating || _order.Mode == State::Celebrating;
        if (halted)
        {
            if (_lastMode != _order.Mode)
            {
                native->SetBotCommandState(BOT_COMMAND_FULLSTOP);
                me->CombatStop(true);
                me->GetThreatManager().ClearAllThreat();
                _cursor = _order.Frontline;
                if (_order.Mode==State::Retreating && _side==ROLE_ATTACKER)
                {
                    float nearest=std::numeric_limits<float>::max();
                    for (uint32 i=0;i<=_order.Frontline && i<_cfg.Waypoints.size();++i)
                        if (float distance=me->GetExactDist2d(&_cfg.Waypoints[i]);distance<nearest)
                        { nearest=distance; _cursor=i; }
                }
                if (_order.Mode == State::Celebrating) me->HandleEmoteCommand(EMOTE_ONESHOT_CHEER);
            }
        }
        else native->RemoveBotCommandState(BOT_COMMAND_FULLSTOP);
        _lastMode = _order.Mode;
        if (_order.Mode == State::Celebrating || _order.Mode == State::Spawning) return;

        // Drop a stale victim before it can overwrite the objective destination.
        // This also cancels an old city fight after resurrection at the gate.
        if (Unit* victim=me->GetVictim();victim && !CanAIAttack(victim))
        {
            me->AttackStop();
            me->CombatStop(true);
            me->GetThreatManager().ClearAllThreat();
            native->SetBotCommandState(BOT_COMMAND_COMBATRESET);
            _pathTimer=0;
        }
        Position goal = _order.Destination;
        if (_order.Mode == State::Retreating)
        {
            me->CombatStop(true);
            goal = _side == ROLE_ATTACKER ? _cfg.Waypoints[std::min<size_t>(_cursor, _cfg.Waypoints.size()-1)] : _cfg.DefenderGraveyardPos;
            if (me->GetExactDist2d(&goal) < 8.f)
            {
                if (_side == ROLE_ATTACKER && _cursor) --_cursor;
                else _retreated = true;
            }
            Move(goal);
            return;
        }
        if (_reinforcing)
        {
            if (_side == ROLE_ATTACKER)
            {
                _cursor = std::min(_cursor, _order.Frontline);
                goal = _cfg.Waypoints[std::min<size_t>(_cursor, _cfg.Waypoints.size()-1)];
                if (me->GetExactDist2d(&goal) < 8.f)
                {
                    if (_cursor < _order.Frontline) ++_cursor;
                    else _reinforcing = false;
                }
            }
            else
            {
                // Defenders reinforce directly to the City Boss (Thrall / King Varian Wrynn)!
                goal = _cfg.CheckpointSpawns[2];
                if (me->GetExactDist2d(&goal) < 15.f) _reinforcing = false;
            }
        }
        if (me->HasUnitState(UNIT_STATE_CONTROLLED | UNIT_STATE_ROOT | UNIT_STATE_CASTING)) return;
        Unit* focus = _order.Focus.IsEmpty() ? nullptr : ObjectAccessor::GetUnit(*me, _order.Focus);
        if (focus && CanAIAttack(focus) && native->CanBotAttack(focus) && me->IsWithinLOSInMap(focus))
        {
            if (me->GetVictim() != focus) native->AttackStart(focus);
            if (_order.Mode == State::Boss && _order.Taunt && _index < 4 && focus->GetVictim() != me)
            {
                uint32 taunt = native->GetBotClass() == BOT_CLASS_WARRIOR ? 355 : native->GetBotClass() == BOT_CLASS_PALADIN ? 62124
                    : native->GetBotClass() == BOT_CLASS_DEATH_KNIGHT ? 56222 : 6795;
                native->TryCityRaidSpell(taunt, focus);
            }
        }
        if (_order.Mode != State::Boss && !_reinforcing)
        {
            Unit* victim = me->GetVictim();
            if (victim && victim->IsAlive() && victim->GetMap() == me->GetMap())
            {
                if (native->HasRole(BOT_ROLE_HEAL))
                {
                    // Native DPS keeps its combat positioning; healers keep a safe casting distance.
                    if (me->GetExactDist2d(victim) < 13.f)
                    {
                        float a = victim->GetAbsoluteAngle(me);
                        goal.Relocate(victim->GetPositionX()+20.f*std::cos(a),victim->GetPositionY()+20.f*std::sin(a),me->GetPositionZ());
                    }
                    Move(goal);
                }
                else if (!me->IsWithinCombatRange(victim,35.f)) Move(victim->GetPosition());
                return;
            }
        }
        Move(goal);
        if (focus && focus->IsAlive() && !me->isMoving()) me->SetFacingToObject(focus);
    }
private:
    void Move(Position goal)
    {
        if (_retreated || _pathTimer || me->HasUnitState(UNIT_STATE_NOT_MOVE) || me->IsMovementPreventedByCasting()) return;
        if (me->GetExactDist2d(&goal) < 2.5f) return;
        if (me->isMoving() && _hasGoal && goal.GetExactDist2d(&_lastGoal) < 3.f) return;
        if (_hasGoal && _goalComplete && goal.GetExactDist2d(&_lastGoal)<3.f && me->GetExactDist2d(&_acceptedGoal)<2.f) return;
        _pathTimer = 500 + 17 * _index % 250;
        Unit* focus=_order.Mode==State::Boss && !_reinforcing && !_order.Focus.IsEmpty()
            ? ObjectAccessor::GetUnit(*me,_order.Focus) : nullptr;
        for (uint32 attempt=0; attempt<3; ++attempt)
        {
            if (attempt && !focus) break;
            if (!_runtime->TakePathToken()) return;
            Position candidate=goal;
            if (attempt && focus)
            {
                float radius=focus->GetExactDist2d(&goal);
                float angle=focus->GetAbsoluteAngle(&goal)+(attempt==1?0.25f:-0.25f);
                if (radius>14.f) radius=std::max(15.f,radius*0.9f);
                candidate.m_positionX=focus->GetPositionX()+radius*std::cos(angle);
                candidate.m_positionY=focus->GetPositionY()+radius*std::sin(angle);
            }
            me->UpdateAllowedPositionZ(candidate.m_positionX,candidate.m_positionY,candidate.m_positionZ);
            PathGenerator path(me);
            // Detour corridor corners, not raycasts: long graveyard routes must
            // not overflow the 74-point smooth-path buffer into BuildShortcut().
            path.SetUseStraightPath(true);
            if (!path.SetNavMeshQuery(_runtime->Navigation.get())) continue;
            if (!path.CalculatePath(candidate.GetPositionX(),candidate.GetPositionY(),candidate.GetPositionZ(),false)) continue;
            uint32 type=uint32(path.GetPathType());
            if (!(type & (PATHFIND_NORMAL|PATHFIND_INCOMPLETE)) || (type & (PATHFIND_NOPATH|PATHFIND_SHORTCUT|PATHFIND_NOT_USING_PATH|PATHFIND_SHORT)) || path.GetPath().size()<2) continue;
            auto const& end=path.GetPath().back();
            if (focus && !(type & PATHFIND_INCOMPLETE) && !me->GetMap()->isInLineOfSight(end.x,end.y,end.z+2.f,
                focus->GetPositionX(),focus->GetPositionY(),focus->GetPositionZ()+2.f,me->GetPhaseMask(),LINEOFSIGHT_ALL_CHECKS,VMAP::ModelIgnoreFlags::Nothing)) continue;
            me->GetMotionMaster()->Clear(MOTION_PRIORITY_NORMAL);
            Movement::MoveSplineInit init(me); init.MovebyPath(path.GetPath()); init.SetWalk(false); init.Launch();
            _lastGoal=goal; _acceptedGoal.Relocate(end.x,end.y,end.z); _hasGoal=true;
            _goalComplete=!(type & PATHFIND_INCOMPLETE) && candidate.GetExactDist2d(&_acceptedGoal)<4.f;
            return;
        }
    }
    CityRaidConfig const& _cfg;
    CityRaidRole _side;
    uint8 _index, _checkpoint;
    std::shared_ptr<RaidRuntime> _runtime;
    RaidOrder _order;
    State _lastMode = State::Idle;
    uint32 _think, _pathTimer, _cursor = 0;
    bool _reinforcing = false, _retreated = false, _hasGoal = false, _goalComplete = false;
    Position _lastGoal, _acceptedGoal;
};

struct RewardRequest { CityRaidType Raid; ObjectGuid Chest, PlayerGuid; };
void QueueReward(RewardRequest request);

class CityRaidInstance
{
public:
    explicit CityRaidInstance(CityRaidType type) : _cfg(CityRaidConstants::CityConfigs[type]), _type(type) { }
    bool Active() const { return _state != State::Idle; }
    bool OffCooldown() const { return !_cooldown; }
    bool Manual() const { return _manual; }
    void MakeManual() { _manual = true; }
    void Start(bool manual)
    {
        if (Active()) { _manual |= manual; return; }
        Map* map = sMapMgr->CreateBaseMap(_cfg.MapId);
        if (!map || !BotDataMgr::AllBotsLoaded() || !sFactionTemplateStore.LookupEntry(_cfg.AttackerFaction)
            || !sFactionTemplateStore.LookupEntry(_cfg.DefenderFaction)) return;
        _graveyards = {_cfg.GraveyardPos,_cfg.DefenderGraveyardPos};
        for (Position& pos : _graveyards)
        {
            map->LoadGrid(pos.GetPositionX(),pos.GetPositionY());
            float z=map->GetHeight(PHASEMASK_NORMAL,pos.GetPositionX(),pos.GetPositionY(),pos.GetPositionZ()+10.f,true,40.f);
            if (z<=INVALID_HEIGHT)
            {
                TC_LOG_ERROR("npcbots","City raid {} graveyard has no valid ground",_cfg.EventId);
                return;
            }
            pos.m_positionZ=z;
        }
        auto navigation=std::make_unique<dtNavMeshQuery>();
        auto* mesh=MMAP::MMapFactory::createOrGetMMapManager()->GetNavMesh(_cfg.MapId);
        if (!mesh || dtStatusFailed(navigation->init(mesh,Tactics::QueryNodes)))
        {
            TC_LOG_ERROR("npcbots","City raid {} requires a valid navigation mesh",_cfg.EventId);
            return;
        }
        _runtime = std::make_shared<RaidRuntime>();
        _runtime->Navigation=std::move(navigation);
        _state = State::Spawning; _manual = manual; _wp = 0; _spawn = 0; _elapsed = 0; _decision = 0; _sinceDecision=0;
        _duration = uint32(std::clamp(sConfigMgr->GetIntDefault("CityRaid.Duration",1800),60,7200))*1000;
        _attackCount = Tactics::AttackerCount; // fixed 40-player attacking raid; defenders are additional
        if (sConfigMgr->GetIntDefault("CityRaid.AttackerCount",40)!=int32(_attackCount))
            TC_LOG_WARN("npcbots","CityRaid.AttackerCount is fixed at {}; configured value ignored",_attackCount);
        _defendCount = uint32(std::clamp(sConfigMgr->GetIntDefault("CityRaid.DefenderCount",15),0,15));
        _attackers.clear(); _defenders.clear(); _participants.clear(); _eligible.clear(); _claimed.clear();
        _bossAnchor=_cfg.Waypoints.back(); _aggroReadyAt=0;
        _boss.Clear(); _tank.Clear(); _lastVictim.Clear(); _bossSeenAlive = _bossEngaged = false; _lastTankHealth = 100.f; _swapAt = 0;
        _attackWave = 20000; _defendWave = 30000; _sampleAt = 1000;
    }
    void Stop()
    {
        TC_LOG_INFO("npcbots","City raid {} stopping state={} waypoint={} elapsed_ms={} attackers={} defenders={}",
            _cfg.EventId,uint32(_state),_wp,_elapsed,_attackers.size(),_defenders.size());
        if (_manual && sGameEventMgr->IsActiveEvent(_cfg.EventId)) sGameEventMgr->StopEvent(_cfg.EventId,true);
        if (_runtime) _runtime->Running.store(false);
        // Disband before queuing any bot deletion: no dangling GroupBotReference.
        for (ObjectGuid& guid : _groups)
        {
            if (!guid.IsEmpty()) if (Group* group=sGroupMgr->GetGroupByGUID(guid)) group->Disband();
            guid.Clear();
        }
        for (auto const& m : _attackers) BotDataMgr::DespawnCityRaidBot(m.Entry);
        for (auto const& m : _defenders) BotDataMgr::DespawnCityRaidBot(m.Entry);
        _attackers.clear(); _defenders.clear();
        if (Map* map = GetMap()) if (GameObject* go = map->GetGameObject(_chest)) go->Delete();
        _chest.Clear(); _eligible.clear(); _claimed.clear(); _runtime.reset(); _state = State::Idle; _manual = false;
        _cooldown = uint32(std::clamp(sConfigMgr->GetIntDefault("CityRaid.Cooldown",7200),60,86400))*1000;
    }
    void Update(uint32 diff)
    {
        Tactics::Tick(_cooldown,diff); // also while idle
        if (!Active()) return;
        _elapsed += diff;
        _sinceDecision += diff;
        if (_state == State::Celebrating)
        {
            if (Tactics::Tick(_finish,diff)) Stop();
            return;
        }
        if (_state == State::Retreating)
        {
            if (Tactics::Tick(_finish,diff)) { Stop(); return; }
        }
        else if (Tactics::Tick(_duration,diff)) BeginRetreat();
        if (!Tactics::Tick(_decision,diff)) return;
        uint32 decisionDiff = _sinceDecision; _sinceDecision=0;
        _decision = Tactics::DecisionMs;
        Map* map = GetMap();
        if (!map) { Stop(); return; }
        _runtime->PathTokens.store(Tactics::PathBudget);
        if (_state == State::Spawning) { SpawnBatch(map); return; }
        if (_state == State::Retreating)
        {
            bool finished = true;
            for (auto const& m : _attackers) if (Creature* b=GetBot(map,m)) if (b->IsAlive()) if (auto* ai=AI(b)) finished &= ai->Retreated();
            if (finished) Stop();
            return;
        }
        bool attackerWave = Tactics::WaveTick(_attackWave,20000,decisionDiff);
        bool defenderWave = Tactics::WaveTick(_defendWave,30000,decisionDiff);
        Resurrect(map,_attackers,attackerWave,_graveyards[0]);
        Resurrect(map,_defenders,defenderWave,_graveyards[1]);
        if (_state == State::Marching || _state == State::Boss)
        {
            uint32 activeAttackersInCity = 0;
            for (auto const& m : _attackers)
            {
                if (Creature* b = GetBot(map, m))
                {
                    if (b->IsAlive() && b->GetExactDist2d(&_graveyards[0]) > 50.f)
                        ++activeAttackersInCity;
                }
            }

            if (activeAttackersInCity == 0)
            {
                TC_LOG_INFO("npcbots", "City raid {}: All attacking forces in {} defeated! Raid failed.", _cfg.EventId, _cfg.CityName);
                BeginRetreat();
                return;
            }
        }
        if (_elapsed >= _sampleAt) { _sampleAt = _elapsed+1000; SampleParticipants(map); }
        if (_state == State::Boss) BossOrders(map);
        else MarchOrders(map);
        DefenderOrders(map);
    }
    void Claim(Player* player, ObjectGuid chest)
    {
        if (!player || !player->IsInWorld() || !player->GetSession() || _state != State::Celebrating || chest != _chest
            || !_eligible.count(player->GetGUID()) || _claimed.count(player->GetGUID()) || player->GetMapId() != _cfg.MapId) return;
        Map* map = GetMap(); GameObject* go = map ? map->GetGameObject(chest) : nullptr;
        if (!go || !player->IsWithinDistInMap(go,8.f)) return;
        uint32 item = 47241; // Emblem of Triumph (level-80 currency)
        uint32 count = uint32(std::clamp(sConfigMgr->GetIntDefault("CityRaid.Reward.Emblems",2),0,10));
        uint32 money = uint32(std::clamp(sConfigMgr->GetIntDefault("CityRaid.Reward.Gold",10),0,100))*10000;
        if (uint64(player->GetMoney())+money > MAX_MONEY_AMOUNT) return;
        ItemPosCountVec dest;
        if (count)
        {
            InventoryResult result = player->CanStoreNewItem(NULL_BAG,NULL_SLOT,dest,item,count);
            if (result != EQUIP_ERR_OK) { player->SendEquipError(result,nullptr,nullptr,item); return; }
            if (!player->StoreNewItem(dest,item,count,true)) return;
        }
        player->ModifyMoney(int32(money));
        _claimed.insert(player->GetGUID()); // world-thread serialized; duplicate queued clicks are harmless
        player->SaveToDB(); // normal character inventory + money persistence
        player->GetSession()->SendNotification("City raid reward: %u emblem(s), %u gold.",count,money/10000);
    }
private:
    Map* GetMap() const { return sMapMgr->FindBaseNonInstanceMap(_cfg.MapId); }
    Creature* GetBot(Map* map, CityRaidBotMember const& m) const
    {
        Creature* bot = map ? map->GetCreature(m.Guid) : nullptr;
        return bot && bot->IsInWorld() && bot->GetMap() == map && bot->GetBotAI() ? bot : nullptr;
    }
    static CityRaidBotAI* AI(Creature* bot)
    {
        return bot && bot->GetBotAI() ? dynamic_cast<CityRaidBotAI*>(bot->GetBotAI()->GetCityRaidAI()) : nullptr;
    }
    void Publish(Creature* bot, RaidOrder const& order) { if (auto* ai = AI(bot)) ai->Order(order); }
    bool VerifyRoster(Map* map) const
    {
        auto verify=[this,map](std::vector<CityRaidBotMember> const& members,uint32 expected,ObjectGuid guid)
        {
            if (members.size()!=expected) return false;
            Group* group=guid.IsEmpty()?nullptr:sGroupMgr->GetGroupByGUID(guid);
            if (!expected) return !group;
            if (!group || group->GetMembersCount()!=expected) return false;
            std::set<ObjectGuid> unique;
            for (auto const& member:members)
            {
                Creature* bot=GetBot(map,member);
                if (!bot || !AI(bot) || bot->GetBotGroup()!=group || !group->IsMember(member.Guid)
                    || !unique.insert(member.Guid).second) return false;
            }
            return true;
        };
        return verify(_attackers,Tactics::AttackerCount,_groups[0]) && verify(_defenders,_defendCount,_groups[1]);
    }
    void SpawnBatch(Map* map)
    {
        for (uint32 n=0; n<2 && _spawn<_attackCount+_defendCount; ++n,++_spawn)
        {
            bool attack = _spawn<_attackCount;
            uint32 index = attack ? _spawn : _spawn-_attackCount;
            uint8 cp = uint8(index%3), slot = uint8(index/3);
            auto const& profile = attack ? CityRaidConstants::AttackerRoster[index] : CityRaidConstants::DefenderRoster[slot];
            Position p = attack ? _cfg.Waypoints.front() : _cfg.CheckpointSpawns[cp];
            float a = float(index)*2.399963f;
            p.m_positionX += (3.f+float(index%4)*2.f)*std::cos(a);
            p.m_positionY += (3.f+float(index%4)*2.f)*std::sin(a);
            map->LoadGrid(p.GetPositionX(),p.GetPositionY());
            float z = map->GetHeight(PHASEMASK_NORMAL,p.GetPositionX(),p.GetPositionY(),p.GetPositionZ()+10.f,true,40.f);
            if (z>INVALID_HEIGHT) p.m_positionZ = z;
            uint32 roles = profile.roles | BOT_ROLE_PARTY;
            if (roles & BOT_ROLE_TANK) roles |= BOT_ROLE_DPS;
            uint32 entry = BotDataMgr::SpawnCityRaidBot(80,_cfg.MapId,p,attack?_cfg.AttackerTeam:_cfg.DefenderTeam,
                attack?_cfg.AttackerFaction:_cfg.DefenderFaction,profile.botClass,profile.spec,roles);
            Creature* bot = entry ? const_cast<Creature*>(BotDataMgr::FindBot(entry)) : nullptr;
            if (!bot || !bot->IsInWorld() || bot->GetMap()!=map || !bot->GetBotAI())
            {
                if (entry) BotDataMgr::DespawnCityRaidBot(entry);
                TC_LOG_ERROR("npcbots", "City raid {} could not generate role slot {}",_cfg.EventId,_spawn);
                Stop(); return; // rollback a partial raid, never substitute arbitrary classes
            }
            bot->GetBotAI()->SetCityRaidAI(std::make_unique<CityRaidBotAI>(bot,_cfg,attack?ROLE_ATTACKER:ROLE_DEFENDER,uint8(index),cp,_runtime));
            bot->GetBotAI()->SetWanderer(); bot->GetBotAI()->SetBotCommandState(BOT_COMMAND_NOGOSSIP);
            bot->SetPvP(true); bot->RemoveByteFlag(UNIT_FIELD_BYTES_2,1,UNIT_BYTE2_FLAG_FFA_PVP);
            bot->setActive(true); bot->SetReactState(REACT_AGGRESSIVE);
            (attack?_attackers:_defenders).push_back({bot->GetGUID(),entry,uint8(attack?ROLE_ATTACKER:ROLE_DEFENDER),uint8(index),cp,false});
            ObjectGuid& groupGuid=_groups[attack?0:1];
            Group* group=groupGuid.IsEmpty()?nullptr:sGroupMgr->GetGroupByGUID(groupGuid);
            if (!group)
            {
                group=new Group;
                if (!group->Create(bot,true)) { delete group; Stop(); return; }
                sGroupMgr->AddGroup(group); groupGuid=group->GetGUID();
            }
            else if (!group->AddMember(bot)) { Stop(); return; }
        }
        if (_spawn>=_attackCount+_defendCount)
        {
            if (!VerifyRoster(map))
            {
                TC_LOG_ERROR("npcbots","City raid {} aborted: incomplete roster (attackers={}/40, defenders={}/{})",
                    _cfg.EventId,_attackers.size(),_defenders.size(),_defendCount);
                Stop(); return;
            }
            TC_LOG_INFO("npcbots","City raid {} assembled: attackers={} defenders={} total={}",
                _cfg.EventId,_attackers.size(),_defenders.size(),_attackers.size()+_defenders.size());
            // Redistribution is atomic with respect to map updates: native party
            // buffs now reach one healer per subgroup and each checkpoint team.
            for (auto* roster:{&_attackers,&_defenders})
                for (auto const& member:*roster)
                    if (Creature* bot=GetBot(map,member))
                        if (Group* group=bot->GetBotGroup())
                            group->ChangeMembersGroup(member.Guid,roster==&_attackers?uint8(Tactics::AttackerSubgroup(member.Index)):member.Checkpoint);
            _state = State::Marching;
            Announce("The invaders are advancing on "+std::string(_cfg.CityName)+"!");
        }
    }
    void Resurrect(Map* map, std::vector<CityRaidBotMember>& members, bool wave, Position const& graveyard)
    {
        uint32 revived=0;
        for (auto& m : members)
        {
            Creature* bot = GetBot(map,m); if (!bot) continue;
            if (!bot->IsAlive())
            {
                if (!m.IsDead)
                {
                    m.IsDead=true; BotDataMgr::SpawnBotBones(bot);
                    bot->CombatStop(true);
                    bot->GetThreatManager().ClearAllThreat();
                    bot->NearTeleportTo(graveyard.GetPositionX(),graveyard.GetPositionY(),graveyard.GetPositionZ(),graveyard.GetOrientation());
                }
                if (!wave) continue;
                bot->NearTeleportTo(graveyard.GetPositionX(),graveyard.GetPositionY(),graveyard.GetPositionZ(),graveyard.GetOrientation());
                BotMgr::ReviveBot(bot);
                bot->SetFullHealth();
                if (bot->GetMaxPower(POWER_MANA)>1) bot->SetPower(POWER_MANA,bot->GetMaxPower(POWER_MANA));
                bot->CombatStop(true);
                bot->GetThreatManager().ClearAllThreat();
                if (&members == &_attackers)
                {
                    // Revived attackers stay at the graveyard and do NOT march back into the city piecemeal
                    if (auto* ai=AI(bot))
                        ai->Order({State::Idle, graveyard, ObjectGuid::Empty, _wp, false, false});
                }
                else
                {
                    // Revived defenders rush back to the city boss to reinforce the defense!
                    if (auto* ai=AI(bot))
                        ai->Reinforce();
                }
                ++revived;
            }
            m.IsDead=false;
        }
        if (revived) TC_LOG_INFO("npcbots","City raid {} reinforcing side={} count={} waypoint={} graveyard_x={} graveyard_y={}",
            _cfg.EventId,&members==&_attackers?"attackers":"defenders",revived,_wp,graveyard.GetPositionX(),graveyard.GetPositionY());
    }
    Creature* Leader(Map* map)
    {
        for (auto const& m:_attackers)
            if (Creature* b=GetBot(map,m)) if (b->IsAlive() && AI(b) && !AI(b)->Reinforcing()) return b;
        return nullptr;
    }
    void MarchOrders(Map* map)
    {
        Creature* leader = Leader(map); if (!leader) return;
        if (_wp>=8)
            if (Creature* boss=leader->FindNearestCreature(_cfg.BossEntry,55.f,true))
            { _boss=boss->GetGUID(); _wp=uint32(_cfg.Waypoints.size()-1); _state=State::Boss; BossOrders(map); return; }

        float distToWp = leader->GetExactDist2d(&_cfg.Waypoints[_wp]);
        bool advance = distToWp < 14.f;
        if (!advance && _wp + 1 < _cfg.Waypoints.size())
        {
            float distToNext = leader->GetExactDist2d(&_cfg.Waypoints[_wp + 1]);
            if (distToNext < distToWp && distToNext < 30.f)
                advance = true;
        }

        if (advance)
        {
            if (_wp+1<_cfg.Waypoints.size())
            {
                ++_wp;
                TC_LOG_INFO("npcbots","City raid {} advancing waypoint={}/{} leader={} x={} y={} z={}",
                    _cfg.EventId,_wp,_cfg.Waypoints.size()-1,leader->GetEntry(),leader->GetPositionX(),leader->GetPositionY(),leader->GetPositionZ());
                if (_wp==1 || _wp==4 || _wp==8 || _wp==10) Announce("The invaders have reached checkpoint "+std::to_string(_wp)+" in "+_cfg.CityName+"!");
            }
            else { _state=State::Boss; return; }
        }
        Unit* enemy=nullptr;
        for (auto const& m:_attackers) if (Creature* b=GetBot(map,m))
            if (b->IsAlive() && b->GetVictim() && b->GetVictim()->IsAlive() && b->GetVictim()->GetMap()==map)
                { enemy=b->GetVictim(); break; }

        float marchAngle = leader->GetAbsoluteAngle(&_cfg.Waypoints[_wp]);
        float backAngle = Position::NormalizeOrientation(marchAngle + float(M_PI));

        for (auto const& m:_attackers) if (Creature* b=GetBot(map,m))
        {
            if (!b->IsAlive()) continue;
            Position goal=_cfg.Waypoints[_wp];
            auto role=Tactics::AttackerRole(m.Index);
            if (b!=leader)
            {
                goal=leader->GetPosition();
                float behind=role==Tactics::Role::Healer?12.f:role==Tactics::Role::Ranged?8.f:3.f;
                float side=(float(m.Index%5)-2.f)*2.5f;
                goal.m_positionX+=behind*std::cos(backAngle)+side*std::sin(backAngle);
                goal.m_positionY+=behind*std::sin(backAngle)-side*std::cos(backAngle);
            }
            ObjectGuid focus = enemy && b->GetExactDist2d(enemy)<40.f ? enemy->GetGUID():ObjectGuid::Empty;
            Publish(b,{State::Marching,goal,focus,_wp,role!=Tactics::Role::Healer,true});
        }
    }
    void DefenderOrders(Map* map)
    {
        Creature* boss = _boss.IsEmpty() ? nullptr : map->GetCreature(_boss);
        if (!boss)
        {
            for (auto const& m : _defenders)
            {
                if (m.Checkpoint == 2)
                {
                    if (Creature* b = GetBot(map, m))
                    {
                        if (Creature* foundBoss = b->FindNearestCreature(_cfg.BossEntry, 80.f, true))
                        {
                            boss = foundBoss;
                            _boss = boss->GetGUID();
                            break;
                        }
                    }
                }
            }
        }

        Unit* bossThreat = nullptr;
        if (boss && boss->IsAlive())
        {
            if (Unit* victim = boss->GetVictim())
                if (victim->IsAlive() && victim->GetMap() == map)
                    bossThreat = victim;

            if (!bossThreat)
            {
                float closestDist = 50.f;
                for (auto const& m : _attackers)
                {
                    if (Creature* b = GetBot(map, m))
                    {
                        if (b->IsAlive() && b->GetMap() == map && b->GetExactDist2d(boss) < closestDist)
                        {
                            closestDist = b->GetExactDist2d(boss);
                            bossThreat = b;
                        }
                    }
                }
            }
        }

        for (auto const& m : _defenders) if (Creature* b = GetBot(map, m)) if (b->IsAlive())
        {
            bool defendBoss = (_state == State::Boss || _wp >= 8 || m.Checkpoint == 2);

            Position goal;
            if (defendBoss)
            {
                goal = boss ? boss->GetPosition() : _cfg.CheckpointSpawns[2];
                float a = float(m.Index) * 2.399963f;
                float r = (b->GetBotAI() && b->GetBotAI()->HasRole(BOT_ROLE_HEAL)) ? 14.f :
                          (b->GetBotAI() && b->GetBotAI()->HasRole(BOT_ROLE_RANGED)) ? 9.f : 3.5f;
                goal.m_positionX += r * std::cos(a);
                goal.m_positionY += r * std::sin(a);
            }
            else
            {
                uint8 targetCp = m.Checkpoint;
                if (_wp >= 6 && targetCp < 2) targetCp = 2;
                else if (_wp >= 3 && targetCp < 1) targetCp = 1;

                goal = _cfg.CheckpointSpawns[targetCp];
                float a = float(m.Index) * 2.399963f;
                goal.m_positionX += 4.f * std::cos(a);
                goal.m_positionY += 4.f * std::sin(a);
            }

            ObjectGuid focus = bossThreat ? bossThreat->GetGUID() : ObjectGuid::Empty;
            Publish(b, {defendBoss ? State::Boss : State::Marching, goal, focus, _wp, !b->GetBotAI()->HasRole(BOT_ROLE_HEAL), true});
        }
    }
    void BossOrders(Map* map)
    {
        Creature* leader=Leader(map);
        Creature* boss=_boss.IsEmpty()?nullptr:map->GetCreature(_boss);
        if (!boss && leader) boss=leader->FindNearestCreature(_cfg.BossEntry,120.f,false);
        if (!boss && leader) boss=leader->FindNearestCreature(_cfg.BossEntry,120.f,true);
        if (boss)
        {
            _boss=boss->GetGUID();
            if (!_bossSeenAlive && _tank.IsEmpty())
            {
                _bossAnchor=boss->GetPosition();
                _facing=_cfg.Waypoints[_cfg.Waypoints.size()-2].GetAbsoluteAngle(boss);
            }
        }
        if (Tactics::VictoryEligible(_runtime->BossKilled.load(),boss && !boss->IsAlive(),_bossSeenAlive,_bossEngaged)) { Victory(map); return; }
        if (!boss || !boss->IsAlive()) return; // A pre-dead/missing city leader is never a victory.
        _bossSeenAlive=true;
        if (_tank.IsEmpty())
        {
            _bossAnchor=boss->GetPosition();
            _facing=_cfg.Waypoints[_cfg.Waypoints.size()-2].GetAbsoluteAngle(boss);
        }
        Unit* current=boss->GetVictim();
        Creature* selected=nullptr;
        if (current && current->IsPlayer() && current->GetFaction()!=boss->GetFaction())
            _tank=current->GetGUID(); // Respect a participating human tank.
        else
        {
            float best=-1.f;
            for (auto const& m:_attackers) if (m.Index<4) if (Creature* b=GetBot(map,m))
                if (b->IsAlive() && b!=current && AI(b) && !AI(b)->Reinforcing() && b->GetHealthPct()>best)
                    { best=b->GetHealthPct(); selected=b; }
            Unit* planned=ObjectAccessor::GetUnit(*boss,_tank);
            if (!planned || !planned->IsAlive())
            {
                bool existingTank=current && current->IsNPCBot() && current->ToCreature()->GetBotAI()
                    && current->ToCreature()->GetBotAI()->IsCityRaidBot() && current->ToCreature()->GetBotAI()->HasRole(BOT_ROLE_TANK);
                _tank=existingTank?current->GetGUID():selected?selected->GetGUID():ObjectGuid::Empty;
            }
            if (current && current->IsAlive() && _elapsed>=_swapAt && selected && selected!=current)
            {
                bool mortal=false;
                for (AuraEffect const* effect:current->GetAuraEffectsByType(SPELL_AURA_MOD_HEALING_PCT)) mortal|=effect->GetAmount()<=-30;
                float burst=current->GetGUID()==_lastVictim?std::max(0.f,_lastTankHealth-current->GetHealthPct()):0.f;
                if (Tactics::ShouldSwap(current->GetHealthPct(),burst,mortal,selected->GetHealthPct()))
                {
                    _tank=selected->GetGUID(); _swapAt=_elapsed+8000;
                    TC_LOG_INFO("npcbots","City raid {} requests tank swap to {}",_cfg.EventId,selected->GetName());
                }
            }
        }
        _lastTankHealth=current?current->GetHealthPct():100.f;
        _lastVictim=current?current->GetGUID():ObjectGuid::Empty;
        bool holding=current && (current->GetGUID()==_tank || current->IsPlayer());
        _bossEngaged |= holding;
        if (!holding) _aggroReadyAt=0;
        else if (!_aggroReadyAt) _aggroReadyAt=_elapsed+2000;
        bool threatReady=holding && _elapsed>=_aggroReadyAt && current
            && (current->IsPlayer() || boss->GetThreatManager().GetThreat(current)>=100.f);
        bool whirlwind=boss->HasAura(41056) || (boss->GetCurrentSpell(CURRENT_CHANNELED_SPELL)
            && boss->GetCurrentSpell(CURRENT_CHANNELED_SPELL)->GetSpellInfo()->Id==41056);
        float melee=std::max(3.f,boss->GetCombatReach()+1.f);
        for (auto const& m:_attackers) if (Creature* b=GetBot(map,m)) if (b->IsAlive())
        {
            bool main=b->GetGUID()==_tank;
            auto role=Tactics::AttackerRole(m.Index);
            auto off=Tactics::BossOffset(role,m.Index<12?m.Index:(m.Index<26?m.Index-12:m.Index-26),main,melee,whirlwind);
            Position goal=main?_bossAnchor:boss->GetPosition();
            float facing=main?_facing:boss->GetOrientation();
            goal.m_positionX+=off.X*std::cos(facing)-off.Y*std::sin(facing);
            goal.m_positionY+=off.X*std::sin(facing)+off.Y*std::cos(facing);
            bool engage=main || (threatReady && role!=Tactics::Role::Healer && !(whirlwind && role==Tactics::Role::Melee));
            Publish(b,{State::Boss,goal,boss->GetGUID(),_wp,engage,main});
        }
    }
    void SampleParticipants(Map* map)
    {
        Position center=_state==State::Boss?_cfg.Waypoints.back():_cfg.Waypoints[_wp];
        bool fighting=false;
        for (auto const& m:_attackers) if (Creature* b=GetBot(map,m)) fighting|=b->IsAlive()&&b->IsInCombat();
        for (auto const& ref:map->GetPlayers()) if (Player* player=ref.GetSource())
        {
            bool engaged=fighting && player->IsAlive() && player->IsInCombat() && !player->IsGameMaster()
                && player->GetTeam()==_cfg.AttackerTeam && player->GetExactDist2d(&center)<80.f;
            if (engaged) _participants[player->GetGUID()]=Tactics::CombatCredit(_participants[player->GetGUID()],true);
        }
    }
    void Victory(Map* map)
    {
        if (_state!=State::Boss) return;
        _state=State::Celebrating;
        _finish=uint32(std::clamp(sConfigMgr->GetIntDefault("CityRaid.Reward.Seconds",180),30,600))*1000;
        for (auto const& ref:map->GetPlayers()) if (Player* p=ref.GetSource())
            if (Tactics::Eligible(_participants[p->GetGUID()],p->GetLevel()>=80,p->GetTeam()==_cfg.AttackerTeam,
                p->GetExactDist2d(&_cfg.Waypoints.back())<120.f)) _eligible.insert(p->GetGUID());
        Position pos=_bossAnchor;
        pos.m_positionX-=8.f*std::cos(_facing); pos.m_positionY-=8.f*std::sin(_facing);
        float floor=map->GetHeight(PHASEMASK_NORMAL,pos.GetPositionX(),pos.GetPositionY(),pos.GetPositionZ()+10.f,true,30.f);
        if (floor>INVALID_HEIGHT) pos.m_positionZ=floor;
        auto go=std::make_unique<GameObject>();
        uint32 entry=RewardEntryBase+_cfg.EventId;
        if (sObjectMgr->GetGameObjectTemplate(entry) && go->Create(map->GenerateLowGuid<HighGuid::GameObject>(),entry,map,PHASEMASK_NORMAL,pos,QuaternionData(0.f,0.f,0.f,1.f),0,GO_STATE_READY))
        {
            go->SetSpawnedByDefault(false); go->SetRespawnTime(int32(_finish/1000));
            _chest=go->GetGUID(); if (map->AddToMap(go.get())) go.release(); else _chest.Clear();
        }
        else TC_LOG_ERROR("npcbots","City raid reward template {} missing/invalid; apply city raid reward SQL",entry);
        Announce(std::string(_cfg.BossName)+" has fallen! Victory for the "+_cfg.InvadingFactionName+"!",true);
        for (auto const& m:_attackers) if (Creature* b=GetBot(map,m)) Publish(b,{State::Celebrating,b->GetPosition(),ObjectGuid::Empty,_wp,false,false});
        for (auto const& m:_defenders) if (Creature* b=GetBot(map,m)) Publish(b,{State::Celebrating,b->GetPosition(),ObjectGuid::Empty,_wp,false,false});
    }
    void BeginRetreat()
    {
        _state=State::Retreating; _finish=15000;
        Announce("The invasion of "+std::string(_cfg.CityName)+" has been repelled. The surviving raiders are withdrawing!");
        if (Map* map=GetMap())
            for (auto* roster:{&_attackers,&_defenders}) for (auto const& m:*roster)
                if (Creature* b=GetBot(map,m)) Publish(b,{State::Retreating,b->GetPosition(),ObjectGuid::Empty,_wp,false,false});
    }
    void Announce(std::string const& text, bool victory=false)
    {
        Map* map=GetMap(); if (!map) return;
        Creature* speaker=Leader(map);
        WorldPackets::Chat::Chat packet;
        packet.Initialize(speaker?CHAT_MSG_MONSTER_YELL:CHAT_MSG_SYSTEM,LANG_UNIVERSAL,speaker,nullptr,text);
        auto const* data=packet.Write();
        uint32 sound=_cfg.AttackerTeam==HORDE?8454:8455;
        for (auto const& ref:map->GetPlayers()) if (Player* player=ref.GetSource())
            if (player->IsInWorld() && player->GetSession() && player->GetZoneId()==_cfg.CityZoneId)
            {
                player->SendDirectMessage(data);
                if (victory && sSoundEntriesStore.LookupEntry(sound)) player->PlayDirectSound(sound,player);
            }
    }
    CityRaidConfig const& _cfg;
    CityRaidType _type;
    State _state=State::Idle;
    bool _manual=false, _bossSeenAlive=false, _bossEngaged=false;
    uint32 _cooldown=0,_duration=0,_finish=0,_decision=0,_elapsed=0,_wp=0,_spawn=0,_attackCount=0,_defendCount=0;
    uint32 _attackWave=20000,_defendWave=30000,_sampleAt=1000,_swapAt=0,_sinceDecision=0,_aggroReadyAt=0;
    float _lastTankHealth=100.f,_facing=0.f;
    Position _bossAnchor;
    std::array<Position,2> _graveyards;
    ObjectGuid _boss,_tank,_chest,_lastVictim;
    std::array<ObjectGuid,2> _groups{}; // GroupMgr lookup survives an external disband.
    std::shared_ptr<RaidRuntime> _runtime;
    std::vector<CityRaidBotMember> _attackers,_defenders;
    std::map<ObjectGuid,uint32> _participants;
    std::set<ObjectGuid> _eligible,_claimed;
};

class CityRaidMgr
{
public:
    static CityRaidMgr& Instance() { static CityRaidMgr mgr; return mgr; }
    void Update(uint32 diff)
    {
        if (_shutdown) return;
        if (Tactics::Tick(_check,diff))
        {
            _check=2000;
            for (uint8 i=0;i<CITY_RAID_COUNT;++i)
            {
                auto& raid=_raids[i]; auto const& cfg=CityRaidConstants::CityConfigs[i];
                bool exists=cfg.EventId<sGameEventMgr->GetEventMap().size();
                bool event=exists && sGameEventMgr->IsActiveEvent(cfg.EventId);
                if (!sConfigMgr->GetBoolDefault("CityRaid.Enable",true)) { if (raid.Active()) raid.Stop(); _events[i]=false; continue; }
                if (event && !_events[i]) raid.Start(true);
                else if (!event && _events[i] && raid.Manual()) raid.Stop();
                else if (!event && !raid.Active() && raid.OffCooldown() && sConfigMgr->GetBoolDefault("CityRaid.AutoStart",true))
                {
                    for (auto const& [guid,player]:ObjectAccessor::GetPlayers())
                        if (player && player->IsInWorld() && !player->IsGameMaster() && player->GetMapId()==cfg.MapId && player->GetZoneId()==cfg.CityZoneId)
                        { raid.Start(false); break; }
                }
                _events[i]=event; // level-active events never restart a completed raid until a new rising edge
            }
        }
        for (auto& raid:_raids) raid.Update(diff);
        std::deque<RewardRequest> requests;
        { std::lock_guard<std::mutex> lock(_mutex); requests.swap(_claims); }
        for (auto const& request:requests)
            if (request.Raid<CITY_RAID_COUNT) _raids[request.Raid].Claim(ObjectAccessor::FindConnectedPlayer(request.PlayerGuid),request.Chest);
    }
    void Queue(RewardRequest request)
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_claims.size()<128) _claims.push_back(request);
    }
    void Shutdown() { _shutdown=true; for (auto& r:_raids) if (r.Active()) r.Stop(); }
    void Resume() { _shutdown=false; _events={}; }
private:
    CityRaidMgr() : _raids{CityRaidInstance(CITY_RAID_STORMWIND),CityRaidInstance(CITY_RAID_ORGRIMMAR)} { }
    std::array<CityRaidInstance,CITY_RAID_COUNT> _raids;
    std::array<bool,CITY_RAID_COUNT> _events{};
    uint32 _check=2000;
    bool _shutdown=false;
    std::mutex _mutex;
    std::deque<RewardRequest> _claims;
};
void QueueReward(RewardRequest request) { CityRaidMgr::Instance().Queue(request); }

class CityRaidWorldScript final : public WorldScript
{
public:
    CityRaidWorldScript() : WorldScript("CityRaidWorldScript") { }
    void OnUpdate(uint32 diff) override { CityRaidMgr::Instance().Update(diff); }
    void OnShutdownInitiate(ShutdownExitCode,ShutdownMask) override { CityRaidMgr::Instance().Shutdown(); }
    void OnShutdownCancel() override { CityRaidMgr::Instance().Resume(); }
    void OnShutdown() override { CityRaidMgr::Instance().Shutdown(); }
};
class go_city_raid_celebration final : public GameObjectScript
{
public:
    go_city_raid_celebration() : GameObjectScript("go_city_raid_celebration") { }
    struct AI final : GameObjectAI
    {
        explicit AI(GameObject* go) : GameObjectAI(go) { }
        bool OnGossipHello(Player* player) override
        {
            if (player && me->GetEntry()>=RewardEntryBase+86 && me->GetEntry()<=RewardEntryBase+87)
                QueueReward({CityRaidType(me->GetEntry()-(RewardEntryBase+86)),me->GetGUID(),player->GetGUID()});
            return true;
        }
    };
    GameObjectAI* GetAI(GameObject* go) const override { return new AI(go); }
};
}
void AddSC_event_city_raids()
{
    new CityRaidWorldScript();
    new go_city_raid_celebration();
}
