/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/* ScriptData
SDName: Undercity
SD%Complete: 95
SDComment: Quest support: 6628, 9180(post-event).
SDCategory: Undercity
EndScriptData */

/* ContentData
npc_lady_sylvanas_windrunner
npc_highborne_lamenter
npc_parqual_fintallas
EndContentData */

#include "ScriptMgr.h"
#include "Chat.h"
#include "CreatureData.h"
#include "CreatureTextMgr.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "SpellScript.h"
#include "TemporarySummon.h"

#include <span>

/*######
## npc_lady_sylvanas_windrunner
######*/

enum Sylvanas
{
    QUEST_JOURNEY_TO_UNDERCITY      = 9180,

    EMOTE_LAMENT_END                = 0,
    SAY_LAMENT_END                  = 1,
    EMOTE_LAMENT                    = 2,

    // Ambassador Sunsorrow
    SAY_SUNSORROW_WHISPER           = 0,

    SOUND_CREDIT                    = 10896,

    NPC_HIGHBORNE_LAMENTER          = 21628,
    NPC_HIGHBORNE_BUNNY             = 21641,
    NPC_AMBASSADOR_SUNSORROW        = 16287,

    SPELL_HIGHBORNE_AURA            = 37090,
    SPELL_SYLVANAS_CAST             = 36568,
    //SPELL_RIBBON_OF_SOULS         = 34432, the real one to use might be 37099
    SPELL_RIBBON_OF_SOULS           = 37099,

    // Combat spells
    SPELL_BLACK_ARROW               = 59712,
    SPELL_FADE                      = 20672,
    SPELL_FADE_BLINK                = 29211,
    SPELL_MULTI_SHOT                = 59713,
    SPELL_SHOT                      = 59710,
    SPELL_SUMMON_SKELETON           = 59711,

    // Events
    EVENT_FADE                      = 1,
    EVENT_SUMMON_SKELETON           = 2,
    EVENT_BLACK_ARROW               = 3,
    EVENT_SHOOT                     = 4,
    EVENT_MULTI_SHOT                = 5,
    EVENT_LAMENT_OF_THE_HIGHBORN    = 6,
    EVENT_SUNSORROW_WHISPER         = 7,

    GUID_EVENT_INVOKER              = 1,
};

enum Sounds
{
    SOUND_AGGRO                     = 5886
};

float HighborneLoc[4][3]=
{
    {1285.41f, 312.47f, 0.51f},
    {1286.96f, 310.40f, 1.00f},
    {1289.66f, 309.66f, 1.52f},
    {1292.51f, 310.50f, 1.99f},
};

#define HIGHBORNE_LOC_Y             -61.00f
#define HIGHBORNE_LOC_Y_NEW         -55.50f

class npc_lady_sylvanas_windrunner : public CreatureScript
{
public:
    npc_lady_sylvanas_windrunner() : CreatureScript("npc_lady_sylvanas_windrunner") { }

    struct npc_lady_sylvanas_windrunnerAI : public ScriptedAI
    {
        npc_lady_sylvanas_windrunnerAI(Creature* creature) : ScriptedAI(creature)
        {
            Initialize();
        }

        void Initialize()
        {
            LamentEvent = false;
            targetGUID.Clear();
            playerGUID.Clear();
        }

        void Reset() override
        {
            Initialize();
            _events.Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            DoPlaySoundToSet(me, SOUND_AGGRO);
            _events.ScheduleEvent(EVENT_FADE, 30s);
            _events.ScheduleEvent(EVENT_SUMMON_SKELETON, 20s);
            _events.ScheduleEvent(EVENT_BLACK_ARROW, 15s);
            _events.ScheduleEvent(EVENT_SHOOT, 8s);
            _events.ScheduleEvent(EVENT_MULTI_SHOT, 10s);
        }

        void SetGUID(ObjectGuid const& guid, int32 id) override
        {
            if (id == GUID_EVENT_INVOKER)
            {
                Talk(EMOTE_LAMENT);
                DoPlaySoundToSet(me, SOUND_CREDIT);
                DoCast(me, SPELL_SYLVANAS_CAST, false);
                playerGUID = guid;
                LamentEvent = true;

                for (uint8 i = 0; i < 4; ++i)
                    me->SummonCreature(NPC_HIGHBORNE_LAMENTER, HighborneLoc[i][0], HighborneLoc[i][1], HIGHBORNE_LOC_Y, HighborneLoc[i][2], TEMPSUMMON_TIMED_DESPAWN, 160s);

                _events.ScheduleEvent(EVENT_LAMENT_OF_THE_HIGHBORN, 2s);
                _events.ScheduleEvent(EVENT_SUNSORROW_WHISPER, 10s);
            }
        }

        void JustSummoned(Creature* summoned) override
        {
            if (summoned->GetEntry() == NPC_HIGHBORNE_BUNNY)
            {
                summoned->SetDisableGravity(true);

                if (Creature* target = ObjectAccessor::GetCreature(*summoned, targetGUID))
                {
                    target->GetMotionMaster()->MovePoint(0, target->GetPositionX(), target->GetPositionY(), me->GetPositionZ() + 15.0f, false);
                    target->UpdatePosition(target->GetPositionX(), target->GetPositionY(), me->GetPositionZ()+15.0f, 0.0f);
                    summoned->CastSpell(target, SPELL_RIBBON_OF_SOULS, false);
                }

                targetGUID = summoned->GetGUID();
            }
        }

        void UpdateAI(uint32 diff) override
        {
            if (!UpdateVictim() && !LamentEvent)
                return;

            _events.Update(diff);

            if (me->HasUnitState(UNIT_STATE_CASTING))
                return;

            while (uint32 eventId = _events.ExecuteEvent())
            {
                switch (eventId)
                {
                    case EVENT_FADE:
                        DoCast(me, SPELL_FADE);
                        // add a blink to simulate a stealthed movement and reappearing elsewhere
                        DoCast(me, SPELL_FADE_BLINK);
                        // if the victim is out of melee range she cast multi shot
                        if (Unit* victim = me->GetVictim())
                            if (me->GetDistance(victim) > 10.0f)
                                DoCast(victim, SPELL_MULTI_SHOT);
                        _events.ScheduleEvent(EVENT_FADE, 30s, 35s);
                        break;
                    case EVENT_SUMMON_SKELETON:
                        DoCast(me, SPELL_SUMMON_SKELETON);
                        _events.ScheduleEvent(EVENT_SUMMON_SKELETON, 20s, 30s);
                        break;
                    case EVENT_BLACK_ARROW:
                        if (Unit* victim = me->GetVictim())
                            DoCast(victim, SPELL_BLACK_ARROW);
                        _events.ScheduleEvent(EVENT_BLACK_ARROW, 15s, 20s);
                        break;
                    case EVENT_SHOOT:
                        if (Unit* victim = me->GetVictim())
                            DoCast(victim, SPELL_SHOT);
                        _events.ScheduleEvent(EVENT_SHOOT, 8s, 10s);
                        break;
                    case EVENT_MULTI_SHOT:
                        if (Unit* victim = me->GetVictim())
                            DoCast(victim, SPELL_MULTI_SHOT);
                        _events.ScheduleEvent(EVENT_MULTI_SHOT, 10s, 13s);
                        break;
                    case EVENT_LAMENT_OF_THE_HIGHBORN:
                        if (!me->HasAura(SPELL_SYLVANAS_CAST))
                        {
                            Talk(SAY_LAMENT_END);
                            Talk(EMOTE_LAMENT_END);
                            LamentEvent = false;
                            me->HandleEmoteCommand(EMOTE_ONESHOT_KNEEL);
                            Reset();
                        }
                        else
                        {
                            DoSummon(NPC_HIGHBORNE_BUNNY, me, 10.0f, 3s, TEMPSUMMON_TIMED_DESPAWN);
                            _events.ScheduleEvent(EVENT_LAMENT_OF_THE_HIGHBORN, 2s);
                        }
                        break;
                    case EVENT_SUNSORROW_WHISPER:
                        if (Creature* ambassador = me->FindNearestCreature(NPC_AMBASSADOR_SUNSORROW, 20.0f))
                            if (Player* player = ObjectAccessor::GetPlayer(*me, playerGUID))
                                ambassador->AI()->Talk(SAY_SUNSORROW_WHISPER, player);
                        break;
                    default:
                        break;
                }
            }

            DoMeleeAttackIfReady();
        }

