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
#include "CellImpl.h"
#include "Chat.h"
#include "CreatureTextMgr.h"
#include "GridNotifiersImpl.h"
#include "Map.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "SpellScript.h"
#include "TemporarySummon.h"

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
######*/

enum UndercityBattleData
{
    SPELL_BATTLE_FOR_UNDERCITY_PHASE    = 59062,
    SPELL_BATTLE_FOR_THE_UNDERCITY_BUFF = 59444,

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

    // Varimathras Spells
    SPELL_SHADOW_BOLT_VOLLEY            = 28599,
    SPELL_CARRION_SWARM                 = 31306,
    SPELL_RAIN_OF_FIRE                  = 11990,

    // Thrall Spells
    SPELL_THRALL_CHAIN_LIGHTNING        = 16033,
    SPELL_THRALL_SHOCK                  = 16034,

    // Sylvanas Spells
    SPELL_SYLVANAS_BLACK_ARROW          = 59712,
    SPELL_SYLVANAS_MULTI_SHOT           = 59713
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
constexpr uint32 UndercityEscortGas = 16363; // Grobbulus Cloud
constexpr uint32 UndercityHostileFaction = 14;

// The local assault follows a short route near its own entrance. The checkpoint
// controls access to the transfer, so no temporary door blocks unrelated players.
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
                AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Sunt gata să însoțesc escorta.", GOSSIP_SENDER_MAIN, StartAssault);
            else if (_stage == Stage::PassageOpen)
                AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Coboară-mă prin pasajul securizat.", GOSSIP_SENDER_MAIN, CrossPassage);
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

        if (pointId == FirstCheckpoint && MoveToCheckpoint(FinalCheckpoint))
            return;

        if (pointId == FirstCheckpoint || pointId == FinalCheckpoint)
            SetUpGasBarrier();
    }

    void SummonedCreatureDies(Creature* summon, Unit* /*killer*/) override
    {
        if (_stage != Stage::BarrierClosed || !_guards.erase(summon->GetGUID()) || !_guards.empty())
            return;

        _stage = Stage::PassageOpen;
        _remaining = 75s;
        ClearGas();
        me->Say("Gazul se risipește. Vorbiți cu mine ca să coborâți.", LANG_UNIVERSAL);
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
            return false;

        if (_remaining <= Milliseconds(diff))
        {
            if (_stage == Stage::PassageOpen)
                EndAssault();
            else
                AbortAssault();
        }
        else
            _remaining -= Milliseconds(diff);

        return true;
    }