        void OnQuestReward(Player* player, Quest const* quest, uint32 /*opt*/) override
        {
            if (quest->GetQuestId() == QUEST_JOURNEY_TO_UNDERCITY)
                SetGUID(player->GetGUID(), GUID_EVENT_INVOKER);
        }

    private:
        EventMap _events;
        bool LamentEvent;
        ObjectGuid targetGUID;
        ObjectGuid playerGUID;
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_lady_sylvanas_windrunnerAI(creature);
    }
};

/*######
## npc_highborne_lamenter
######*/

class npc_highborne_lamenter : public CreatureScript
{
public:
    npc_highborne_lamenter() : CreatureScript("npc_highborne_lamenter") { }

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_highborne_lamenterAI(creature);
    }

    struct npc_highborne_lamenterAI : public ScriptedAI
    {
        npc_highborne_lamenterAI(Creature* creature) : ScriptedAI(creature)
        {
            Initialize();
        }

        void Initialize()
        {
            EventMoveTimer = 10000;
            EventCastTimer = 17500;
            EventMove = true;
            EventCast = true;
        }

        uint32 EventMoveTimer;
        uint32 EventCastTimer;
        bool EventMove;
        bool EventCast;

        void Reset() override
        {
            Initialize();
        }

        void JustEngagedWith(Unit* /*who*/) override { }

        void UpdateAI(uint32 diff) override
        {
            if (EventMove)
            {
                if (EventMoveTimer <= diff)
                {
                    me->SetDisableGravity(true);
                    me->MonsterMoveWithSpeed(me->GetPositionX(), me->GetPositionY(), HIGHBORNE_LOC_Y_NEW, me->GetDistance(me->GetPositionX(), me->GetPositionY(), HIGHBORNE_LOC_Y_NEW) / (5000 * 0.001f));
                    me->UpdatePosition(me->GetPositionX(), me->GetPositionY(), HIGHBORNE_LOC_Y_NEW, me->GetOrientation());
                    EventMove = false;
                } else EventMoveTimer -= diff;
            }
            if (EventCast)
            {
                if (EventCastTimer <= diff)
                {
                    DoCast(me, SPELL_HIGHBORNE_AURA);
                    EventCast = false;
                } else EventCastTimer -= diff;
            }
        }
    };
};

/*######
## Quest 1846: Dragonmaw Shinbones
######*/

enum DragonmawShinbones
{
    SPELL_BENDING_SHINBONE1 = 8854,
    SPELL_BENDING_SHINBONE2 = 8855
};

// 8856 - Bending Shinbone
class spell_undercity_bending_shinbone : public SpellScript
{
    PrepareSpellScript(spell_undercity_bending_shinbone);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_BENDING_SHINBONE1, SPELL_BENDING_SHINBONE2 });
    }

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        GetCaster()->CastSpell(GetCaster(), roll_chance_i(20) ? SPELL_BENDING_SHINBONE1 : SPELL_BENDING_SHINBONE2);
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_undercity_bending_shinbone::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

/*######
## Battle for the Undercity & Zidormi
## References & Attribution:
##   - AzerothCore WotLK PR #1479 (commit 5379ed193b6008d40168fe6ce807b268f2d2d525 by Killyana, closing Issue #204)
##   - Dalaran-WoW zone_undercity.cpp encounter scripts
##   - TrinityCore 3.3.5a NPCBots Complete Edition
######*/

enum UndercityBattleData
{
    SPELL_BATTLE_FOR_UNDERCITY_PHASE    = 59062,

    QUEST_BATTLE_UNDERCITY_HORDE        = 13267,
    QUEST_BATTLE_UNDERCITY_ALLIANCE     = 13377,

    NPC_UC_THRALL                       = 31650,
    NPC_UC_SYLVANAS                     = 31651,
    NPC_UC_VOLJIN                       = 31649,
    NPC_UC_VARIMATHRAS                  = 31565,
    NPC_UC_PUTRESS                      = 31530,
    NPC_UC_VARIAN                       = 32303,
    NPC_UC_VARIAN_THRONE                = 32401,
    NPC_UC_JAINA_THRONE                 = 31418,
    NPC_ZIDORMI                         = 31848,
    NPC_UC_BROLL                        = 32376,
    NPC_UC_LEGION_INVADER               = 32269,
    NPC_UC_FAILED_EXPERIMENT            = 32519,

    // Spells
    SPELL_SHADOW_BOLT_VOLLEY            = 28599,
    SPELL_CARRION_SWARM                 = 31306,
    SPELL_RAIN_OF_FIRE                  = 11990,
    SPELL_BLIGHT_BARREL                 = 59460,

    // Thrall Spells
    SPELL_THRALL_CHAIN_LIGHTNING        = 16033,
    SPELL_THRALL_SHOCK                  = 16034,

    // Sylvanas Spells
    SPELL_SYLVANAS_BLACK_ARROW          = 59712,
    SPELL_SYLVANAS_MULTI_SHOT           = 59713,

    // Varian Spells
    SPELL_VARIAN_HEROIC_STRIKE          = 11978,
    SPELL_VARIAN_BATTLE_SHOUT           = 52635
};

namespace
{
enum UndercityBattleText : uint8
{
    SAY_UC_THRALL_VICTORY          = 240,
    SAY_UC_VARIAN_CONFRONTATION    = 240,
    SAY_UC_THRALL_RESPONSE         = 241,
    SAY_UC_JAINA_INTERVENTION      = 240,
    SAY_UC_THRALL_ASSAULT          = 242,
    SAY_UC_VARIMATHRAS_AGGRO       = 240,
    SAY_UC_VARIMATHRAS_DEATH       = 241,
    SAY_UC_VARIAN_ASSAULT          = 240,
    SAY_UC_VARIAN_VICTORY          = 241,
    SAY_UC_VARIAN_APOTH_GATE       = 242,
    SAY_UC_PUTRESS_AGGRO           = 240,
    SAY_UC_PUTRESS_DEATH           = 241,
};

// Use the local line until the custom world migration is loaded.
void SayUndercityBattleText(Creature* speaker, UndercityBattleText group, char const* fallback)
{
    if (sCreatureTextMgr->TextExist(speaker->GetEntry(), group))
        sCreatureTextMgr->SendChat(speaker, group);
    else
        speaker->Yell(fallback, LANG_UNIVERSAL);
}

constexpr uint32 UndercityBattlePhase = 64;
constexpr uint32 UndercityEscortGuard = 10405; // Plague Ghoul
constexpr uint32 UndercityGasTrigger = 22515;
constexpr uint32 UndercityGroundGas = 23861; // Persistent poison-cloud visual and damage
constexpr uint32 UndercityHostileFaction = 14;
constexpr uint32 UndercityCatapult = 26495;
constexpr uint32 UndercityBoulder = 38498;
constexpr uint32 UndercitySiegeProjectile = 47005; // Cosmetic boulder against the walls

Position const UndercityHordeLanding(1585.0f, 240.0f, -52.15f, 1.10f);
Position const UndercityThroneApproach(1334.0f, 313.0f, -63.68f, 3.14f);

Position const UndercityAllianceLanding(1780.0f, 420.0f, -57.14f, 3.94f);
Position const UndercityApothecariumApproach(1560.0f, 360.0f, -62.08f, 3.98f);

struct SiegePlacement
{
    Position Engine;
    Position Impact;
};

SiegePlacement const UndercitySiegeBattery[] =
{
    { { 1930.0f, 223.0f, 48.35f }, { 1863.0f, 234.0f, 62.50f } },
    { { 1927.0f, 245.0f, 48.43f }, { 1865.0f, 241.0f, 63.20f } },
    { { 1916.0f, 224.0f, 51.86f }, { 1854.0f, 231.0f, 62.50f } },
};

SiegePlacement const UndercityAllianceSiegeBattery[] =
{
    { { 1802.0f, 460.0f, 15.5f }, { 1822.0f, 436.0f, 13.2f } },
    { { 1805.0f, 465.0f, 15.5f }, { 1825.0f, 432.0f, 12.5f } },
};

enum class AssaultStop { Walk, Gas, Artillery, Lift, Corridor, Throne };

struct AssaultCheckpoint
{
    Position Location;
    AssaultStop Stop = AssaultStop::Walk;
};

// Horde Courtyard (short legs keep nav query in budget)
AssaultCheckpoint const UndercityCourtyardRoute[] =
{
    { { 1910.0f, 237.0f, 52.59f } },
    { { 1880.0f, 235.0f, 60.10f } },
    { { 1850.0f, 235.0f, 62.28f }, AssaultStop::Gas },
    { { 1819.73f, 233.333f, 60.02f } },
    { { 1800.0f, 235.0f, 62.45f }, AssaultStop::Artillery },
    { { 1770.0f, 240.0f, 60.81f } },
    { { 1740.0f, 240.0f, 62.43f } },
    { { 1710.0f, 240.0f, 62.60f } },
    { { 1680.0f, 240.0f, 62.60f } },
    { { 1650.0f, 240.0f, 62.59f } },
    { { 1620.53f, 224.0f, 62.69f } },
    { { 1597.07f, 249.60f, 60.29f } },
    { { 1596.0f, 276.0f, 55.24f }, AssaultStop::Lift },
};

// Horde Interior
AssaultCheckpoint const UndercityInteriorRoute[] =
{
    { { 1594.0f, 244.0f, -52.14f }, AssaultStop::Corridor },
    { { 1593.33f, 257.867f, -56.24f } },
    { { 1567.87f, 273.467f, -60.26f } },
    { { 1536.0f, 277.0f, -62.17f }, AssaultStop::Corridor },
    { { 1467.0f, 265.0f, -62.17f }, AssaultStop::Corridor },
    { { 1472.0f, 312.0f, -56.24f } },
    { { 1464.0f, 368.267f, -59.44f } },
    { { 1433.07f, 405.333f, -57.58f } },
    { { 1408.0f, 428.80f, -54.91f } },
    { { 1373.07f, 437.067f, -52.78f } },
    { { 1339.73f, 426.667f, -55.98f } },
    { { 1316.53f, 402.40f, -61.58f } },
    { { 1308.53f, 374.667f, -67.18f } },
    { { 1307.73f, 351.20f, -65.58f } },
    { UndercityThroneApproach, AssaultStop::Throne },
};

// Alliance Exterior Sewer Approach
AssaultCheckpoint const UndercityAllianceExteriorRoute[] =
{
    { { 1812.0f, 452.0f, 14.8f } },
    { { 1818.0f, 445.0f, 14.0f }, AssaultStop::Gas },
    { { 1822.0f, 436.0f, 13.2f }, AssaultStop::Artillery },
    { { 1826.0f, 428.0f, 12.0f }, AssaultStop::Lift }, // Sewer threshold passage
};

// Alliance Interior Canal & Apothecarium Route
AssaultCheckpoint const UndercityAllianceInteriorRoute[] =
{
    { { 1775.0f, 418.0f, -57.14f }, AssaultStop::Corridor },
    { { 1756.0f, 410.0f, -57.13f }, AssaultStop::Corridor },
    { { 1725.0f, 418.0f, -60.00f } },
    { { 1693.0f, 422.0f, -62.21f }, AssaultStop::Corridor },
    { { 1655.0f, 405.0f, -62.15f } },
    { { 1630.0f, 380.0f, -62.08f } },
    { { 1585.0f, 365.0f, -62.08f }, AssaultStop::Corridor },
    { UndercityApothecariumApproach, AssaultStop::Throne },
};

class UndercityAssaultAI : public ScriptedAI
{
public:
    UndercityAssaultAI(Creature* creature, uint32 quest, TeamId team, Position destination,
        UndercityBattleText assaultText, char const* assaultFallback)
        : ScriptedAI(creature), _quest(quest), _team(team), _destination(destination),
          _assaultText(assaultText), _assaultFallback(assaultFallback) { }