private:
    enum class Stage { Idle, Moving, BarrierClosed, PassageOpen };
    enum : uint32
    {
        StartAssault = GOSSIP_ACTION_INFO_DEF + 1,
        CrossPassage = GOSSIP_ACTION_INFO_DEF + 2,
        FirstCheckpoint = 1,
        FinalCheckpoint = 2
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
            ChatHandler(player->GetSession()).SendSysMessage("Ieși din luptă înainte să pornești escorta.");
            return;
        }

        _originalReactState = me->GetReactState();
        _originalActive = me->isActiveObject();
        _stage = Stage::Moving;
        me->SetReactState(REACT_PASSIVE);
        me->setActive(true);
        if (!MoveToCheckpoint(FirstCheckpoint))
        {
            AbortAssault();
            return;
        }

        SayUndercityBattleText(me, _assaultText, _assaultFallback);
    }

    bool MoveToCheckpoint(uint32 pointId)
    {
        for (float angle : { 0.0f, 0.8f, -0.8f, 1.6f, -1.6f, 3.14f })
        {
            Position checkpoint = me->GetPosition();
            me->MovePositionToFirstCollision(checkpoint, 7.0f, angle);
            if (me->GetExactDist2d(&checkpoint) < 4.0f ||
                std::abs(checkpoint.GetPositionZ() - me->GetPositionZ()) > 2.0f ||
                !me->IsWithinLOS(checkpoint.GetPositionX(), checkpoint.GetPositionY(), checkpoint.GetPositionZ()))
                continue;

            // Avoid turning back to the starting position at the second point.
            if (pointId == FinalCheckpoint && me->GetHomePosition().GetExactDist2d(&checkpoint) < 7.0f)
                continue;

            _remaining = 18s;
            me->GetMotionMaster()->MovePoint(pointId, checkpoint);
            return true;
        }

        return false;
    }

    void SetUpGasBarrier()
    {
        _stage = Stage::BarrierClosed;
        _remaining = 55s;

        Position gasPosition = me->GetPosition();
        me->MovePositionToFirstCollision(gasPosition, 3.0f, 0.0f);
        TempSummon* gas = me->SummonCreature(UndercityEscortGas, gasPosition, TEMPSUMMON_TIMED_DESPAWN, _remaining);
        if (!gas)
        {
            AbortAssault();
            return;
        }

        _gas = gas->GetGUID();
        gas->SetPhaseMask(UndercityBattlePhase, true);
        gas->SetFaction(UndercityHostileFaction);

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

        me->Say("Pasajul e blocat de gaz. Eliminați cele două gărzi ca să-l putem deschide.", LANG_UNIVERSAL);
    }

    void TransferPlayer(Player* player) const
    {
        if (player->IsInCombat() || !player->IsWithinLOSInMap(me) || player->GetTransport())
        {
            ChatHandler(player->GetSession()).SendSysMessage("Apropie-te de escortă și ieși din luptă înainte de coborâre.");
            return;
        }

        Map* map = me->GetMap();
        map->LoadGrid(_destination.GetPositionX(), _destination.GetPositionY());
        float const landingZ = map->GetHeight(UndercityBattlePhase, _destination.GetPositionX(),
            _destination.GetPositionY(), _destination.GetPositionZ() + 3.0f, true, 8.0f);
        if (!std::isfinite(landingZ) || std::abs(landingZ - _destination.GetPositionZ()) > 4.0f)
        {
            ChatHandler(player->GetSession()).SendSysMessage("Locul de aterizare nu e sigur. Coborârea a fost oprită.");
            return;
        }

        bool const alreadyBuffed = player->HasAura(SPELL_BATTLE_FOR_THE_UNDERCITY_BUFF);
        if (!alreadyBuffed)
            player->CastSpell(player, SPELL_BATTLE_FOR_THE_UNDERCITY_BUFF, true);
        if (!player->TeleportTo(me->GetMapId(), _destination.GetPositionX(), _destination.GetPositionY(), landingZ + 0.2f, _destination.GetOrientation()))
        {
            if (!alreadyBuffed)
                player->RemoveAurasDueToSpell(SPELL_BATTLE_FOR_THE_UNDERCITY_BUFF);
            ChatHandler(player->GetSession()).SendSysMessage("Coborârea a eșuat. Încearcă din nou.");
        }
    }

    void ClearGas()
    {
        ObjectGuid const gas = _gas;
        _gas.Clear();
        if (Creature* cloud = ObjectAccessor::GetCreature(*me, gas))
            cloud->DespawnOrUnsummon();
    }

    void AbortAssault()
    {
        me->Say("Ne întoarcem la punctul de întâlnire. Putem încerca din nou.", LANG_UNIVERSAL);
        EndAssault();
    }

    void EndAssault()
    {
        if (_stage == Stage::Idle)
            return;

        _stage = Stage::Idle;
        ClearGas();
        for (ObjectGuid const& guid : _guards)
            if (Creature* guard = ObjectAccessor::GetCreature(*me, guid))
                guard->DespawnOrUnsummon();
        _guards.clear();
        me->SetReactState(_originalReactState);
        me->setActive(_originalActive);
        if (me->IsAlive())
            me->GetMotionMaster()->MoveTargetedHome();
    }

    uint32 const _quest;
    TeamId const _team;
    Position const _destination;
    UndercityBattleText const _assaultText;
    char const* const _assaultFallback;
    Stage _stage = Stage::Idle;
    Milliseconds _remaining = 0ms;
    ObjectGuid _gas;
    std::set<ObjectGuid> _guards;
    ReactStates _originalReactState = REACT_PASSIVE;
    bool _originalActive = false;
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
                AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Arata-mi Tirisfal Glades si Undercity in starea de pace (Iesi din faza de batalie).", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);
            }
            else if (hasHordeQuest || hasAllianceQuest || completedHorde || completedAlliance)
            {
                AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Intoarce-ma la Batalia pentru Undercity.", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 2);
            }

            if (hasHordeQuest || hasAllianceQuest)
            {
                AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Raport: Fortele noastre au triumfat! (Deblocheaza & Finalizeaza Questul).", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 3);
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
                    ChatHandler(player->GetSession()).SendSysMessage("Ai parasit faza Bataliei pentru Undercity. Orasul a revenit la starea normala.");
                    CloseGossipMenuFor(player);
                    break;
                case GOSSIP_ACTION_INFO_DEF + 2:
                    player->CastSpell(player, SPELL_BATTLE_FOR_UNDERCITY_PHASE, true);
                    player->SetPhaseMask(64, true);
                    ChatHandler(player->GetSession()).SendSysMessage("Ai intrat in faza Bataliei pentru Undercity!");
                    CloseGossipMenuFor(player);
                    break;
                case GOSSIP_ACTION_INFO_DEF + 3:
                    if (player->GetQuestStatus(QUEST_BATTLE_UNDERCITY_HORDE) == QUEST_STATUS_INCOMPLETE)
                        player->CompleteQuest(QUEST_BATTLE_UNDERCITY_HORDE);
                    if (player->GetQuestStatus(QUEST_BATTLE_UNDERCITY_ALLIANCE) == QUEST_STATUS_INCOMPLETE)
                        player->CompleteQuest(QUEST_BATTLE_UNDERCITY_ALLIANCE);
                    player->RemoveAurasDueToSpell(SPELL_BATTLE_FOR_UNDERCITY_PHASE);
                    player->SetPhaseMask(1, true);
                    ChatHandler(player->GetSession()).SendSysMessage("Questul pentru Batalia pentru Undercity a fost finalizat, iar faza a fost resetata cu succes!");
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
            : UndercityAssaultAI(creature, QUEST_BATTLE_UNDERCITY_HORDE, TEAM_HORDE, Position(1340.0f, 316.0f, -63.5f, 3.14f),
                SAY_UC_THRALL_ASSAULT, "Pentru Hoarda! Soldati, inaintam spre inima orasului! Nimic nu va ramane in calea noastra!")
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
                        SayUndercityBattleText(me, SAY_UC_THRALL_VICTORY, "Varimathras a fost infrant! Orasul Undercity apartine din nou Hoardei! Pentru Forsaken!");
                        break;
                    case 2:
                        if (Creature* varian = me->SummonCreature(NPC_UC_VARIAN_THRONE, 1315.0f, 316.0f, -63.5f, 3.14f, TEMPSUMMON_TIMED_DESPAWN, 20s))
                            SayUndercityBattleText(varian, SAY_UC_VARIAN_CONFRONTATION, "Thrall! Astazi cerem socoteala pentru Bolvar Fordragon si cadavrele de la Wrathgate!");
                        me->SummonCreature(NPC_UC_JAINA_THRONE, 1318.0f, 320.0f, -63.5f, 3.14f, TEMPSUMMON_TIMED_DESPAWN, 20s);
                        break;
                    case 3:
                        SayUndercityBattleText(me, SAY_UC_THRALL_RESPONSE, "Varian, retrage-ti oamenii! Tradatorul a fost pedepsit, iar ciuma a fost oprita. Nu incepe un razboi aici!");
                        break;
                    case 4:
                        if (Creature* jaina = me->FindNearestCreature(NPC_UC_JAINA_THRONE, 50.0f))
                        {
                            SayUndercityBattleText(jaina, SAY_UC_JAINA_INTERVENTION, "Opriti-va! Nu va mai curge sange astazi! Acest conflict nu va aduce pace nimanui!");
                            jaina->CastSpell(jaina, 55761, true);
                        }
                        if (Creature* varian = me->FindNearestCreature(NPC_UC_VARIAN_THRONE, 50.0f))
                            varian->DespawnOrUnsummon(1s);

                        // Reward quest to all players in the room
                        {
                            std::list<Player*> playerList;
                            Trinity::AnyPlayerInObjectRangeCheck checker(me, 75.0f);
                            Trinity::PlayerListSearcher<Trinity::AnyPlayerInObjectRangeCheck> searcher(me, playerList, checker);
                            Cell::VisitGridObjects(me, searcher, 75.0f);
                            for (Player* p : playerList)
                            {
                                if (p->GetQuestStatus(QUEST_BATTLE_UNDERCITY_HORDE) == QUEST_STATUS_INCOMPLETE)
                                    p->CompleteQuest(QUEST_BATTLE_UNDERCITY_HORDE);
                            }
                        }
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
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            SayUndercityBattleText(me, SAY_UC_VARIMATHRAS_AGGRO, "Ati intarziat, viermi! Puterea Legiunii este infinita! Acest oras ne apartine!");
        }

        void JustDied(Unit* /*killer*/) override
        {
            SayUndercityBattleText(me, SAY_UC_VARIMATHRAS_DEATH, "Blestemati sa fiti... Stapane, iarta-ma...");

            std::list<Player*> playerList;
            Trinity::AnyPlayerInObjectRangeCheck checker(me, 75.0f);
            Trinity::PlayerListSearcher<Trinity::AnyPlayerInObjectRangeCheck> searcher(me, playerList, checker);
            Cell::VisitGridObjects(me, searcher, 75.0f);
            for (Player* p : playerList)
            {
                if (p->GetQuestStatus(QUEST_BATTLE_UNDERCITY_HORDE) == QUEST_STATUS_INCOMPLETE)
                    p->CompleteQuest(QUEST_BATTLE_UNDERCITY_HORDE);
            }

            if (Creature* thrall = me->FindNearestCreature(NPC_UC_THRALL, 75.0f))
                thrall->AI()->DoAction(1);
        }

        void UpdateAI(uint32 diff) override
        {
            if (!UpdateVictim())
                return;

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
                    me->SummonCreature(14751, me->GetPositionX() + frand(-5.0f, 5.0f), me->GetPositionY() + frand(-5.0f, 5.0f), me->GetPositionZ(), 0.0f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 60s);
            }

            DoMeleeAttackIfReady();
        }
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
            : UndercityAssaultAI(creature, QUEST_BATTLE_UNDERCITY_ALLIANCE, TEAM_ALLIANCE, Position(1543.31f, 362.002f, -62.1596f, 0.0f),
                SAY_UC_VARIAN_ASSAULT, "Pentru Bolvar! Pentru Alianta! Asaltati canalele si ucideti tradatorii!") { }

        void UpdateAI(uint32 diff) override
        {
            if (!UpdateAssault(diff) && UpdateVictim())
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
            PotionTimer = 4000;
            SummonPercent = 75;
        }

        uint32 PotionTimer;
        uint32 SummonPercent;

        void Reset() override
        {
            PotionTimer = 4000;
            SummonPercent = 75;
        }

        void JustEngagedWith(Unit* /*who*/) override
        {
            SayUndercityBattleText(me, SAY_UC_PUTRESS_AGGRO, "Ati crezut ca suntem neputinciosi? Ciuma noastra va curata aceasta lume!");
        }

        void JustDied(Unit* /*killer*/) override
        {
            SayUndercityBattleText(me, SAY_UC_PUTRESS_DEATH, "Munca mea... nu se va opri... niciodata...");

            std::list<Player*> playerList;
            Trinity::AnyPlayerInObjectRangeCheck checker(me, 75.0f);
            Trinity::PlayerListSearcher<Trinity::AnyPlayerInObjectRangeCheck> searcher(me, playerList, checker);
            Cell::VisitGridObjects(me, searcher, 75.0f);
            for (Player* p : playerList)
            {
                if (p->GetQuestStatus(QUEST_BATTLE_UNDERCITY_ALLIANCE) == QUEST_STATUS_INCOMPLETE)
                    p->CompleteQuest(QUEST_BATTLE_UNDERCITY_ALLIANCE);
            }
        }

        void UpdateAI(uint32 diff) override
        {
            if (!UpdateVictim())
                return;

            if (PotionTimer <= diff)
            {
                DoCastVictim(SPELL_SHADOW_BOLT_VOLLEY);
                PotionTimer = 8000;
            }
            else PotionTimer -= diff;

            if (HealthBelowPct(SummonPercent))
            {
                SummonPercent -= 25;
                for (uint8 i = 0; i < 2; ++i)
                    me->SummonCreature(14751, me->GetPositionX() + frand(-5.0f, 5.0f), me->GetPositionY() + frand(-5.0f, 5.0f), me->GetPositionZ(), 0.0f, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 60s);
            }

            DoMeleeAttackIfReady();
        }
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