    bool OnGossipHello(Player* player) override
    {
        InitGossipMenuFor(player, 10131);
        if (me->IsQuestGiver())
            player->PrepareQuestMenu(me->GetGUID());

        if (CanParticipate(player))
        {
            if (_stage == Stage::Idle && CanStart())
                AddGossipItemFor(player, GOSSIP_ICON_CHAT, _team == TEAM_HORDE ? "I am ready to join the escort." :
                    "I am ready to lead the assault into the sewers.", GOSSIP_SENDER_MAIN, StartAssault);
            else if (_stage == Stage::PassageOpen)
                AddGossipItemFor(player, GOSSIP_ICON_CHAT, _team == TEAM_HORDE ? "Our party is ready to descend." :
                    "Take me down through the secured passage.", GOSSIP_SENDER_MAIN, CrossPassage);
        }

        SendGossipMenuFor(player, 10131, me->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
    {
        uint32 const action = player->PlayerTalkClass->GetGossipOptionAction(gossipListId);
        CloseGossipMenuFor(player);

        if (!CanParticipate(player) || !me->IsWithinDistInMap(player, INTERACTION_DISTANCE))
            return true;

        if (action == StartAssault && _stage == Stage::Idle && CanStart())
            BeginAssault(player);
        else if (action == CrossPassage && _stage == Stage::PassageOpen)
            TransferPlayer(player);

        return true;
    }

    void Reset() override
    {
        EndAssault();
    }

    void JustDied(Unit* /*killer*/) override
    {
        EndAssault();
    }

    void MovementInform(uint32 type, uint32 pointId) override
    {
        if (_stage != Stage::Moving || type != POINT_MOTION_TYPE)
            return;

        if (pointId == EscortRoutePoint)
        {
            auto const route = GetRoute();
            if (_routeIndex >= route.size())
            {
                EndAssault();
                return;
            }

            AssaultCheckpoint const& point = route[_routeIndex];
            if (me->GetExactDist(point.Location) > 4.0f)
            {
                AbortAssault();
                return;
            }

            if (point.Stop == AssaultStop::Walk)
                MoveEscort(++_routeIndex);
            else
            {
                _stage = Stage::Rallying;
                _remaining = 3min;
                _poll = 0ms;
                if (point.Stop == AssaultStop::Corridor)
                    me->Say("Clear the enemies ahead and regroup with me.", LANG_UNIVERSAL);
            }
        }
    }

    void SummonedCreatureDies(Creature* summon, Unit* /*killer*/) override
    {
        if (_stage != Stage::BarrierClosed || !_guards.erase(summon->GetGUID()))
            return;

        _catapults.erase(summon->GetGUID());
        if (!_guards.empty())
            return;

        ClearGas();
        me->Say("The way is open. Stay with me.", LANG_UNIVERSAL);
        MoveEscort(++_routeIndex);
    }

    void SummonedCreatureDespawn(Creature* summon) override
    {
        if (_stage == Stage::BarrierClosed && _guards.contains(summon->GetGUID()))
            AbortAssault();
    }

protected:
    bool UpdateAssault(uint32 diff)
    {
        if (_stage == Stage::Idle)
        {
            if (me->GetHomePosition().GetPositionZ() > 0.0f)
                UpdateSiegeApproach(diff);
            return false;
        }

        if (_remaining <= Milliseconds(diff))
        {
            if (_stage == Stage::PassageOpen)
                EndAssault();
            else
                AbortAssault();
        }
        else
        {
            _remaining -= Milliseconds(diff);
            if (!_siegeBattery.empty())
                UpdateSiegeBattery(diff);
            if (_stage == Stage::Rallying)
                UpdateRally(diff);
            else if (_stage == Stage::BarrierClosed)
            {
                if (!_catapults.empty())
                    UpdateArtillery(diff);
                else if (!_gasPockets.empty())
                    UpdateGas(diff);
            }
        }

        return true;
    }

private:
    enum class Stage { Idle, Moving, BarrierClosed, PassageOpen, Rallying };
    enum : uint32
    {
        StartAssault = GOSSIP_ACTION_INFO_DEF + 1,
        CrossPassage = GOSSIP_ACTION_INFO_DEF + 2,
        EscortRoutePoint = 1
    };

    bool CanParticipate(Player const* player) const
    {
        return player->IsAlive() && !player->IsBeingTeleported() && player->GetTeamId() == _team &&
            me->InSamePhase(player) && player->InSamePhase(UndercityBattlePhase) &&
            player->GetQuestStatus(_quest) == QUEST_STATUS_INCOMPLETE;
    }

    bool CanStart() const
    {
        return me->GetMapId() == 0 && me->GetHomePosition().GetPositionZ() > 0.0f &&
            me->IsWithinDist3d(&me->GetHomePosition(), INTERACTION_DISTANCE) && !me->IsInCombat();
    }

    void BeginAssault(Player* player)
    {
        if (player->IsInCombat())
        {
            ChatHandler(player->GetSession()).SendSysMessage("Leave combat before starting the escort.");
            return;
        }

        _originalReactState = me->GetReactState();
        _originalActive = me->isActiveObject();
        _originalWalk = me->IsWalking();
        _stage = Stage::Moving;
        me->SetReactState(REACT_PASSIVE);
        me->setActive(true);
        me->SetWalk(true);
        StartSiegeBattery();
        MoveEscort(0);

        SayUndercityBattleText(me, _assaultText, _assaultFallback);
    }

    std::span<AssaultCheckpoint const> GetRoute() const
    {
        if (_team == TEAM_HORDE)
        {
            if (_belowGround)
                return UndercityInteriorRoute;
            return UndercityCourtyardRoute;
        }
        else
        {
            if (_belowGround)
                return UndercityAllianceInteriorRoute;
            return UndercityAllianceExteriorRoute;
        }
    }

    std::span<SiegePlacement const> GetSiegePlacements() const
    {
        if (_team == TEAM_HORDE)
            return UndercitySiegeBattery;
        return UndercityAllianceSiegeBattery;
    }

    void MoveEscort(std::size_t index)
    {
        _routeIndex = index;
        auto const route = GetRoute();
        if (index >= route.size())
        {
            EndAssault();
            return;
        }
        Position const& position = route[index].Location;
        me->GetMap()->LoadGrid(position.GetPositionX(), position.GetPositionY());
        _stage = Stage::Moving;
        _remaining = 65s;
        me->GetMotionMaster()->MovePoint(EscortRoutePoint, position);
    }

    void StartSiegeBattery()
    {
        if (!_siegeBattery.empty())
            return;

        _siegeVolley = 0ms;
        for (SiegePlacement const& placement : GetSiegePlacements())
        {
            Position position = placement.Engine;
            float const ground = me->GetMap()->GetHeight(UndercityBattlePhase, position.GetPositionX(),
                position.GetPositionY(), position.GetPositionZ() + 3.0f, true, 8.0f);
            if (std::abs(ground - position.GetPositionZ()) > 3.0f)
                continue;
            position.Relocate(position.GetPositionX(), position.GetPositionY(), ground + 0.1f,
                position.GetAbsoluteAngle(&placement.Impact));

            if (TempSummon* engine = me->SummonCreature(UndercityCatapult, position, TEMPSUMMON_TIMED_DESPAWN, 2min))
            {
                engine->SetPhaseMask(UndercityBattlePhase, true);
                engine->SetFaction(me->GetFaction());
                engine->SetReactState(REACT_PASSIVE);
                engine->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_UNINTERACTIBLE);
                _siegeBattery.emplace_back(engine->GetGUID(), placement.Impact);
            }
        }
    }

    void UpdateSiegeApproach(uint32 diff)
    {
        if (_siegeBattery.empty())
        {
            if (_poll > Milliseconds(diff))
            {
                _poll -= Milliseconds(diff);
                return;
            }

            _poll = 2s;
            if (!FindEscortPlayer())
                return;

            StartSiegeBattery();
        }

        UpdateSiegeBattery(diff);
    }

    void UpdateSiegeBattery(uint32 diff)
    {
        if (_siegeVolley > Milliseconds(diff))
        {
            _siegeVolley -= Milliseconds(diff);
            return;
        }
        _siegeVolley = 6s;
        std::erase_if(_siegeBattery, [this](auto const& shot)
        {
            return !ObjectAccessor::GetCreature(*me, shot.first);
        });
        for (auto const& [guid, impact] : _siegeBattery)
            if (Creature* engine = ObjectAccessor::GetCreature(*me, guid))
                engine->CastSpell(impact, UndercitySiegeProjectile, true);
    }

    void ClearSiegeBattery()
    {
        for (auto const& [guid, impact] : _siegeBattery)
            if (Creature* engine = ObjectAccessor::GetCreature(*me, guid))
                engine->DespawnOrUnsummon();
        _siegeBattery.clear();
    }

    Player* FindEscortPlayer() const
    {
        std::list<Player*> players;
        me->GetPlayerListInGrid(players, 24.0f);
        for (Player* player : players)
            if (CanParticipate(player) && me->IsWithinLOSInMap(player))
                return player;
        return nullptr;
    }

    bool HasCorridorGuards() const
    {
        std::list<Creature*> enemies;
        // Check Horde corridor enemies
        me->GetCreatureListWithEntryInGrid(enemies, 31278, 16.0f);
        me->GetCreatureListWithEntryInGrid(enemies, 32772, 16.0f);
        // Check Alliance corridor enemies
        me->GetCreatureListWithEntryInGrid(enemies, 32394, 16.0f);
        me->GetCreatureListWithEntryInGrid(enemies, 32395, 16.0f);
        me->GetCreatureListWithEntryInGrid(enemies, 32396, 16.0f);
        me->GetCreatureListWithEntryInGrid(enemies, 32397, 16.0f);
        return std::any_of(enemies.begin(), enemies.end(), [this](Creature const* enemy)
        {
            CreatureData const* spawn = enemy->GetCreatureData();
            return enemy->IsAlive() && me->InSamePhase(enemy) && spawn && spawn->StringId.starts_with("undercity_assault_");
        });
    }

    void UpdateRally(uint32 diff)
    {
        if (_poll > Milliseconds(diff))
        {
            _poll -= Milliseconds(diff);
            return;
        }
        _poll = 1s;
        if (!FindEscortPlayer())
            return;

        // The first indoor leg waits at the landing until the player's transfer completes.
        if (_awaitingArrival)
        {
            _awaitingArrival = false;
            MoveEscort(0);
            return;
        }

        auto const route = GetRoute();
        if (_routeIndex >= route.size())
        {
            EndAssault();
            return;
        }

        switch (route[_routeIndex].Stop)
        {
            case AssaultStop::Gas:
                SetUpGasBarrier(false);
                break;
            case AssaultStop::Artillery:
                SetUpGasBarrier(true);
                break;
            case AssaultStop::Lift:
                _stage = Stage::PassageOpen;
                _remaining = 2min;
                if (_team == TEAM_HORDE)
                    me->Say("Gather your party by the lift. Tell me when you are ready to descend together.", LANG_UNIVERSAL);
                else
                    me->Say("Gather your party by the sewer tunnel. Tell me when you are ready to breach the lower city.", LANG_UNIVERSAL);
                break;
            case AssaultStop::Corridor:
                if (!HasCorridorGuards())
                    MoveEscort(++_routeIndex);
                break;
            case AssaultStop::Throne:
                if (_team == TEAM_HORDE)
                    me->Say("We're at the Royal Quarter. Finish clearing the way to Varimathras.", LANG_UNIVERSAL);
                else
                    me->Say("We're at the Apothecarium. Finish clearing the way to Putress.", LANG_UNIVERSAL);
                EndAssault();
                break;
            case AssaultStop::Walk:
                break;
        }
    }

    void UpdateArtillery(uint32 diff)
    {
        if (_poll > Milliseconds(diff))
        {
            _poll -= Milliseconds(diff);
            return;
        }
        _poll = 5s;
        if (Player* player = FindEscortPlayer())
            for (ObjectGuid const& guid : _catapults)
                if (Creature* catapult = ObjectAccessor::GetCreature(*me, guid))
                    catapult->CastSpell(player->GetPosition(), UndercityBoulder, true);
    }

    void UpdateGas(uint32 diff)
    {
        if (_poll > Milliseconds(diff))
        {
            _poll -= Milliseconds(diff);
            return;
        }
        _poll = 8s;
        for (ObjectGuid const& guid : _gasPockets)
            if (Creature* trigger = ObjectAccessor::GetCreature(*me, guid))
            {
                trigger->RemoveAllDynObjects();
                trigger->CastSpell(trigger->GetPosition(), UndercityGroundGas, true);
            }
    }

    void SetUpGasBarrier(bool artillery = false)
    {
        _stage = Stage::BarrierClosed;
        _remaining = artillery ? 2min : 55s;
        if (artillery)
        {
            for (float angle : { -0.7f, 0.7f })
            {
                Position position = me->GetPosition();
                me->MovePositionToFirstCollision(position, 7.0f, angle);
                TempSummon* catapult = me->SummonCreature(UndercityCatapult, position, TEMPSUMMON_TIMED_DESPAWN, _remaining);
                if (!catapult)
                {
                    AbortAssault();
                    return;
                }
                catapult->SetPhaseMask(UndercityBattlePhase, true);
                catapult->SetFaction(UndercityHostileFaction);
                catapult->SetMaxHealth(8500);
                catapult->SetFullHealth();
                catapult->SetReactState(REACT_PASSIVE);
                _catapults.insert(catapult->GetGUID());
                _guards.insert(catapult->GetGUID());
            }
            _poll = 3s;
        }
        else
        {
            for (float angle : { -0.55f, 0.55f })
            {
                Position position = me->GetPosition();
                me->MovePositionToFirstCollision(position, 5.0f, angle);
                if (TempSummon* gas = me->SummonCreature(UndercityGasTrigger, position, TEMPSUMMON_TIMED_DESPAWN, _remaining))
                {
                    gas->SetPhaseMask(UndercityBattlePhase, true);
                    gas->SetFaction(UndercityHostileFaction);
                    _gasPockets.push_back(gas->GetGUID());
                }
            }
            _poll = 0ms;
            UpdateGas(0);
        }

        for (float angle : { -1.2f, 1.2f })
        {
            Position position = me->GetPosition();
            me->MovePositionToFirstCollision(position, 4.0f, angle);
            TempSummon* guard = me->SummonCreature(UndercityEscortGuard, position, TEMPSUMMON_TIMED_DESPAWN, _remaining);
            if (!guard)
            {
                AbortAssault();
                return;
            }

            _guards.insert(guard->GetGUID());
            guard->SetPhaseMask(UndercityBattlePhase, true);
            guard->SetFaction(UndercityHostileFaction);
            guard->SetMaxHealth(7000);
            guard->SetFullHealth();
            guard->SetReactState(REACT_DEFENSIVE);
        }

        me->Say(artillery ? "Incoming fire! Destroy the catapults and clear out their crew." :
            "Gas is blocking the passage. Take out both guards so we can get through.", LANG_UNIVERSAL);
    }

    bool CanDescend(Player const* player) const
    {
        return CanParticipate(player) && !player->IsInCombat() && !player->GetTransport() && player->IsWithinLOSInMap(me);
    }

    bool Descend(Player* player, Position const& landing) const
    {
        if (!player->HasAura(SPELL_BATTLE_FOR_UNDERCITY_PHASE))
            player->CastSpell(player, SPELL_BATTLE_FOR_UNDERCITY_PHASE, true);
        return player->TeleportTo(me->GetMapId(), landing.GetPositionX(), landing.GetPositionY(), landing.GetPositionZ(),
            landing.GetOrientation());
    }

    void TransferPlayer(Player* player)
    {
        if (!CanDescend(player))
        {
            ChatHandler(player->GetSession()).SendSysMessage("Move closer to the escort and leave combat before descending.");
            return;
        }

        Map* map = me->GetMap();
        map->LoadGrid(_destination.GetPositionX(), _destination.GetPositionY());
        float const landingZ = map->GetHeight(UndercityBattlePhase, _destination.GetPositionX(),
            _destination.GetPositionY(), _destination.GetPositionZ() + 3.0f, true, 8.0f);
        if (!std::isfinite(landingZ) || std::abs(landingZ - _destination.GetPositionZ()) > 4.0f)
        {
            ChatHandler(player->GetSession()).SendSysMessage("The landing point is unsafe. The descent has been stopped.");
            return;
        }

        std::list<Player*> party;
        if (player->GetGroup())
        {
            std::list<Player*> nearby;
            me->GetPlayerListInGrid(nearby, 25.0f);
            for (Player* member : nearby)
            {
                if (member == player || !player->IsInSameGroupWith(member) || !CanParticipate(member))
                    continue;
                if (!CanDescend(member))
                {
                    ChatHandler(player->GetSession()).SendSysMessage("Wait for your nearby party members to reach the rally point and leave combat.");
                    return;
                }
                party.push_back(member);
            }
        }

        Position const landing(_destination.GetPositionX(), _destination.GetPositionY(), landingZ + 0.2f, _destination.GetOrientation());
        if (!Descend(player, landing))
        {
            ChatHandler(player->GetSession()).SendSysMessage("The descent failed. Please try again.");
            return;
        }

        for (Player* member : party)
            if (!Descend(member, landing))
                ChatHandler(member->GetSession()).SendSysMessage("The descent failed. Return to the rally point and try again.");

        _belowGround = true;
        _awaitingArrival = true;
        _stage = Stage::Rallying;
        _remaining = 2min;
        _poll = 2s;
        ClearSiegeBattery();
        me->NearTeleportTo(landing);
        me->Say("Stay close. I'll lead you through the lower corridors.", LANG_UNIVERSAL);
    }

    void ClearGas()
    {
        for (ObjectGuid const& guid : _gasPockets)
            if (Creature* trigger = ObjectAccessor::GetCreature(*me, guid))
            {
                trigger->RemoveAllDynObjects();
                trigger->DespawnOrUnsummon();
            }
        _gasPockets.clear();
    }

    void AbortAssault()
    {
        me->Say("Fall back to the rally point. We can try again there.", LANG_UNIVERSAL);
        EndAssault();
    }

    void EndAssault()
    {
        if (_stage == Stage::Idle)
            return;

        _stage = Stage::Idle;
        ClearGas();
        ClearSiegeBattery();
        _catapults.clear();
        for (ObjectGuid const& guid : _guards)
            if (Creature* guard = ObjectAccessor::GetCreature(*me, guid))
                guard->DespawnOrUnsummon();
        _guards.clear();
        _belowGround = false;
        _awaitingArrival = false;
        _poll = 0ms;
        me->SetReactState(_originalReactState);
        me->setActive(_originalActive);
        me->SetWalk(_originalWalk);
        if (me->IsAlive())
        {
            me->GetMotionMaster()->MoveIdle();
            me->NearTeleportTo(me->GetHomePosition());
        }
    }

    uint32 const _quest;
    TeamId const _team;
    Position const _destination;
    UndercityBattleText const _assaultText;
    char const* const _assaultFallback;
    Stage _stage = Stage::Idle;
    Milliseconds _remaining = 0ms;
    Milliseconds _poll = 0ms;
    Milliseconds _siegeVolley = 0ms;
    std::vector<ObjectGuid> _gasPockets;
    std::vector<std::pair<ObjectGuid, Position>> _siegeBattery;
    std::set<ObjectGuid> _guards;
    std::set<ObjectGuid> _catapults;
    std::size_t _routeIndex = 0;
    ReactStates _originalReactState = REACT_PASSIVE;
    bool _originalActive = false;
    bool _originalWalk = false;
    bool _belowGround = false;
    bool _awaitingArrival = false;
};
}

/*######
## npc_zidormi_undercity
######*/

class npc_zidormi_undercity : public CreatureScript
{
public:
    npc_zidormi_undercity() : CreatureScript("npc_zidormi_undercity") { }

    struct npc_zidormi_undercityAI : public ScriptedAI
    {
        npc_zidormi_undercityAI(Creature* creature) : ScriptedAI(creature) { }

        bool OnGossipHello(Player* player) override
        {
            InitGossipMenuFor(player, 10131);

            bool inBattlePhase = player->HasAura(SPELL_BATTLE_FOR_UNDERCITY_PHASE) || (player->GetPhaseMask() & 64);
            bool hasHordeQuest = player->GetQuestStatus(QUEST_BATTLE_UNDERCITY_HORDE) == QUEST_STATUS_INCOMPLETE;
            bool hasAllianceQuest = player->GetQuestStatus(QUEST_BATTLE_UNDERCITY_ALLIANCE) == QUEST_STATUS_INCOMPLETE;
            bool completedHorde = player->GetQuestStatus(QUEST_BATTLE_UNDERCITY_HORDE) == QUEST_STATUS_COMPLETE || player->GetQuestRewardStatus(QUEST_BATTLE_UNDERCITY_HORDE);
            bool completedAlliance = player->GetQuestStatus(QUEST_BATTLE_UNDERCITY_ALLIANCE) == QUEST_STATUS_COMPLETE || player->GetQuestRewardStatus(QUEST_BATTLE_UNDERCITY_ALLIANCE);

            if (inBattlePhase)
            {
                AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Show me Tirisfal Glades and Undercity in peacetime.", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);

                if (hasHordeQuest || hasAllianceQuest)
                {
                    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "The assault is already underway. Teleport me inside to join the battle.", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 4);
                }
            }
            else if (hasHordeQuest || hasAllianceQuest || completedHorde || completedAlliance)
            {
                AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Take me back to the Battle for the Undercity.", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 2);
            }

            if (hasHordeQuest || hasAllianceQuest)
            {
                AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Report the victory and complete the quest.", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 3);
            }

            SendGossipMenuFor(player, 10131, me->GetGUID());
            return true;
        }

        bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
        {
            uint32 const action = player->PlayerTalkClass->GetGossipOptionAction(gossipListId);
            ClearGossipMenuFor(player);
            switch (action)
            {
                case GOSSIP_ACTION_INFO_DEF + 1:
                    player->RemoveAurasDueToSpell(SPELL_BATTLE_FOR_UNDERCITY_PHASE);
                    player->SetPhaseMask(1, true);
                    ChatHandler(player->GetSession()).SendSysMessage("You have left the battle and returned to Undercity in peacetime.");
                    CloseGossipMenuFor(player);
                    break;
                case GOSSIP_ACTION_INFO_DEF + 2:
                    player->CastSpell(player, SPELL_BATTLE_FOR_UNDERCITY_PHASE, true);
                    player->SetPhaseMask(64, true);
                    ChatHandler(player->GetSession()).SendSysMessage("You have entered the Battle for the Undercity.");
                    CloseGossipMenuFor(player);
                    break;
                case GOSSIP_ACTION_INFO_DEF + 3:
                    if (player->GetQuestStatus(QUEST_BATTLE_UNDERCITY_HORDE) == QUEST_STATUS_INCOMPLETE)
                    {
                        player->AreaExploredOrEventHappens(QUEST_BATTLE_UNDERCITY_HORDE);
                        player->GroupEventHappens(QUEST_BATTLE_UNDERCITY_HORDE, me);
                    }
                    if (player->GetQuestStatus(QUEST_BATTLE_UNDERCITY_ALLIANCE) == QUEST_STATUS_INCOMPLETE)
                    {
                        player->AreaExploredOrEventHappens(QUEST_BATTLE_UNDERCITY_ALLIANCE);
                        player->GroupEventHappens(QUEST_BATTLE_UNDERCITY_ALLIANCE, me);
                    }
                    player->RemoveAurasDueToSpell(SPELL_BATTLE_FOR_UNDERCITY_PHASE);
                    player->SetPhaseMask(1, true);
                    ChatHandler(player->GetSession()).SendSysMessage("The Battle for the Undercity is complete. You have returned to the normal phase.");
                    CloseGossipMenuFor(player);
                    break;
                case GOSSIP_ACTION_INFO_DEF + 4:
                    if (!player->HasAura(SPELL_BATTLE_FOR_UNDERCITY_PHASE))
                        player->CastSpell(player, SPELL_BATTLE_FOR_UNDERCITY_PHASE, true);
                    player->SetPhaseMask(64, true);
                    if (player->GetTeamId() == TEAM_HORDE)
                        player->TeleportTo(0, UndercityHordeLanding.GetPositionX(), UndercityHordeLanding.GetPositionY(), UndercityHordeLanding.GetPositionZ(), UndercityHordeLanding.GetOrientation());
                    else
                        player->TeleportTo(0, UndercityAllianceLanding.GetPositionX(), UndercityAllianceLanding.GetPositionY(), UndercityAllianceLanding.GetPositionZ(), UndercityAllianceLanding.GetOrientation());
                    ChatHandler(player->GetSession()).SendSysMessage("You have rejoined the assault inside the Undercity.");
                    CloseGossipMenuFor(player);
                    break;
            }
            return true;
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_zidormi_undercityAI(creature);
    }
};

/*######
## npc_thrall_battle_for_undercity
######*/

class npc_thrall_battle_for_undercity : public CreatureScript
{
public:
    npc_thrall_battle_for_undercity() : CreatureScript("npc_thrall_battle_for_undercity") { }

    struct npc_thrall_battle_for_undercityAI : public UndercityAssaultAI
    {
        npc_thrall_battle_for_undercityAI(Creature* creature)
            : UndercityAssaultAI(creature, QUEST_BATTLE_UNDERCITY_HORDE, TEAM_HORDE, UndercityHordeLanding,
                SAY_UC_THRALL_ASSAULT, "For the Horde! Soldiers, push into the heart of the city. Sweep aside anyone who stands in our way!")
        {
            Initialize();
        }

        void Initialize()
        {
            ChainLightningTimer = 4000;
            ShockTimer = 9000;
            events.Reset();
        }

        uint32 ChainLightningTimer;
        uint32 ShockTimer;
        EventMap events;

        void Reset() override
        {
            UndercityAssaultAI::Reset();
            Initialize();
        }

        void DoAction(int32 actionId) override
        {
            if (actionId == 1) // Triggered when Varimathras dies
            {
                events.ScheduleEvent(1, 2s);
                events.ScheduleEvent(2, 6s);
                events.ScheduleEvent(3, 12s);
                events.ScheduleEvent(4, 18s);
            }
        }

        void UpdateAI(uint32 diff) override
        {
            if (UpdateAssault(diff))
                return;

            events.Update(diff);
            while (uint32 eventId = events.ExecuteEvent())
            {
                switch (eventId)
                {
                    case 1:
                        SayUndercityBattleText(me, SAY_UC_THRALL_VICTORY, "Varimathras has fallen! Undercity is ours again. For the Forsaken!");
                        break;
                    case 2:
                        if (Creature* varian = me->SummonCreature(NPC_UC_VARIAN_THRONE, 1315.0f, 316.0f, -63.5f, 3.14f, TEMPSUMMON_TIMED_DESPAWN, 20s))
                            SayUndercityBattleText(varian, SAY_UC_VARIAN_CONFRONTATION, "Thrall! We have come for answers about Bolvar Fordragon and everyone who died at the Wrathgate!");
                        me->SummonCreature(NPC_UC_JAINA_THRONE, 1318.0f, 320.0f, -63.5f, 3.14f, TEMPSUMMON_TIMED_DESPAWN, 20s);
                        break;
                    case 3:
                        SayUndercityBattleText(me, SAY_UC_THRALL_RESPONSE, "Varian, pull your soldiers back! The traitor has fallen and the plague is contained. There is no need to start a war here.");
                        break;
                    case 4:
                        if (Creature* jaina = me->FindNearestCreature(NPC_UC_JAINA_THRONE, 50.0f))
                        {
                            SayUndercityBattleText(jaina, SAY_UC_JAINA_INTERVENTION, "Stand down, both of you! Enough blood has been spilled today.");
                            jaina->CastSpell(jaina, 55761, true);
                        }
                        if (Creature* varian = me->FindNearestCreature(NPC_UC_VARIAN_THRONE, 50.0f))
                            varian->DespawnOrUnsummon(1s);
                        break;
                }
            }

            if (!UpdateVictim())
                return;

            if (ChainLightningTimer <= diff)
            {
                DoCastVictim(SPELL_THRALL_CHAIN_LIGHTNING);
                ChainLightningTimer = 9000;
            }
            else ChainLightningTimer -= diff;

            if (ShockTimer <= diff)
            {
                DoCastVictim(SPELL_THRALL_SHOCK);
                ShockTimer = 14000;
            }
            else ShockTimer -= diff;

            DoMeleeAttackIfReady();
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_thrall_battle_for_undercityAI(creature);
    }
};

/*######
## boss_varimathras_battle_for_undercity
######*/

class boss_varimathras_battle_for_undercity : public CreatureScript
{
public:
    boss_varimathras_battle_for_undercity() : CreatureScript("boss_varimathras_battle_for_undercity") { }

    struct boss_varimathras_battle_for_undercityAI : public ScriptedAI
    {
        boss_varimathras_battle_for_undercityAI(Creature* creature) : ScriptedAI(creature)
        {
            Initialize();
        }

        void Initialize()
        {
            ShadowBoltTimer = 5000;
            CarrionSwarmTimer = 12000;
            RainOfFireTimer = 18000;
            SummonPercent = 75;
        }

        uint32 ShadowBoltTimer;
        uint32 CarrionSwarmTimer;
        uint32 RainOfFireTimer;
        uint32 SummonPercent;

        void Reset() override
        {
            Initialize();
            _stage = Stage::Waiting;
            _stageTimer = 2s;
            for (ObjectGuid const& guid : _guards)
                if (Creature* guard = ObjectAccessor::GetCreature(*me, guid))
                    guard->DespawnOrUnsummon();
            _guards.clear();
            if (_activeForGuards)
            {
                me->setActive(false);
                _activeForGuards = false;
            }
            me->SetReactState(REACT_PASSIVE);
            me->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_UNINTERACTIBLE);
            me->SetVisible(false);
        }

        void SummonedCreatureDies(Creature* summon, Unit* /*killer*/) override
        {
            if (_stage != Stage::Guards || !_guards.erase(summon->GetGUID()) || !_guards.empty())
                return;

            _stage = Stage::Boss;
            _stageTimer = 2min;
            me->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_UNINTERACTIBLE);
            me->SetReactState(REACT_AGGRESSIVE);
            me->SetVisible(true);
            if (Creature* thrall = me->FindNearestCreature(NPC_UC_THRALL, 75.0f))
                thrall->Say("The way is clear. Varimathras is up ahead.", LANG_UNIVERSAL);
        }

        void SummonedCreatureDespawn(Creature* summon) override
        {
            if (_stage == Stage::Guards && _guards.contains(summon->GetGUID()))
                Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            SayUndercityBattleText(me, SAY_UC_VARIMATHRAS_AGGRO, "You are too late, worms. This city belongs to the Legion now!");
        }

        void JustDied(Unit* /*killer*/) override
        {
            if (_activeForGuards)
            {
                me->setActive(false);
                _activeForGuards = false;
            }
            SayUndercityBattleText(me, SAY_UC_VARIMATHRAS_DEATH, "Damn you... Master, forgive me...");

            std::list<Player*> playerList;
            me->GetPlayerListInGrid(playerList, 75.0f);
            for (Player* p : playerList)
            {
                if (p->GetQuestStatus(QUEST_BATTLE_UNDERCITY_HORDE) == QUEST_STATUS_INCOMPLETE)
                {
                    p->AreaExploredOrEventHappens(QUEST_BATTLE_UNDERCITY_HORDE);
                    p->GroupEventHappens(QUEST_BATTLE_UNDERCITY_HORDE, me);
                }
            }

            if (Creature* thrall = me->FindNearestCreature(NPC_UC_THRALL, 75.0f))
                thrall->AI()->DoAction(1);
        }

        void UpdateAI(uint32 diff) override
        {
            if (_stage != Stage::Boss || !UpdateVictim())
            {
                UpdatePrelude(diff);
                return;
            }

            if (ShadowBoltTimer <= diff)
            {
                DoCastVictim(SPELL_SHADOW_BOLT_VOLLEY);
                ShadowBoltTimer = 9000;
            }
            else ShadowBoltTimer -= diff;

            if (CarrionSwarmTimer <= diff)
            {
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 40.0f, true))
                    DoCast(target, SPELL_CARRION_SWARM);
                CarrionSwarmTimer = 16000;
            }
            else CarrionSwarmTimer -= diff;

            if (RainOfFireTimer <= diff)
            {
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 40.0f, true))
                    DoCast(target, SPELL_RAIN_OF_FIRE);
                RainOfFireTimer = 22000;
            }
            else RainOfFireTimer -= diff;

            if (HealthBelowPct(SummonPercent))
            {
                SummonPercent -= 25;
                for (uint8 i = 0; i < 2; ++i)
                    me->SummonCreature(NPC_UC_LEGION_INVADER, me->GetPositionX() + frand(-5.0f, 5.0f), me->GetPositionY() + frand(-5.0f, 5.0f), me->GetPositionZ(), 0.0f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 60s);
            }

            DoMeleeAttackIfReady();
        }

    private:
        enum class Stage { Waiting, Guards, Boss };

        void UpdatePrelude(uint32 diff)
        {
            if (_stageTimer > Milliseconds(diff))
            {
                _stageTimer -= Milliseconds(diff);
                return;
            }

            if (_stage != Stage::Waiting)
            {
                Reset();
                return;
            }

            _stageTimer = 2s;
            std::list<Player*> players;
            me->GetPlayerListInGrid(players, 75.0f);
            for (Player* player : players)
                if (player->GetTeamId() == TEAM_HORDE && player->InSamePhase(UndercityBattlePhase) &&
                    player->GetQuestStatus(QUEST_BATTLE_UNDERCITY_HORDE) == QUEST_STATUS_INCOMPLETE &&
                    player->GetExactDist(UndercityThroneApproach) < 16.0f)
                {
                    SummonThroneGuards(player);
                    return;
                }
        }

        void SummonThroneGuards(Player* player)
        {
            _stage = Stage::Guards;
            _stageTimer = 90s;
            if (!me->isActiveObject())
            {
                me->setActive(true);
                _activeForGuards = true;
            }

            // Both positions are on the same floor as the Royal Quarter approach.
            for (Position const& position : { Position(1338.0f, 313.0f, -63.66f, 3.14f),
                Position(1334.0f, 319.0f, -63.70f, 3.14f) })
            {
                TempSummon* guard = me->SummonCreature(UndercityEscortGuard, position, TEMPSUMMON_TIMED_DESPAWN, _stageTimer);
                if (!guard)
                {
                    Reset();
                    return;
                }

                _guards.insert(guard->GetGUID());
                guard->SetFaction(UndercityHostileFaction);
                guard->SetMaxHealth(9000);
                guard->SetFullHealth();
                guard->AI()->AttackStart(player);
            }

            if (Creature* thrall = me->FindNearestCreature(NPC_UC_THRALL, 75.0f))
                thrall->Say("Hold here and take out the throne room guards. Then we can go after Varimathras.", LANG_UNIVERSAL);
        }

        Stage _stage = Stage::Waiting;
        Milliseconds _stageTimer = 2s;
        std::set<ObjectGuid> _guards;
        bool _activeForGuards = false;
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new boss_varimathras_battle_for_undercityAI(creature);
    }
};

/*######
## npc_sylvanas_battle_for_undercity
######*/

class npc_sylvanas_battle_for_undercity : public CreatureScript
{
public:
    npc_sylvanas_battle_for_undercity() : CreatureScript("npc_sylvanas_battle_for_undercity") { }

    struct npc_sylvanas_battle_for_undercityAI : public ScriptedAI
    {
        npc_sylvanas_battle_for_undercityAI(Creature* creature) : ScriptedAI(creature)
        {
            BlackArrowTimer = 4000;
            MultiShotTimer = 8000;
        }

        uint32 BlackArrowTimer;
        uint32 MultiShotTimer;

        void Reset() override
        {
            BlackArrowTimer = 4000;
            MultiShotTimer = 8000;
        }

        void UpdateAI(uint32 diff) override
        {
            if (!UpdateVictim())
                return;

            if (BlackArrowTimer <= diff)
            {
                DoCastVictim(SPELL_SYLVANAS_BLACK_ARROW);
                BlackArrowTimer = 12000;
            }
            else BlackArrowTimer -= diff;

            if (MultiShotTimer <= diff)
            {
                DoCastVictim(SPELL_SYLVANAS_MULTI_SHOT);
                MultiShotTimer = 9000;
            }
            else MultiShotTimer -= diff;

            DoMeleeAttackIfReady();
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_sylvanas_battle_for_undercityAI(creature);
    }
};

/*######
## npc_varian_battle_for_undercity
######*/

class npc_varian_battle_for_undercity : public CreatureScript
{
public:
    npc_varian_battle_for_undercity() : CreatureScript("npc_varian_battle_for_undercity") { }

    struct npc_varian_battle_for_undercityAI : public UndercityAssaultAI
    {
        npc_varian_battle_for_undercityAI(Creature* creature)
            : UndercityAssaultAI(creature, QUEST_BATTLE_UNDERCITY_ALLIANCE, TEAM_ALLIANCE, UndercityAllianceLanding,
                SAY_UC_VARIAN_ASSAULT, "For Bolvar and the Alliance! Sweep through the sewers and take down the traitors!")
        {
            StrikeTimer = 4000;
            ShoutTimer = 10000;
        }

        uint32 StrikeTimer;
        uint32 ShoutTimer;

        void Reset() override
        {
            UndercityAssaultAI::Reset();
            StrikeTimer = 4000;
            ShoutTimer = 10000;
        }

        void UpdateAI(uint32 diff) override
        {
            if (UpdateAssault(diff))
                return;

            if (!UpdateVictim())
                return;

            if (StrikeTimer <= diff)
            {
                DoCastVictim(SPELL_VARIAN_HEROIC_STRIKE);
                StrikeTimer = 6000;
            }
            else StrikeTimer -= diff;

            if (ShoutTimer <= diff)
            {
                DoCast(me, SPELL_VARIAN_BATTLE_SHOUT);
                ShoutTimer = 30000;
            }
            else ShoutTimer -= diff;

            DoMeleeAttackIfReady();
        }
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new npc_varian_battle_for_undercityAI(creature);
    }
};

/*######
## boss_putress_battle_for_undercity
######*/

class boss_putress_battle_for_undercity : public CreatureScript
{
public:
    boss_putress_battle_for_undercity() : CreatureScript("boss_putress_battle_for_undercity") { }

    struct boss_putress_battle_for_undercityAI : public ScriptedAI
    {
        boss_putress_battle_for_undercityAI(Creature* creature) : ScriptedAI(creature)
        {
            Initialize();
        }

        void Initialize()
        {
            ShadowBoltTimer = 4000;
            BlightBarrelTimer = 9000;
            SummonPercent = 75;
        }

        uint32 ShadowBoltTimer;
        uint32 BlightBarrelTimer;
        uint32 SummonPercent;

        void Reset() override
        {
            Initialize();
            _stage = Stage::Waiting;
            _stageTimer = 2s;
            for (ObjectGuid const& guid : _guards)
                if (Creature* guard = ObjectAccessor::GetCreature(*me, guid))
                    guard->DespawnOrUnsummon();
            _guards.clear();
            if (_activeForGuards)
            {
                me->setActive(false);
                _activeForGuards = false;
            }
            me->SetReactState(REACT_PASSIVE);
            me->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_UNINTERACTIBLE);
            me->SetVisible(false);
        }

        void SummonedCreatureDies(Creature* summon, Unit* /*killer*/) override
        {
            if (_stage != Stage::Guards || !_guards.erase(summon->GetGUID()) || !_guards.empty())
                return;

            _stage = Stage::Boss;
            _stageTimer = 2min;
            me->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_UNINTERACTIBLE);
            me->SetReactState(REACT_AGGRESSIVE);
            me->SetVisible(true);
            SayUndercityBattleText(me, SAY_UC_PUTRESS_AGGRO, "You thought we were helpless? Our plague will scour this world!");
        }

        void SummonedCreatureDespawn(Creature* summon) override
        {
            if (_stage == Stage::Guards && _guards.contains(summon->GetGUID()))
                Reset();
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            SayUndercityBattleText(me, SAY_UC_PUTRESS_AGGRO, "You thought we were helpless? Our plague will scour this world!");
        }

        void JustDied(Unit* /*killer*/) override
        {
            if (_activeForGuards)
            {
                me->setActive(false);
                _activeForGuards = false;
            }
            SayUndercityBattleText(me, SAY_UC_PUTRESS_DEATH, "My work... will outlive me...");

            std::list<Player*> playerList;
            me->GetPlayerListInGrid(playerList, 75.0f);
            for (Player* p : playerList)
            {
                if (p->GetQuestStatus(QUEST_BATTLE_UNDERCITY_ALLIANCE) == QUEST_STATUS_INCOMPLETE)
                {
                    p->AreaExploredOrEventHappens(QUEST_BATTLE_UNDERCITY_ALLIANCE);
                    p->GroupEventHappens(QUEST_BATTLE_UNDERCITY_ALLIANCE, me);
                }
            }

            // Spawn King Varian on the platform for convenient turn-in
            if (TempSummon* varian = me->SummonCreature(NPC_UC_VARIAN, 1548.0f, 362.0f, -62.16f, 3.14f, TEMPSUMMON_TIMED_DESPAWN, 10min))
            {
                varian->SetNpcFlag(UNIT_NPC_FLAG_QUESTGIVER | UNIT_NPC_FLAG_GOSSIP);
                varian->SetPhaseMask(UndercityBattlePhase, true);
                varian->Yell("Justice has been served for the fallen at the Wrathgate! The Alliance has triumphed!", LANG_UNIVERSAL);
            }
        }

        void UpdateAI(uint32 diff) override
        {
            if (_stage != Stage::Boss || !UpdateVictim())
            {
                UpdatePrelude(diff);
                return;
            }

            if (ShadowBoltTimer <= diff)
            {
                DoCastVictim(SPELL_SHADOW_BOLT_VOLLEY);
                ShadowBoltTimer = 8000;
            }
            else ShadowBoltTimer -= diff;

            if (BlightBarrelTimer <= diff)
            {
                if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 40.0f, true))
                    DoCast(target, SPELL_BLIGHT_BARREL);
                BlightBarrelTimer = 12000;
            }
            else BlightBarrelTimer -= diff;

            if (HealthBelowPct(SummonPercent))
            {
                SummonPercent -= 25;
                for (uint8 i = 0; i < 2; ++i)
                    me->SummonCreature(NPC_UC_FAILED_EXPERIMENT, me->GetPositionX() + frand(-4.0f, 4.0f), me->GetPositionY() + frand(-4.0f, 4.0f), me->GetPositionZ(), 0.0f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 60s);
            }

            DoMeleeAttackIfReady();
        }

    private:
        enum class Stage { Waiting, Guards, Boss };

        void UpdatePrelude(uint32 diff)
        {
            if (_stageTimer > Milliseconds(diff))
            {
                _stageTimer -= Milliseconds(diff);
                return;
            }

            if (_stage != Stage::Waiting)
            {
                Reset();
                return;
            }

            _stageTimer = 2s;
            std::list<Player*> players;
            me->GetPlayerListInGrid(players, 75.0f);
            for (Player* player : players)
                if (player->GetTeamId() == TEAM_ALLIANCE && player->InSamePhase(UndercityBattlePhase) &&
                    player->GetQuestStatus(QUEST_BATTLE_UNDERCITY_ALLIANCE) == QUEST_STATUS_INCOMPLETE &&
                    player->GetExactDist(UndercityApothecariumApproach) < 18.0f)
                {
                    SummonApothecariumGuards(player);
                    return;
                }
        }

        void SummonApothecariumGuards(Player* player)
        {
            _stage = Stage::Guards;
            _stageTimer = 90s;
            if (!me->isActiveObject())
            {
                me->setActive(true);
                _activeForGuards = true;
            }

            for (Position const& position : { Position(1552.0f, 360.0f, -62.12f, 3.98f),
                Position(1552.0f, 364.0f, -62.12f, 3.98f) })
            {
                TempSummon* guard = me->SummonCreature(NPC_UC_FAILED_EXPERIMENT, position, TEMPSUMMON_TIMED_DESPAWN, _stageTimer);
                if (!guard)
                {
                    Reset();
                    return;
                }

                _guards.insert(guard->GetGUID());
                guard->SetFaction(UndercityHostileFaction);
                guard->SetMaxHealth(8500);
                guard->SetFullHealth();
                guard->AI()->AttackStart(player);
            }

            if (Creature* varian = me->FindNearestCreature(NPC_UC_VARIAN, 75.0f))
                varian->Say("Hold here and destroy the apothecary guards before advancing on Putress!", LANG_UNIVERSAL);
        }

        Stage _stage = Stage::Waiting;
        Milliseconds _stageTimer = 2s;
        std::set<ObjectGuid> _guards;
        bool _activeForGuards = false;
    };

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new boss_putress_battle_for_undercityAI(creature);
    }
};

/*######
## AddSC
######*/

void AddSC_undercity()
{
    new npc_lady_sylvanas_windrunner();
    new npc_highborne_lamenter();
    RegisterSpellScript(spell_undercity_bending_shinbone);

    // Battle for the Undercity & Zidormi
    new npc_zidormi_undercity();
    new npc_thrall_battle_for_undercity();
    new boss_varimathras_battle_for_undercity();
    new npc_sylvanas_battle_for_undercity();
    new npc_varian_battle_for_undercity();
    new boss_putress_battle_for_undercity();
}
