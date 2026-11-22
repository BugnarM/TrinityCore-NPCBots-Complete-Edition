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

#include "ScriptMgr.h"
#include "Battlefield.h"
#include "bot_ai.h"
#include "botdatamgr.h"
#include "botwanderful.h"
#include "BattlefieldMgr.h"
#include "Battlefield/BattlefieldWG.h"
#include "CellImpl.h"
#include "ChatPackets.h"
#include "Config.h"
#include "TemporarySummon.h"
#include "wintergrasp_bot_war.h"
#include "wintergrasp_tactics.h"
#include "wintergrasp_vehicle_navigation.h"
#include <array>
#include <map>
#include <cmath>
#include <limits>
#include "DBCStores.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "GameTime.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "MapManager.h"
#include "Map.h"
#include "Log.h"
#include "MotionMaster.h"
#include "MMapFactory.h"
#include "MoveSplineInit.h"
#include "PathGenerator.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "ScriptSystem.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "Spell.h"
#include "SpellHistory.h"
#include "SpellAuraEffects.h"
#include "SpellScript.h"
#include "Vehicle.h"
#include "World.h"
#include "WorldSession.h"

#define GOSSIP_HELLO_DEMO1  "Build catapult."
#define GOSSIP_HELLO_DEMO2  "Build demolisher."
#define GOSSIP_HELLO_DEMO3  "Build siege engine."
#define GOSSIP_HELLO_DEMO4  "I cannot build more!"

enum WGqueuenpctext
{
    WG_NPCQUEUE_TEXT_H_NOWAR            = 14775,
    WG_NPCQUEUE_TEXT_H_QUEUE            = 14790,
    WG_NPCQUEUE_TEXT_H_WAR              = 14777,
    WG_NPCQUEUE_TEXT_A_NOWAR            = 14782,
    WG_NPCQUEUE_TEXT_A_QUEUE            = 14791,
    WG_NPCQUEUE_TEXT_A_WAR              = 14781,
    WG_NPCQUEUE_TEXTOPTION_JOIN         = 20077,
};

enum Spells
{
    // Demolisher engineers spells
    SPELL_BUILD_SIEGE_VEHICLE_FORCE_HORDE     = 61409,
    SPELL_BUILD_SIEGE_VEHICLE_FORCE_ALLIANCE  = 56662,
    SPELL_BUILD_CATAPULT_FORCE                = 56664,
    SPELL_BUILD_DEMOLISHER_FORCE              = 56659,
    SPELL_ACTIVATE_CONTROL_ARMS               = 49899,
    SPELL_RIDE_WG_VEHICLE                     = 60968,

    SPELL_VEHICLE_TELEPORT                    = 49759,

    // Spirit guide
    SPELL_CHANNEL_SPIRIT_HEAL                 = 22011,
};

enum CreatureIds
{
    NPC_GOBLIN_MECHANIC                             = 30400,
    NPC_GNOMISH_ENGINEER                            = 30499,

    NPC_WINTERGRASP_CONTROL_ARMS                    = 27852,

    NPC_WORLD_TRIGGER_LARGE_AOI_NOT_IMMUNE_PC_NPC   = 23472,
};

enum QuestIds
{
    QUEST_BONES_AND_ARROWS_HORDE_ATT              = 13193,
    QUEST_JINXING_THE_WALLS_HORDE_ATT             = 13202,
    QUEST_SLAY_THEM_ALL_HORDE_ATT                 = 13180,
    QUEST_FUELING_THE_DEMOLISHERS_HORDE_ATT       = 13200,
    QUEST_HEALING_WITH_ROSES_HORDE_ATT            = 13201,
    QUEST_DEFEND_THE_SIEGE_HORDE_ATT              = 13223,

    QUEST_BONES_AND_ARROWS_HORDE_DEF              = 13199,
    QUEST_WARDING_THE_WALLS_HORDE_DEF             = 13192,
    QUEST_SLAY_THEM_ALL_HORDE_DEF                 = 13178,
    QUEST_FUELING_THE_DEMOLISHERS_HORDE_DEF       = 13191,
    QUEST_HEALING_WITH_ROSES_HORDE_DEF            = 13194,
    QUEST_TOPPLING_THE_TOWERS_HORDE_DEF           = 13539,
    QUEST_STOP_THE_SIEGE_HORDE_DEF                = 13185,

    QUEST_BONES_AND_ARROWS_ALLIANCE_ATT           = 13196,
    QUEST_WARDING_THE_WARRIORS_ALLIANCE_ATT       = 13198,
    QUEST_NO_MERCY_FOR_THE_MERCILESS_ALLIANCE_ATT = 13179,
    QUEST_DEFEND_THE_SIEGE_ALLIANCE_ATT           = 13222,
    QUEST_A_RARE_HERB_ALLIANCE_ATT                = 13195,

    QUEST_BONES_AND_ARROWS_ALLIANCE_DEF           = 13154,
    QUEST_WARDING_THE_WARRIORS_ALLIANCE_DEF       = 13153,
    QUEST_NO_MERCY_FOR_THE_MERCILESS_ALLIANCE_DEF = 13177,
    QUEST_SHOUTHERN_SABOTAGE_ALLIANCE_DEF         = 13538,
    QUEST_STOP_THE_SIEGE_ALLIANCE_DEF             = 13186,
    QUEST_A_RARE_HERB_ALLIANCE_DEF                = 13156,
};

uint8 const MAX_WINTERGRASP_VEHICLES = 4;

uint32 const vehiclesList[MAX_WINTERGRASP_VEHICLES] =
{
    NPC_WINTERGRASP_CATAPULT,
    NPC_WINTERGRASP_DEMOLISHER,
    NPC_WINTERGRASP_SIEGE_ENGINE_ALLIANCE,
    NPC_WINTERGRASP_SIEGE_ENGINE_HORDE
};

struct npc_wg_demolisher_engineer : public ScriptedAI
{
    npc_wg_demolisher_engineer(Creature* creature) : ScriptedAI(creature) { }

    bool OnGossipHello(Player* player) override
    {
        if (me->IsQuestGiver())
            player->PrepareQuestMenu(me->GetGUID());

        if (CanBuild())
        {
            if (player->HasAura(SPELL_CORPORAL))
                AddGossipItemFor(player, GOSSIP_ICON_CHAT, GOSSIP_HELLO_DEMO1, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF);
            else if (player->HasAura(SPELL_LIEUTENANT))
            {
                AddGossipItemFor(player, GOSSIP_ICON_CHAT, GOSSIP_HELLO_DEMO1, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF);
                AddGossipItemFor(player, GOSSIP_ICON_CHAT, GOSSIP_HELLO_DEMO2, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);
                AddGossipItemFor(player, GOSSIP_ICON_CHAT, GOSSIP_HELLO_DEMO3, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 2);
            }
        }
        else
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, GOSSIP_HELLO_DEMO4, GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 9);

        SendGossipMenuFor(player, player->GetGossipTextId(me), me->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
    {
        uint32 const action = player->PlayerTalkClass->GetGossipOptionAction(gossipListId);
        CloseGossipMenuFor(player);

        if (CanBuild())
        {
            switch (action - GOSSIP_ACTION_INFO_DEF)
            {
                case 0:
                    DoCast(player, SPELL_BUILD_CATAPULT_FORCE, true);
                    break;
                case 1:
                    DoCast(player, SPELL_BUILD_DEMOLISHER_FORCE, true);
                    break;
                case 2:
                    DoCast(player, player->GetTeamId() == TEAM_ALLIANCE ? SPELL_BUILD_SIEGE_VEHICLE_FORCE_ALLIANCE : SPELL_BUILD_SIEGE_VEHICLE_FORCE_HORDE, true);
                    break;
            }
            if (Creature* controlArms = me->FindNearestCreature(NPC_WINTERGRASP_CONTROL_ARMS, 30.0f, true))
                DoCast(controlArms, SPELL_ACTIVATE_CONTROL_ARMS, true);
        }
        return true;
    }

private:
    bool CanBuild() const
    {
        Battlefield* wintergrasp = sBattlefieldMgr->GetBattlefieldByBattleId(BATTLEFIELD_BATTLEID_WG);
        if (!wintergrasp)
            return false;

        switch (me->GetEntry())
        {
            case NPC_GOBLIN_MECHANIC:
                return (wintergrasp->GetData(BATTLEFIELD_WG_DATA_MAX_VEHICLE_H) > wintergrasp->GetData(BATTLEFIELD_WG_DATA_VEHICLE_H));
            case NPC_GNOMISH_ENGINEER:
                return (wintergrasp->GetData(BATTLEFIELD_WG_DATA_MAX_VEHICLE_A) > wintergrasp->GetData(BATTLEFIELD_WG_DATA_VEHICLE_A));
            default:
                return false;
        }
    }
};

struct npc_wg_spirit_guide : public ScriptedAI
{
    npc_wg_spirit_guide(Creature* creature) : ScriptedAI(creature) { }

    void UpdateAI(uint32 /*diff*/) override
    {
        if (!me->HasUnitState(UNIT_STATE_CASTING))
            DoCast(me, SPELL_CHANNEL_SPIRIT_HEAL);
    }

    bool OnGossipHello(Player* player) override
    {
        if (me->IsQuestGiver())
            player->PrepareQuestMenu(me->GetGUID());

        Battlefield* wintergrasp = sBattlefieldMgr->GetBattlefieldByBattleId(BATTLEFIELD_BATTLEID_WG);
        if (!wintergrasp)
            return true;

        GraveyardVect graveyard = wintergrasp->GetGraveyardVector();
        for (uint8 i = 0; i < graveyard.size(); i++)
            if (graveyard[i]->GetControlTeamId() == player->GetTeamId())
                AddGossipItemFor(player, GOSSIP_ICON_CHAT, player->GetSession()->GetTrinityString(((BfGraveyardWG*)graveyard[i])->GetTextId()), GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + i);

        SendGossipMenuFor(player, player->GetGossipTextId(me), me->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
    {
        uint32 const action = player->PlayerTalkClass->GetGossipOptionAction(gossipListId);
        CloseGossipMenuFor(player);

        Battlefield* wintergrasp = sBattlefieldMgr->GetBattlefieldByBattleId(BATTLEFIELD_BATTLEID_WG);
        if (wintergrasp)
        {
            GraveyardVect gy = wintergrasp->GetGraveyardVector();
            for (uint8 i = 0; i < gy.size(); i++)
                if (action - GOSSIP_ACTION_INFO_DEF == i && gy[i]->GetControlTeamId() == player->GetTeamId())
                    if (WorldSafeLocsEntry const* safeLoc = sWorldSafeLocsStore.LookupEntry(gy[i]->GetGraveyardId()))
                        player->TeleportTo(safeLoc->Continent, safeLoc->Loc.X, safeLoc->Loc.Y, safeLoc->Loc.Z, 0);
        }
        return true;
    }
};

enum WGQueue
{
    SPELL_FROST_ARMOR                               = 12544
};

struct npc_wg_queue : public ScriptedAI
{
    npc_wg_queue(Creature* creature) : ScriptedAI(creature)
    {
        FrostArmor_Timer = 0;
    }

    uint32 FrostArmor_Timer;

    void Reset() override
    {
        FrostArmor_Timer = 0;
    }

    void JustEngagedWith(Unit* /*who*/) override { }

    void UpdateAI(uint32 diff) override
    {
        if (FrostArmor_Timer <= diff)
        {
            DoCast(me, SPELL_FROST_ARMOR);
            FrostArmor_Timer = 180000;
        }
        else FrostArmor_Timer -= diff;

        DoMeleeAttackIfReady();
    }

    bool OnGossipHello(Player* player) override
    {
        if (me->IsQuestGiver())
            player->PrepareQuestMenu(me->GetGUID());

        Battlefield* wintergrasp = sBattlefieldMgr->GetBattlefieldByBattleId(BATTLEFIELD_BATTLEID_WG);
        if (!wintergrasp)
            return true;

        if (wintergrasp->IsWarTime())
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, player->GetSession()->GetTrinityString(WG_NPCQUEUE_TEXTOPTION_JOIN), GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF);
            SendGossipMenuFor(player, wintergrasp->GetDefenderTeam() ? WG_NPCQUEUE_TEXT_H_WAR : WG_NPCQUEUE_TEXT_A_WAR, me->GetGUID());
        }
        else
        {
            uint32 timer = wintergrasp->GetTimer() / 1000;
            player->SendUpdateWorldState(4354, GameTime::GetGameTime() + timer);
            if (timer < 15 * MINUTE)
            {
                AddGossipItemFor(player, GOSSIP_ICON_CHAT, player->GetSession()->GetTrinityString(WG_NPCQUEUE_TEXTOPTION_JOIN), GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF);
                SendGossipMenuFor(player, wintergrasp->GetDefenderTeam() ? WG_NPCQUEUE_TEXT_H_QUEUE : WG_NPCQUEUE_TEXT_A_QUEUE, me->GetGUID());
            }
            else
                SendGossipMenuFor(player, wintergrasp->GetDefenderTeam() ? WG_NPCQUEUE_TEXT_H_NOWAR : WG_NPCQUEUE_TEXT_A_NOWAR, me->GetGUID());
        }
        return true;
    }

    bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 /*gossipListId*/) override
    {
        CloseGossipMenuFor(player);

        Battlefield* wintergrasp = sBattlefieldMgr->GetBattlefieldByBattleId(BATTLEFIELD_BATTLEID_WG);
        if (!wintergrasp)
            return true;

        if (wintergrasp->IsWarTime())
            wintergrasp->InvitePlayerToWar(player);
        else
        {
            uint32 timer = wintergrasp->GetTimer() / 1000;
            if (timer < 15 * MINUTE)
                wintergrasp->InvitePlayerToQueue(player);
        }
        return true;
    }
};

struct go_wg_vehicle_teleporter : public GameObjectAI
{
    go_wg_vehicle_teleporter(GameObject* gameObject) : GameObjectAI(gameObject), _checkTimer(0) { }

    bool IsFriendly(Unit* passenger)
    {
        return ((me->GetFaction() == FACTION_HORDE_GENERIC_WG && passenger->GetFaction() == HORDE) ||
                (me->GetFaction() == FACTION_ALLIANCE_GENERIC_WG && passenger->GetFaction() == ALLIANCE));
    }

    Creature* GetValidVehicle(Creature* cVeh)
    {
        if (!cVeh->HasAura(SPELL_VEHICLE_TELEPORT))
            if (Vehicle* vehicle = cVeh->GetVehicleKit())
                if (Unit* passenger = vehicle->GetPassenger(0))
                    if (IsFriendly(passenger))
                        if (Creature* teleportTrigger = passenger->SummonTrigger(me->GetPositionX()-60.0f, me->GetPositionY(), me->GetPositionZ()+1.0f, cVeh->GetOrientation(), 1s))
                            return teleportTrigger;

        return nullptr;
    }

    void UpdateAI(uint32 diff) override
    {
        _checkTimer += diff;
        if (_checkTimer >= 1000)
        {
            for (uint8 i = 0; i < MAX_WINTERGRASP_VEHICLES; i++)
                if (Creature* vehicleCreature = me->FindNearestCreature(vehiclesList[i], 3.0f, true))
                    if (Creature* teleportTrigger = GetValidVehicle(vehicleCreature))
                        teleportTrigger->CastSpell(vehicleCreature, SPELL_VEHICLE_TELEPORT, true);

            _checkTimer = 0;
        }
    }

private:
    uint32 _checkTimer;
};

/* 49899 - Activate Robotic Arms
   56659 - Build Demolisher (Force)
   56662 - Build Siege Vehicle (Force)
   56664 - Build Catapult (Force)
   61409 - Build Siege Vehicle (Force) */
class spell_wintergrasp_force_building : public SpellScript
{
    PrepareSpellScript(spell_wintergrasp_force_building);

    bool Validate(SpellInfo const* /*spell*/) override
    {
        return ValidateSpellInfo(
        {
            SPELL_BUILD_CATAPULT_FORCE,
            SPELL_BUILD_DEMOLISHER_FORCE,
            SPELL_BUILD_SIEGE_VEHICLE_FORCE_HORDE,
            SPELL_BUILD_SIEGE_VEHICLE_FORCE_ALLIANCE
        });
    }

    void HandleScript(SpellEffIndex effIndex)
    {
        PreventHitDefaultEffect(effIndex);
        GetHitUnit()->CastSpell(GetHitUnit(), GetEffectValue(), false);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_wintergrasp_force_building::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// 61178 - Grab Passenger
class spell_wintergrasp_grab_passenger : public SpellScript
{
    PrepareSpellScript(spell_wintergrasp_grab_passenger);

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        if (Player* target = GetHitPlayer())
            target->CastSpell(GetCaster(), SPELL_RIDE_WG_VEHICLE, false);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_wintergrasp_grab_passenger::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// 49899 - Activate Robotic Arms
class spell_wintergrasp_activate_robotic_arms : public SpellScript
{
    PrepareSpellScript(spell_wintergrasp_activate_robotic_arms);

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        GetHitUnit()->HandleEmoteCommand(EMOTE_ONESHOT_CUSTOM_SPELL_05);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_wintergrasp_activate_robotic_arms::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

class achievement_wg_didnt_stand_a_chance : public AchievementCriteriaScript
{
    public:
        achievement_wg_didnt_stand_a_chance() : AchievementCriteriaScript("achievement_wg_didnt_stand_a_chance") { }

        bool OnCheck(Player* source, Unit* target) override
        {
            if (!target)
                return false;

            if (Player* victim = target->ToPlayer())
            {
                if (!victim->IsMounted())
                    return false;

                if (Vehicle* vehicle = source->GetVehicle())
                    if (vehicle->GetVehicleInfo()->ID == 244) // Wintergrasp Tower Cannon
                        return true;
            }

            return false;
        }
};

enum WgTeleport
{
    SPELL_WINTERGRASP_TELEPORT_TRIGGER = 54643,
};

// 54640 - Teleport
class spell_wintergrasp_defender_teleport : public SpellScript
{
    PrepareSpellScript(spell_wintergrasp_defender_teleport);

    SpellCastResult CheckCast()
    {
        if (Battlefield* wg = sBattlefieldMgr->GetBattlefieldByBattleId(BATTLEFIELD_BATTLEID_WG))
            if (Player* target = GetExplTargetUnit()->ToPlayer())
                // check if we are in Wintergrasp at all, SotA uses same teleport spells
                if ((target->GetZoneId() == AREA_WINTERGRASP && target->GetTeamId() != wg->GetDefenderTeam()) || target->HasAura(SPELL_WINTERGRASP_TELEPORT_TRIGGER))
                    return SPELL_FAILED_BAD_TARGETS;
        return SPELL_CAST_OK;
    }

    void Register() override
    {
        OnCheckCast += SpellCheckCastFn(spell_wintergrasp_defender_teleport::CheckCast);
    }
};

// 54643 - Teleport
class spell_wintergrasp_defender_teleport_trigger : public SpellScript
{
    PrepareSpellScript(spell_wintergrasp_defender_teleport_trigger);

    void HandleDummy(SpellEffIndex /*effindex*/)
    {
        if (Unit* target = GetHitUnit())
        {
            WorldLocation loc = target->GetWorldLocation();
            SetExplTargetDest(loc);
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_wintergrasp_defender_teleport_trigger::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 58549 - Tenacity
// 59911 - Tenacity
class spell_wintergrasp_tenacity_refresh : public AuraScript
{
    PrepareAuraScript(spell_wintergrasp_tenacity_refresh);

    bool Validate(SpellInfo const* spellInfo) override
    {
        uint32 triggeredSpellId = spellInfo->GetEffect(EFFECT_2).CalcValue();
        return !triggeredSpellId || ValidateSpellInfo({ triggeredSpellId });
    }

    void Refresh(AuraEffect const* aurEff)
    {
        PreventDefaultAction();

        if (uint32 triggeredSpellId = aurEff->GetSpellEffectInfo().CalcValue())
        {
            int32 bp = 0;
            if (AuraEffect const* healEffect = GetEffect(EFFECT_0))
                bp = healEffect->GetAmount();

            CastSpellExtraArgs args(aurEff);
            args
                .AddSpellMod(SPELLVALUE_BASE_POINT0, bp)
                .AddSpellMod(SPELLVALUE_BASE_POINT1, bp);
            GetTarget()->CastSpell(nullptr, triggeredSpellId, args);
        }

        RefreshDuration();
    }

    void OnRemove(AuraEffect const* aurEff, AuraEffectHandleModes /*mode*/)
    {
        if (uint32 triggeredSpellId = aurEff->GetSpellEffectInfo().CalcValue())
            GetTarget()->RemoveAurasDueToSpell(triggeredSpellId);
    }

    void Register() override
    {
        OnEffectPeriodic += AuraEffectPeriodicFn(spell_wintergrasp_tenacity_refresh::Refresh, EFFECT_2, SPELL_AURA_PERIODIC_DUMMY);
        AfterEffectRemove += AuraEffectRemoveFn(spell_wintergrasp_tenacity_refresh::OnRemove, EFFECT_2, SPELL_AURA_PERIODIC_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }
};

class condition_is_wintergrasp_horde : public ConditionScript
{
    public:
        condition_is_wintergrasp_horde() : ConditionScript("condition_is_wintergrasp_horde") { }

        bool OnConditionCheck(Condition const* /* condition */, ConditionSourceInfo& /* sourceInfo */) override
        {
            Battlefield* wintergrasp = sBattlefieldMgr->GetBattlefieldByBattleId(BATTLEFIELD_BATTLEID_WG);
            if (wintergrasp && wintergrasp->IsEnabled() && wintergrasp->GetDefenderTeam() == TEAM_HORDE)
                return true;
            return false;
        }
};

class condition_is_wintergrasp_alliance : public ConditionScript
{
    public:
        condition_is_wintergrasp_alliance() : ConditionScript("condition_is_wintergrasp_alliance") { }

        bool OnConditionCheck(Condition const* /* condition */, ConditionSourceInfo& /* sourceInfo */) override
        {
            Battlefield* wintergrasp = sBattlefieldMgr->GetBattlefieldByBattleId(BATTLEFIELD_BATTLEID_WG);
            if (wintergrasp && wintergrasp->IsEnabled() && wintergrasp->GetDefenderTeam() == TEAM_ALLIANCE)
                return true;
            return false;
        }
};

namespace
{
using namespace WintergraspBotWar;

uint32 const WGFactionTable[] = { FACTION_ALLIANCE_GENERIC_WG, FACTION_HORDE_GENERIC_WG };

BattlefieldWG* GetBotWarBattlefield()
{
    Battlefield* bf = sBattlefieldMgr->GetBattlefieldByBattleId(BATTLEFIELD_BATTLEID_WG);
    return bf && bf->IsEnabled() && bf->IsWarTime()
        && sConfigMgr->GetBoolDefault("Wintergrasp.BotWar.Enable", false, true) ? static_cast<BattlefieldWG*>(bf) : nullptr;
}

bool IsBotWarSoldier(Unit const* unit)
{
    return unit && unit->IsCreature() && ((unit->GetEntry() >= 80501 && unit->GetEntry() <= 80508)
        || (unit->IsNPCBot() && unit->ToCreature()->GetBotAI()->IsBattlefieldBot()));
}

bool HasBotWarHumanPassenger(Creature* vehicle)
{
    Vehicle* kit = vehicle->GetVehicleKit();
    if (!kit)
        return false;
    for (auto const& seat : kit->Seats)
        if (Unit* passenger = kit->GetPassenger(seat.first))
        {
            if (passenger->ToPlayer())
                return true;
            if (Vehicle* turret = passenger->GetVehicleKit())
                for (auto const& turretSeat : turret->Seats)
                    if (Unit* gunner = turret->GetPassenger(turretSeat.first))
                        if (gunner->ToPlayer())
                            return true;
        }
    return false;
}

bool IsBotWarSeatAvailable(Vehicle const* kit, int8 seat)
{
    if (!kit)
        return false;
    // A ride spell queues the actual seat change. Skip seats with a pending join as well.
    auto next = kit->GetNextEmptySeat(seat, true);
    return next != kit->Seats.end() && next->first == seat;
}

bool YieldBotWarSeat(Creature* vehicle)
{
    Vehicle* kit = vehicle->GetVehicleKit();
    Unit* passenger = kit ? kit->GetPassenger(0) : nullptr;
    if (!IsBotWarSoldier(passenger))
        return false;
    std::list<Player*> players;
    GetPlayerListInGrid(players, vehicle, 8.f);
    for (Player* player : players)
        if (player->IsAlive() && player->IsFriendlyTo(vehicle) && !player->GetVehicle()
            && !player->IsInCombat() && !player->IsMounted() && vehicle->IsWithinLOSInMap(player))
        {
            passenger->ToCreature()->AI()->DoAction(1);
            passenger->ExitVehicle();
            return true;
        }
    return false;
}

void SetBotWarMovementSpeed(Creature* bot, bool mounted)
{
    bot->SetWalk(false);
    // SetSpeed takes metres/second; SetSpeedRate takes the multiplier of the
    // normal 7 m/s run speed. Epic ground mounts provide a 100% speed bonus.
    bot->SetSpeedRate(MOVE_RUN, mounted ? 2.f : 1.f);
}

uint32 GetWintergraspMount(Creature* bot)
{
    bool isAlliance = (bot->GetFaction() == WGFactionTable[TEAM_ALLIANCE]);
    uint32 seed = bot->GetGUID().GetCounter() + bot->GetEntry();
    if (isAlliance)
    {
        static constexpr uint32 allianceMounts[] = { 2784, 14584, 2787, 14332, 6569, 16992, 14337 };
        return allianceMounts[seed % 7];
    }
    else
    {
        static constexpr uint32 hordeMounts[] = { 14334, 14578, 14374, 14349, 14376, 14338 };
        return hordeMounts[seed % 6];
    }
}

WintergraspBotWar::Lane ResolveSquadLane(BattlefieldWG* wg, bool attacker, uint32 slot)
{
    Lane lane = SquadLane(attacker, slot);
    if (IsTowerDefense(lane))
        if (GameObject* tower = wg->GetBotWarBuilding(GuardedTowerEntry(lane)))
            lane = ResolveTowerDefense(lane, tower->GetDestructibleState() == GO_DESTRUCTIBLE_DESTROYED);
    return lane;
}

struct BotCombatFocus { ObjectGuid Target; uint32 Until=0; };
std::array<std::array<BotCombatFocus, LaneCount>,2> BotFocus;
std::map<ObjectGuid,uint32> BotInterrupts, BotControls;
uint32 BotCombatSerial=0, BotCombatPrune=0;

void UpdateCombatReservations()
{
    if (BotCombatSerial != BotBattleStats.Serial)
    {
        BotCombatSerial = BotBattleStats.Serial;
        BotFocus = {};
        BotInterrupts.clear();
        BotControls.clear();
        BotCombatPrune = 0;
    }
    if (BotBattleStats.Elapsed - BotCombatPrune >= 15000)
    {
        BotCombatPrune = BotBattleStats.Elapsed;
        auto expired = [](auto const& entry) { return CanReserveAction(BotBattleStats.Elapsed, entry.second); };
        std::erase_if(BotInterrupts, expired);
        std::erase_if(BotControls, expired);
    }
}
bool IsInterruptAbility(uint32 spell)
{
    return spell==6552 || spell==2139 || spell==57994 || spell==1766 || spell==34490;
}
bool IsControlAbility(uint32 spell)
{
    return spell==5246 || spell==10890 || spell==42917 || spell==10308 || spell==6215 || spell==47860;
}
bool HasBreakableControlNear(Unit* center, Unit* caster, float range)
{
    std::list<Unit*> units;
    Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(center,caster,range);
    Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(center,units,check);
    Cell::VisitAllObjects(center,searcher,range);
    return std::any_of(units.begin(),units.end(),[](Unit* unit) {
        return unit->IsAlive() && unit->HasUnitState(UNIT_STATE_FLEEING | UNIT_STATE_CONFUSED);
    });
}

bool BotAbilityAvailable(Creature* caster, Unit* target, uint32 spell)
{
    UpdateCombatReservations();
    if (target==caster && IsControlAbility(spell) && HasBreakableControlNear(caster,caster,10.f)) return false;
    if (target == caster) target = caster->GetVictim();
    if (!target || (!IsInterruptAbility(spell) && !IsControlAbility(spell))) return true;
    if (IsControlAbility(spell) && target->HasUnitState(UNIT_STATE_CONTROLLED | UNIT_STATE_FLEEING | UNIT_STATE_CONFUSED))
        return false;
    auto const& reservations = IsInterruptAbility(spell) ? BotInterrupts : BotControls;
    auto it = reservations.find(target->GetGUID());
    return it == reservations.end() || CanReserveAction(BotBattleStats.Elapsed, it->second);
}
void RecordBotAbility(Creature* caster, Unit* target, uint32 spell)
{
    if (target == caster) target = caster->GetVictim();
    if (!target || (!IsInterruptAbility(spell) && !IsControlAbility(spell))) return;
    auto& reservations = IsInterruptAbility(spell) ? BotInterrupts : BotControls;
    reservations[target->GetGUID()] = BotBattleStats.Elapsed + (IsInterruptAbility(spell) ? 1500 : 3000);
    if (BotBattleStats.Active)
    {
        auto& count = BotBattleStats.Teams[caster->GetFaction() == WGFactionTable[TEAM_ALLIANCE] ? 0 : 1];
        if (IsInterruptAbility(spell)) ++count.Interrupts;
        else ++count.Controls;
    }
}
bool IsBotWarHealer(Unit* unit)
{
    if (!unit) return false;
    if (IsBotWarSoldier(unit)) return IsHealer(unit->ToCreature()->AI()->GetData(ProfileData));
    if (unit->ToPlayer())
        if (Spell* spell = unit->GetCurrentSpell(CURRENT_GENERIC_SPELL))
            return spell->GetSpellInfo()->IsPositive() && (unit->GetClass()==CLASS_PRIEST
                || unit->GetClass()==CLASS_PALADIN || unit->GetClass()==CLASS_SHAMAN || unit->GetClass()==CLASS_DRUID);
    return false;
}

struct BotWaypoint
{
    Position Pos;
    uint32 Barrier = 0;
    uint32 CaptureArea = 0;
};

// Checkpoints are on the approach side of each obstacle. A missing obstacle is
// treated as closed: unloaded buildings must never let a bot skip the siege.
std::vector<BotWaypoint> const BotRoutes[LaneCount] =
{
    { // Center
        { {4452.86f, 2808.87f, 402.973f, 0.f} },
        { {4550.00f, 2840.00f, 389.182f, 0.f} },
        { {4700.00f, 2830.00f, 383.395f, 0.f} },
        { {4750.00f, 2840.00f, 361.242f, 0.f} },
        { {4920.00f, 2840.00f, 381.534f, 0.f} },
        { {5050.00f, 2840.00f, 393.468f, 0.f} },
        { {5135.00f, 2841.00f, 408.163f, 0.f}, 190375 },
        { {5200.00f, 2841.00f, 409.346f, 0.f} },
        { {5250.00f, 2841.00f, 409.353f, 0.f}, 191805 },
        { {5320.00f, 2841.00f, 409.300f, 0.f} },
        { {5370.00f, 2841.00f, 410.035f, 0.f}, 191810 },
        { {5420.00f, 2840.50f, 419.702f, 0.f} },
        { {5440.38f, 2840.49f, 420.589f, 0.f} }
    },
    { // West: pass through Westpark Workshop bridge, Broken Temple, outer wall, inner wall, vault gate.
        { {4390.78f, 3304.09f, 372.482f, 0.f} },
        { {4585.00f, 3450.00f, 360.942f, 0.f} },
        { {4738.93f, 3501.33f, 352.215f, 0.f} },
        { {4850.00f, 3420.00f, 356.788f, 0.f} },
        { {4948.524f, 3342.338f, 377.051f, 0.f}, 0, AREA_THE_BROKEN_TEMPLATE },
        { {5020.53f, 3246.13f, 365.567f, 0.f} },
        { {5100.00f, 3100.00f, 367.683f, 0.f} },
        { {5180.00f, 2934.00f, 409.300f, 0.f}, 190371 },
        { {5235.00f, 2934.00f, 409.300f, 0.f} },
        { {5250.00f, 2841.00f, 409.353f, 0.f}, 191805 },
        { {5320.00f, 2841.00f, 409.300f, 0.f} },
        { {5370.00f, 2841.00f, 410.035f, 0.f}, 191810 },
        { {5420.00f, 2840.50f, 419.702f, 0.f} },
        { {5440.38f, 2840.49f, 420.589f, 0.f} }
    },
    { // East: pass through Eastpark Workshop bridge, Sunken Ring, outer wall, inner wall, vault gate.
        { {4398.08f, 2356.50f, 376.355f, 0.f} },
        { {4550.00f, 2250.00f, 358.755f, 0.f} },
        { {4700.00f, 2149.87f, 383.288f, 0.f} },
        { {4848.10f, 2296.92f, 356.710f, 0.f} },
        { {4949.344f, 2432.586f, 320.256f, 0.f}, 0, AREA_THE_SUNKEN_RING },
        { {5030.00f, 2550.00f, 349.676f, 0.f} },
        { {5110.00f, 2680.00f, 373.993f, 0.f} },
        { {5180.00f, 2747.00f, 409.300f, 0.f}, 190372 },
        { {5235.00f, 2747.00f, 409.355f, 0.f} },
        { {5250.00f, 2841.00f, 409.353f, 0.f}, 191805 },
        { {5320.00f, 2841.00f, 409.300f, 0.f} },
        { {5370.00f, 2841.00f, 410.035f, 0.f}, 191810 },
        { {5420.00f, 2840.50f, 419.702f, 0.f} },
        { {5440.38f, 2840.49f, 420.589f, 0.f} }
    },
    { // Counterattacks start outside the keep, avoiding its closed gate.
        { {5100.f, 2841.f, 390.f, 0.f} }, { {4920.f, 2840.f, 375.f, 0.f} },
        { {4700.f, 2830.f, 385.f, 0.f} }, { {4430.f, 2822.f, 405.f, 0.f}, 190357 }
    },
    {
        { {5100.f, 3100.f, 385.f, 0.f} }, { {4948.524f, 3342.338f, 376.875f, 0.f} },
        { {4740.f, 3500.f, 390.f, 0.f} }, { {4585.f, 3623.f, 396.f, 0.f}, 190356 }
    },
    {
        { {5110.f, 2680.f, 385.f, 0.f} }, { {4949.344f, 2432.586f, 320.177f, 0.f} },
        { {4700.f, 2150.f, 400.f, 0.f} }, { {4490.f, 1944.f, 435.f, 0.f}, 190358 }
    },
    { { {5392.00f, 2841.00f, 415.00f, 3.14f} } }, // KeepCenter
    { { {5392.91f, 2975.26f, 415.22f, 3.14f} } }, // KeepWest
    { { {5391.61f, 2707.72f, 415.05f, 3.14f} } }, // KeepEast
    { { {4948.524f, 3342.338f, 376.875f, 0.f} } }, // WorkshopWest (relocated to actual flag below)
    { { {4949.344f, 2432.586f, 320.177f, 0.f} } }, // WorkshopEast
    { { {4452.860f, 2808.870f, 402.604f, 0.f} } }, // GuardSouth, native standing-guard position
    { { {4585.f, 3623.f, 396.f, 0.f} } },          // GuardSouthWest, tower approach
    { { {4494.580f, 1943.760f, 435.627f, 0.f} } }, // GuardSouthEast, native standing-guard position
    { { {4390.78f, 3304.09f, 372.43f, 0.f} } }, // WorkshopSouthWest
    { { {4398.08f, 2356.50f, 376.19f, 0.f} } }  // WorkshopSouthEast
};

struct BotWorkshopSite
{
    uint8 Id;
    uint32 Area;
    Lane Objective;
    Position Fallback;
};
std::array<BotWorkshopSite, 6> const BotWorkshopSites = {{
    { BATTLEFIELD_WG_WORKSHOP_NW, AREA_THE_BROKEN_TEMPLATE, WorkshopWest, {4948.524f, 3342.338f, 376.875f, 0.f} },
    { BATTLEFIELD_WG_WORKSHOP_NE, AREA_THE_SUNKEN_RING, WorkshopEast, {4949.344f, 2432.586f, 320.177f, 0.f} },
    { BATTLEFIELD_WG_WORKSHOP_SW, AREA_WESTPARK_WORKSHOP, WorkshopSouthWest, {4390.78f, 3304.09f, 372.43f, 0.f} },
    { BATTLEFIELD_WG_WORKSHOP_SE, AREA_EASTPARK_WORKSHOP, WorkshopSouthEast, {4398.08f, 2356.50f, 376.19f, 0.f} },
    { BATTLEFIELD_WG_WORKSHOP_KEEP_WEST, 0, KeepWest, {5392.91f, 2975.26f, 415.223f, 0.f} },
    { BATTLEFIELD_WG_WORKSHOP_KEEP_EAST, 0, KeepEast, {5391.61f, 2707.72f, 415.051f, 0.f} }
}};

uint32 WorkshopArea(Lane lane)
{
    for (BotWorkshopSite const& site : BotWorkshopSites)
        if (site.Objective == lane)
            return site.Area;
    return 0;
}

// Engineers use factory/keep control phases; flags and soldiers are in phase 1.
// Search only the owned factory's engineer phase without changing unit visibility.
void GetBotWarEngineers(std::list<Creature*>& engineers, WorldObject* center, uint32 entry,
    float radius, TeamId team, bool keep)
{
    Trinity::AllCreaturesOfEntryInRange check(center, entry, radius);
    Trinity::CreatureSearcherBase<Trinity::AllCreaturesOfEntryInRange, Trinity::SearcherContainerResult<Creature*>>
        searcher(FactoryPhaseMask(team == TEAM_ALLIANCE, keep), engineers, check);
    Cell::VisitGridObjects(center, searcher, radius);
}

// Exactly one kill callback is used per killer: PlayerScript for players and
// KilledUnit for autonomous soldiers, vehicles and cannons.
void AwardBotWarKill(Unit* killer, Unit* victim)
{
    BattlefieldWG* wg = GetBotWarBattlefield();
    if (!wg || !killer || !victim || killer == victim || victim->GetMapId() != 571
        || victim->GetZoneId() != wg->GetZoneId() || killer->IsFriendlyTo(victim))
        return;
    if (!victim->ToPlayer() && !IsBotWarSoldier(victim)
        && victim->GetEntry() != BATTLEFIELD_WG_NPC_GUARD_A && victim->GetEntry() != BATTLEFIELD_WG_NPC_GUARD_H)
        return;
    TeamId team;
    if (Player* player = killer->ToPlayer())
        team = player->GetTeamId();
    else if (killer->GetFaction() == WGFactionTable[TEAM_ALLIANCE])
        team = TEAM_ALLIANCE;
    else if (killer->GetFaction() == WGFactionTable[TEAM_HORDE])
        team = TEAM_HORDE;
    else
        return;
    if (IsBotWarSoldier(victim))
        wg->HandleBotWarPromotion(team, victim); // Deduplicated with the native player/pet kill callback.
    if (BotBattleStats.Active)
        ++BotBattleStats.Teams[team].Kills;
    std::list<Creature*> bots;
    GetCreatureListWithEntryInGrid(bots, victim, 0, 40.f);
    for (Creature* bot : bots)
        if (bot->IsAlive() && IsBotWarSoldier(bot) && bot->GetFaction() == WGFactionTable[team])
            bot->AI()->SetData(RankCreditData, bot->AI()->GetData(RankCreditData) + 1);
}

// Use the installed walkable surface, not approximate scripted Z values.
// Only destinations and not-yet-created summons are projected: no live teleport.
bool ProjectBotWarGround(Map* map, Position& pos, bool horizontal = false)
{
    map->LoadGrid(pos.GetPositionX(), pos.GetPositionY());
    dtNavMeshQuery const* query=MMAP::MMapFactory::createOrGetMMapManager()->GetNavMeshQuery(map->GetId(),map->GetInstanceId());
    if (!query) return false;
    float point[3]={pos.GetPositionY(),pos.GetPositionZ(),pos.GetPositionX()};
    float extents[3]={3.f,50.f,3.f}, nearest[3];
    dtPolyRef poly=0;
    dtQueryFilter filter;
    filter.setIncludeFlags(NAV_GROUND);
    if (dtStatusFailed(query->findNearestPoly(point,extents,&filter,&poly,nearest)) || !poly)
        return false;
    pos.m_positionZ=nearest[1];
    if (horizontal)
    {
        pos.m_positionX=nearest[2];
        pos.m_positionY=nearest[0];
    }
    return true;
}

Position BotFormationOffset(Position pos, uint32 slot)
{
    static float const offsets[5][2] = { {0.f, 0.f}, {-4.f, -4.f}, {-7.f, 0.f}, {-4.f, 4.f}, {2.f, 3.f} };
    uint32 squad = slot / SquadSize;
    pos.m_positionX += offsets[slot % SquadSize][0] - float(squad % 3) * 2.f;
    pos.m_positionY += offsets[slot % SquadSize][1] + (int32(squad % 5) - 2) * 2.f;
    return pos;
}

// Stable formation slots, so bots keep their place rather than jittering randomly.
Position FormationPosition(Creature* bot, Position pos, uint32 slot)
{
    pos=BotFormationOffset(pos,slot);
    bot->UpdateAllowedPositionZ(pos.m_positionX,pos.m_positionY,pos.m_positionZ);
    return pos;
}

class BotWarVehicleNavigation
{
    static float BodyRadius(Creature* vehicle)
    {
        return std::max(vehicle->IsVehicle() ? 0.75f : 0.25f, vehicle->GetBoundingRadius());
    }
    static auto CollisionCheck(Creature* vehicle)
    {
        return [vehicle](float x1, float y1, float z1, float x2, float y2, float z2) {
            return vehicle->GetMap()->isInLineOfSight(x1, y1, z1, x2, y2, z2,
                vehicle->GetPhaseMask(), vehicle->IsVehicle() ? LINEOFSIGHT_ALL_CHECKS : LINEOFSIGHT_CHECK_GOBJECT,
                VMAP::ModelIgnoreFlags::Nothing);
        };
    }

public:
    uint32 Stalls() const { return _stallEpisodes; }
    uint32 Recoveries() const { return _recoveries; }
    void Move(Creature* vehicle, Position const& requested)
    {
        if (!vehicle->IsInWorld())
            return;
        // Project only when issuing/retrying an order, not on every AI tick.
        Position destination=requested;
        ProjectBotWarGround(vehicle->GetMap(),destination);
        if (vehicle->GetExactDist2d(&destination) <= (vehicle->IsVehicle() ? 4.f : 1.5f))
        {
            Cancel(vehicle);
            return;
        }
        if (!_hasGoal || _goal.GetExactDist2d(&destination) > 2.f || std::fabs(_goal.GetPositionZ() - destination.GetPositionZ()) > 3.f)
            ResetGoal(vehicle, destination, vehicle->IsVehicle() ? 4.f : 1.5f);
        if (!vehicle->isMoving() && !_retryTimer)
            Repath(vehicle);
    }

    bool TryMoveDirect(Creature* vehicle, Position const& destination)
    {
        if (vehicle->HasUnitState(UNIT_STATE_NOT_MOVE) || vehicle->IsMovementPreventedByCasting())
            return false;
        Movement::PointsArray path;
        if (!Calculate(vehicle, destination, path, true))
            return false;
        ResetGoal(vehicle, destination, 1.5f);
        Launch(vehicle, path);
        return true;
    }

    void Cancel(Creature* vehicle)
    {
        _hasGoal = false;
        _wasStalled = false;
        _path.clear();
        if (vehicle->isMoving())
            vehicle->StopMoving();
    }

    void Update(Creature* vehicle, uint32 diff)
    {
        if (!_hasGoal || !vehicle->IsInWorld())
            return;
        _retryTimer = _retryTimer > diff ? _retryTimer - diff : 0;
        if (vehicle->HasUnitState(UNIT_STATE_NOT_MOVE) || vehicle->IsMovementPreventedByCasting())
        {
            if (vehicle->isMoving())
                vehicle->StopMoving();
            _path.clear();
            _stalled = _checkTimer = 0;
            _wasStalled = false;
            _lastPosition = vehicle->GetPosition();
            return;
        }
        _checkTimer += diff;
        if (_checkTimer < 1000)
            return;
        uint32 elapsed = _checkTimer;
        _checkTimer = 0;
        if (vehicle->GetExactDist2d(&_goal) <= _arrivalRange)
        {
            Cancel(vehicle);
            return;
        }
        _stalled = vehicle->GetExactDist2d(&_lastPosition) >= 1.f ? 0 : _stalled + elapsed;
        _lastPosition = vehicle->GetPosition();
        if (_stalled >= 4000 && !_wasStalled)
        {
            ++_stallEpisodes;
            _wasStalled = true;
        }
        else if (!_stalled && _wasStalled)
        {
            ++_recoveries;
            _wasStalled = false;
        }

        // Recheck the next segment: repairable buildings may change collision
        // after a spline was issued. Do not drive through the new obstacle.
        bool blocked = false;
        if (vehicle->isMoving() && _path.size() >= 2)
        {
            G3D::Vector3 current(vehicle->GetPositionX(), vehicle->GetPositionY(), vehicle->GetPositionZ());
            while (_nextPoint + 1 < _path.size())
            {
                auto const& previous = _path[_nextPoint - 1];
                auto const& next = _path[_nextPoint];
                float dx = next.x - previous.x, dy = next.y - previous.y;
                bool passed = (current.x - previous.x) * dx + (current.y - previous.y) * dy >= dx * dx + dy * dy;
                if (!passed && std::hypot(current.x - next.x, current.y - next.y) > 0.75f)
                    break;
                ++_nextPoint;
            }
            float lookAhead = std::max(12.f, vehicle->GetSpeed(MOVE_RUN) * 2.f + vehicle->GetBoundingRadius());
            float checked = 0.f;
            G3D::Vector3 from = current;
            for (std::size_t i = _nextPoint; i < _path.size() && checked < lookAhead; ++i)
            {
                if (!Navigation::SegmentClear(from, _path[i], BodyRadius(vehicle),
                    vehicle->GetCollisionHeight(), CollisionCheck(vehicle)))
                {
                    blocked = true;
                    break;
                }
                checked += std::hypot(_path[i].x - from.x, _path[i].y - from.y);
                from = _path[i];
            }
        }
        if (blocked)
        {
            vehicle->StopMoving();
            _path.clear();
            _retryTimer = 0;
        }
        if (!_retryTimer && (!vehicle->isMoving() || _stalled >= 4000))
            Repath(vehicle);
    }

private:
    void ResetGoal(Creature* vehicle, Position const& destination, float arrivalRange)
    {
        if (vehicle->isMoving())
            vehicle->StopMoving();
        _path.clear();
        _goal = destination;
        _arrivalRange = arrivalRange;
        _hasGoal = true;
        _recentCount = _recentNext = _candidate = _stalled = 0;
        _lastPosition = vehicle->GetPosition();
        _retryTimer = 0;
    }

    bool Calculate(Creature* vehicle, Position const& destination, Movement::PointsArray& result, bool direct = false)
    {
        static_assert(PATHFIND_NORMAL == 1 && PATHFIND_INCOMPLETE == 4);
        PathGenerator path(vehicle);
        if (!path.CalculatePath(destination.GetPositionX(), destination.GetPositionY(), destination.GetPositionZ(), false))
            return false;
        bool verified=vehicle->IsVehicle() ? Navigation::VerifiedPath(uint32(path.GetPathType()),path.GetPath().size())
            : Navigation::VerifiedInfantryPath(uint32(path.GetPathType()),path.GetPath().size());
        // Navmesh already describes the terrain a soldier can walk on. Static
        // vehicle-body rays reject ordinary slopes/bridges; infantry checks
        // dynamic gameobjects here, while retaining genuine navmesh paths.
        if (!verified || !Navigation::PathClear(path.GetPath(), BodyRadius(vehicle), vehicle->GetCollisionHeight(), CollisionCheck(vehicle)))
            return false;
        auto const& end = path.GetPath().back();
        if (std::hypot(end.x - vehicle->GetPositionX(), end.y - vehicle->GetPositionY()) < 1.5f)
            return false;
        if (direct)
        {
            if (destination.GetExactDistSq(end.x, end.y, end.z) > 2.25f)
                return false;
            float length = 0.f;
            for (std::size_t i = 1; i < path.GetPath().size(); ++i)
                length += (path.GetPath()[i] - path.GetPath()[i - 1]).length();
            if (length > vehicle->GetExactDist(&destination) * 1.6f + 6.f)
                return false;
            result = path.GetPath();
            return true;
        }
        for (uint32 i = 0; i < _recentCount; ++i)
            if (std::hypot(end.x - _recent[i].GetPositionX(), end.y - _recent[i].GetPositionY()) < 8.f)
                return false;
        result = path.GetPath();
        return true;
    }

    void Launch(Creature* vehicle, Movement::PointsArray const& path)
    {
        _recent[_recentNext++ % _recent.size()] = vehicle->GetPosition();
        _recentCount = std::min<uint32>(_recentCount + 1, uint32(_recent.size()));
        _path = path;
        _nextPoint = 1;
        _stalled = 0;
        _lastPosition = vehicle->GetPosition();
        vehicle->GetMotionMaster()->Clear(MOTION_PRIORITY_NORMAL);
        Movement::MoveSplineInit spline(vehicle);
        spline.MovebyPath(path);
        spline.SetWalk(vehicle->IsWalking());
        spline.Launch();
    }

    void Repath(Creature* vehicle)
    {
        _retryTimer = 3000; // Bound expensive collision/navmesh queries on failure.
        if (vehicle->HasUnitState(UNIT_STATE_NOT_MOVE) || vehicle->IsMovementPreventedByCasting())
            return;
        bool stalled = _stalled >= 4000;
        Movement::PointsArray path;
        if (!stalled && Calculate(vehicle, _goal, path))
        {
            Launch(vehicle, path);
            return;
        }

        // Short prefixes handle the core's 74-point limit. Side/backward probes
        // also get a vehicle out of a blocked corner without teleporting it.
        float angle = std::atan2(_goal.GetPositionY() - vehicle->GetPositionY(), _goal.GetPositionX() - vehicle->GetPositionX());
        float distance = vehicle->GetExactDist2d(&_goal);
        float bestScore = -std::numeric_limits<float>::max();
        Movement::PointsArray best;
        for (uint32 i = 0; i < 4; ++i)
        {
            Navigation::Detour detour = Navigation::RecoveryDetour(_candidate++);
            if (stalled && std::fabs(detour.Angle) < 0.01f)
                continue; // An unchanged forward route already failed to make progress.
            float length = std::min(distance, detour.Distance);
            Position candidate = vehicle->GetPosition();
            candidate.m_positionX += std::cos(angle + detour.Angle) * length;
            candidate.m_positionY += std::sin(angle + detour.Angle) * length;
            vehicle->UpdateAllowedPositionZ(candidate.m_positionX, candidate.m_positionY, candidate.m_positionZ);
            if (!Calculate(vehicle, candidate, path))
                continue;
            auto const& end = path.back();
            float remaining = std::hypot(end.x - _goal.GetPositionX(), end.y - _goal.GetPositionY());
            float score = distance - remaining - 0.03f * length;
            if (score > bestScore)
            {
                bestScore = score;
                best = path;
            }
        }
        if (!best.empty())
            Launch(vehicle, best);
    }

    Position _goal, _lastPosition;
    float _arrivalRange = 1.5f;
    std::array<Position, 3> _recent;
    Movement::PointsArray _path;
    std::size_t _nextPoint = 1;
    uint32 _recentCount = 0, _recentNext = 0, _candidate = 0;
    uint32 _retryTimer = 0, _checkTimer = 0, _stalled = 0;
    uint32 _stallEpisodes = 0, _recoveries = 0;
    bool _hasGoal = false, _wasStalled = false;
};

struct BotRouteMovement
{
    Lane Route = Center;
    uint32 Point = 0;
    uint32 Slot = 0;
    uint32 RetryTimer = 0;
    uint32 WanderTimer = 0;
    bool Assigned = false;
    bool Arrived = false;
    BotWarVehicleNavigation VehicleNavigation;

    void Assign(Creature* bot, Lane lane, uint32 slot, uint32 point = 0)
    {
        Route = lane;
        Slot = slot;
        Point = point;
        Assigned = true;
        Arrived = false;
        if (!bot->IsVehicle() || (bot->GetVehicleKit() && bot->GetVehicleKit()->GetPassenger(0)))
            Move(bot);
    }

    void RedirectAssault(Creature* bot, BattlefieldWG* wg, Lane lane)
    {
        if (!IsAssault(Route) || !IsAssault(lane) || Route == lane || bot->GetPositionX() < 4950.f)
            return;
        Route = lane;
        Point = 0;
        while (Point + 1 < BotRoutes[Route].size())
        {
            if (Blocked(wg) || MustCaptureWorkshop(wg, bot)) break;
            if (Destination(bot).GetPositionX() > bot->GetPositionX()+10.f) break;
            ++Point;
        }
        Arrived = false;
        Move(bot);
    }

    Position Destination(Creature* bot) const
    {
        Position pos = BotRoutes[Route][Point].Pos;
        if (BattlefieldWG* wg = GetBotWarBattlefield())
        {
            // LAST STAND: If Vault Gate 191810 is destroyed, all keep defenders fall back to Titan Relic!
            if (GameObject* vaultGate = wg->GetBotWarBuilding(191810))
            {
                if (vaultGate->GetDestructibleState() == GO_DESTRUCTIBLE_DESTROYED)
                {
                    if (Route == KeepCenter || Route == KeepWest || Route == KeepEast)
                        return FormationPosition(bot, { 5440.38f, 2840.49f, 430.28f, 0.f }, Slot);
                }
            }

            // INNER WALL: If inner wall 191805 is destroyed, defenders fall back to vault gate
            if (GameObject* innerWall = wg->GetBotWarBuilding(191805))
            {
                if (innerWall->GetDestructibleState() == GO_DESTRUCTIBLE_DESTROYED)
                {
                    if (Route == KeepCenter)
                        pos = { 5350.f, 2841.f, 415.f, 0.f };
                    else if (Route == KeepWest || Route == KeepEast)
                        pos = { 5330.f, 2841.f, 412.f, 0.f };
                }
            }

            // FORWARD WORKSHOP DEFENSE: KeepWest & KeepEast defend Broken Temple & Sunken Ring while controlled
            TeamId defTeam = wg->GetDefenderTeam();
            if (Route == KeepWest)
            {
                if (wg->GetData(AREA_THE_BROKEN_TEMPLATE) == uint32(defTeam))
                    if (GameObject* outerGate = wg->GetBotWarBuilding(190371))
                        if (outerGate->GetDestructibleState() != GO_DESTRUCTIBLE_DESTROYED)
                            if (GameObject* flag = wg->GetBotWarWorkshopFlag(true))
                                pos = flag->GetPosition();
            }
            else if (Route == KeepEast)
            {
                if (wg->GetData(AREA_THE_SUNKEN_RING) == uint32(defTeam))
                    if (GameObject* outerGate = wg->GetBotWarBuilding(190372))
                        if (outerGate->GetDestructibleState() != GO_DESTRUCTIBLE_DESTROYED)
                            if (GameObject* flag = wg->GetBotWarWorkshopFlag(false))
                                pos = flag->GetPosition();
            }

            // COUNTERATTACK: While inner keep wall 191805 is intact, defenders hold courtyard & controlled workshops
            if (IsCounterAttack(Route))
            {
                if (GameObject* innerWall = wg->GetBotWarBuilding(191805))
                {
                    if (innerWall->GetDestructibleState() != GO_DESTRUCTIBLE_DESTROYED)
                    {
                        if (Route == South)
                            pos = { 5392.00f, 2841.00f, 415.00f, 3.14f };
                        else if (Route == SouthWest)
                        {
                            if (wg->GetData(AREA_THE_BROKEN_TEMPLATE) == uint32(defTeam))
                            {
                                if (GameObject* flag = wg->GetBotWarWorkshopFlag(true))
                                    pos = flag->GetPosition();
                            }
                            else
                                pos = { 5392.91f, 2975.26f, 415.22f, 3.14f };
                        }
                        else if (Route == SouthEast)
                        {
                            if (wg->GetData(AREA_THE_SUNKEN_RING) == uint32(defTeam))
                            {
                                if (GameObject* flag = wg->GetBotWarWorkshopFlag(false))
                                    pos = flag->GetPosition();
                            }
                            else
                                pos = { 5391.61f, 2707.72f, 415.05f, 3.14f };
                        }
                    }
                }
            }
        }
        if (uint32 area = IsWorkshop(Route) ? WorkshopArea(Route) : CaptureArea(bot))
            if (BattlefieldWG* wg = GetBotWarBattlefield())
                if (GameObject* flag = wg->GetBotWarWorkshopFlagForArea(area))
                    pos = flag->GetPosition();
        Position formation = FormationPosition(bot, pos, Slot);
        if (IsAssault(Route) && Point + 1 == BotRoutes[Route].size())
        {
            formation.m_positionX = pos.GetPositionX() + (formation.GetPositionX() - pos.GetPositionX()) * 0.2f;
            formation.m_positionY = pos.GetPositionY() + (formation.GetPositionY() - pos.GetPositionY()) * 0.2f;
            bot->UpdateAllowedPositionZ(formation.m_positionX, formation.m_positionY, formation.m_positionZ);
        }
        return formation;
    }

    uint32 BarrierEntry() const { return BotRoutes[Route][Point].Barrier; }

    uint32 CaptureArea(Creature const* bot) const { return bot->IsVehicle() ? 0 : BotRoutes[Route][Point].CaptureArea; }

    bool MustCaptureWorkshop(BattlefieldWG* wg, Creature const* bot) const
    {
        uint32 area = CaptureArea(bot);
        if (!area)
            return false;
        TeamId team = bot->GetFaction() == WGFactionTable[TEAM_ALLIANCE] ? TEAM_ALLIANCE : TEAM_HORDE;
        return wg->GetData(area) != uint32(team);
    }

    bool Blocked(BattlefieldWG* wg) const
    {
        if (!BarrierEntry())
            return false;
        GameObject* building = wg->GetBotWarBuilding(BarrierEntry());
        return !building || building->GetDestructibleState() != GO_DESTRUCTIBLE_DESTROYED;
    }

    void Move(Creature* bot)
    {
        if (!Assigned)
            return;
        bot->SetWalk(false);
        // The same verified prefix/detour navigator works with infantry width.
        VehicleNavigation.Move(bot, Destination(bot));
        RetryTimer = 5000;
        WanderTimer = 0;
    }

    float PatrolRadius(BattlefieldWG* wg, Creature* bot) const
    {
        if (Blocked(wg)) return 12.f;
        TeamId team = bot->GetFaction() == WGFactionTable[TEAM_ALLIANCE] ? TEAM_ALLIANCE : TEAM_HORDE;
        uint32 area = IsWorkshop(Route) ? WorkshopArea(Route) : CaptureArea(bot);
        if (area && wg->GetData(area) != uint32(team)) return 12.f;
        if (IsTowerDefense(Route)) return 30.f;
        if (IsWorkshop(Route) || Route == KeepWest || Route == KeepEast) return 38.f;
        return 18.f;
    }

    void UpdateWander(Creature* bot, Position const& anchor, uint32 diff)
    {
        if (bot->IsVehicle() || bot->GetVehicle() || bot->IsInCombat() || bot->HasUnitState(UNIT_STATE_NOT_MOVE))
            return;

        if (!WanderTimer)
        {
            WanderTimer = urand(2000, 7000);
            return;
        }

        if (WanderTimer > diff)
        {
            WanderTimer -= diff;
            return;
        }

        if (bot->isMoving())
        {
            WanderTimer = 1000;
            return;
        }

        WanderTimer = urand(5000, 11000);

        if (bot->IsMounted())
        {
            bot->Dismount();
            SetBotWarMovementSpeed(bot, false);
        }

        float angle = frand(0.f, 2.f * float(M_PI));
        BattlefieldWG* wg = GetBotWarBattlefield();
        float radius = wg ? PatrolRadius(wg, bot) : 12.f;
        float dist = frand(4.5f, radius);
        Position target = anchor;
        target.m_positionX += std::cos(angle) * dist;
        target.m_positionY += std::sin(angle) * dist;
        if (bot->GetExactDist2d(&target) < 4.0f)
        {
            target.m_positionX = anchor.m_positionX - std::cos(angle) * dist;
            target.m_positionY = anchor.m_positionY - std::sin(angle) * dist;
        }
        ProjectBotWarGround(bot->GetMap(), target);

        bot->SetWalk(true);
        VehicleNavigation.Move(bot, target);
    }

    void UpdateNavigation(Creature* bot, uint32 diff)
    {
        uint32 stalls = VehicleNavigation.Stalls(), recovered = VehicleNavigation.Recoveries();
        VehicleNavigation.Update(bot, diff);
        if (BotBattleStats.Active)
        {
            auto& count = BotBattleStats.Teams[bot->GetFaction() == WGFactionTable[TEAM_ALLIANCE] ? 0 : 1];
            if (bot->IsVehicle())
            {
                count.VehicleStalls += VehicleNavigation.Stalls() - stalls;
                count.VehicleRecovered += VehicleNavigation.Recoveries() - recovered;
            }
            else
            {
                count.InfantryStalls += VehicleNavigation.Stalls() - stalls;
                count.InfantryRecovered += VehicleNavigation.Recoveries() - recovered;
            }
        }
    }

    void MovementInform(uint32 type, uint32 id)
    {
        if (type == POINT_MOTION_TYPE && id == Point)
            Arrived = true;
    }

    void Update(Creature* bot, BattlefieldWG* wg, uint32 diff)
    {
        if (!Assigned)
            return;
        if (!bot->IsVehicle())
            UpdateNavigation(bot, diff);
        // Directly launched, verified spline prefixes do not emit a point-motion
        // arrival event. Only actual proximity may advance the tactical route.
        if (!Arrived)
        {
            Position destination = Destination(bot);
            Arrived = bot->GetExactDist2d(&destination) <= (bot->IsVehicle() ? 12.f : 6.f);
        }
        if (IsTowerDefense(Route))
            if (GameObject* tower = wg->GetBotWarBuilding(GuardedTowerEntry(Route)))
                if (tower->GetDestructibleState() == GO_DESTRUCTIBLE_DESTROYED)
                {
                    Assign(bot, ResolveTowerDefense(Route, true), Slot);
                    return;
                }
        if (Arrived)
        {
            if (IsCounterAttack(Route) && Point + 1 == BotRoutes[Route].size() && !Blocked(wg))
            {
                Assign(bot, Route == SouthWest ? WorkshopSouthWest : WorkshopSouthEast, Slot);
                return;
            }
            Position destination = Destination(bot);
            if (bot->GetExactDist2d(&destination) > std::max(25.f, PatrolRadius(wg, bot) + 8.f))
            {
                Arrived = false;
                Move(bot);
                return;
            }

            if (Blocked(wg) || MustCaptureWorkshop(wg, bot) || Point + 1 >= BotRoutes[Route].size())
            {
                UpdateWander(bot, destination, diff);
                return;
            }
            ++Point;
            Arrived = false;
            Move(bot);
        }
        else if (RetryTimer <= diff)
        {
            RetryTimer = 3000;
            if (!bot->isMoving())
                Move(bot);
        }
        else
            RetryTimer -= diff;
    }
};
}

struct npc_wg_bot_vehicle : public ScriptedAI
{
    npc_wg_bot_vehicle(Creature* creature) : ScriptedAI(creature) { SetCombatMovement(false); }

    void KilledUnit(Unit* victim) override { AwardBotWarKill(me, victim); }
    void OnSpellCast(SpellInfo const* spell) override
    {
        RecordBotAbility(me, me->GetVictim(), spell->Id);
    }

    void SetRoute(WintergraspBotWar::Lane lane, uint32 slot)
    {
        uint32 first = 0;
        if (BattlefieldWG* wg = GetBotWarBattlefield())
            first = FindRouteEntry(wg, lane);
        _approachTarget.Clear();
        _approachTimer = 0;
        _route.Assign(me, lane, slot, first);
    }

    bool CanReserveDriver(Creature const* soldier) const
    {
        return !_takeoverTimer && IsBotWarSeatAvailable(me->GetVehicleKit(), 0)
            && (!_reservationTimer || OwnsDriverReservation(soldier));
    }

    bool OwnsDriverReservation(Creature const* soldier) const
    {
        return _reservationTimer && _reservedDriver == soldier->GetGUID();
    }

    bool ReserveDriver(Creature const* soldier)
    {
        if (!CanReserveDriver(soldier) || HasBotWarHumanPassenger(me))
            return false;
        _reservedDriver = soldier->GetGUID();
        _reservationTimer = 12000;
        return true;
    }

    void ReleaseDriver(Creature const* soldier)
    {
        if (OwnsDriverReservation(soldier))
        {
            _reservedDriver.Clear();
            _reservationTimer = 0;
        }
    }

    void MovementInform(uint32 type, uint32 id) override { _route.MovementInform(type, id); }

    void SetAssaultLane(Lane lane)
    {
        Unit* driver=me->GetVehicleKit()?me->GetVehicleKit()->GetPassenger(0):nullptr;
        if (IsAssault(_route.Route) && IsAssault(lane) && _route.Route != lane
            && IsBotWarSoldier(driver) && !HasBotWarHumanPassenger(me))
            SetRoute(lane, _route.Slot);
    }

    GameObject* SiegeTarget()
    {
        BattlefieldWG* wg = GetBotWarBattlefield();
        if (!wg || !_route.Assigned || !_route.Blocked(wg))
            return nullptr;
        GameObject* building = wg->GetBotWarBuilding(_route.BarrierEntry());
        if (building && building->GetDestructibleState() != GO_DESTRUCTIBLE_DESTROYED)
            if (me->GetExactDist2d(building) <= 55.f)
                return building;
        return nullptr;
    }

    void OnCharmed(bool isNew) override
    {
        // NPC drivers must retain this AI. UnitAI would schedule PossessedAI,
        // whose UpdateAI never runs our navigation or siege weapons.
        if (me->IsCharmed() && !IsBotWarSoldier(me->GetCharmer()))
            UnitAI::OnCharmed(isNew);
        if (!me->IsCharmed())
        {
            me->LastCharmerGUID.Clear();
            _takeoverTimer = 0;
            _route.Arrived = false;
            if (me->GetVehicleKit() && IsBotWarSoldier(me->GetVehicleKit()->GetPassenger(0)))
                _route.Move(me);
        }
    }

    void MoveInLineOfSight(Unit* /*unit*/) override
    {
    }

    void PassengerBoarded(Unit* passenger, int8 seat, bool apply) override
    {
        if (seat == 0 && apply)
        {
            _reservedDriver.Clear();
            _reservationTimer = 0;
        }
        if (seat == 0 && apply && IsBotWarSoldier(passenger))
        {
            if (_waitingDriver && BotBattleStats.Active)
                ++BotBattleStats.Teams[me->GetFaction() == WGFactionTable[TEAM_ALLIANCE] ? 0 : 1].RecoveredVehicles;
            _everDriven = true;
            _waitingDriver = false;
            if (!_route.Assigned)
            {
                if (BattlefieldWG* wg = GetBotWarBattlefield())
                {
                    bool isDefender = (me->GetFaction() == WGFactionTable[wg->GetDefenderTeam()]);
                    WintergraspBotWar::Lane bestLane = isDefender ? WintergraspBotWar::South : WintergraspBotWar::Center;
                    if (!isDefender)
                    {
                        float bestDist = 99999.f;
                        for (WintergraspBotWar::Lane l : { WintergraspBotWar::Center, WintergraspBotWar::West, WintergraspBotWar::East })
                        {
                            uint32 point = FindRouteEntry(wg, l);
                            float d = me->GetExactDist(&BotRoutes[l][point].Pos);
                            if (d < bestDist) { bestDist = d; bestLane = l; }
                        }
                    }
                    SetRoute(bestLane, 0);
                }
            }
            else
                SetRoute(_route.Route, _route.Slot);

            TC_LOG_INFO("bg.battlefield.botwar", "WG BotWar event=vehicle_boarded entry={} lane={} point={}",
                me->GetEntry(), uint32(_route.Route), _route.Point);
        }
        if (seat == 0 && !apply && IsBotWarSoldier(passenger))
            _waitingDriver = _everDriven;
        if (seat == 0 && passenger->ToPlayer())
        {
            if (apply)
                _route.VehicleNavigation.Cancel(me);
            else if (_route.Assigned && me->GetVehicleKit()
                && IsBotWarSoldier(me->GetVehicleKit()->GetPassenger(0)))
                _route.Move(me);
        }
    }

    void UpdateAI(uint32 diff) override
    {
        _reservationTimer = _reservationTimer > diff ? _reservationTimer - diff : 0;
        BattlefieldWG* wg = GetBotWarBattlefield();
        // Player-built vehicles share this template. Only coordinator vehicles have orders.
        if (!wg || !_route.Assigned)
            return;
        if (_takeoverTimer)
        {
            _takeoverTimer = _takeoverTimer > diff ? _takeoverTimer - diff : 0;
            return;
        }
        Unit* driver = me->GetVehicleKit() ? me->GetVehicleKit()->GetPassenger(0) : nullptr;
        if (!driver || driver->ToPlayer())
        {
            if (!driver)
                _route.VehicleNavigation.Cancel(me);
            return;
        }

        if (YieldBotWarSeat(me))
        {
            _takeoverTimer = 20000;
            _route.VehicleNavigation.Cancel(me);
            return;
        }
        _route.UpdateNavigation(me, diff);
        if (_thinkTimer > diff)
        {
            _thinkTimer -= diff;
            return;
        }
        uint32 elapsed = 500;
        _thinkTimer = 500;
        _weaponTimer = _weaponTimer > elapsed ? _weaponTimer - elapsed : 0;
        _approachTimer = _approachTimer > elapsed ? _approachTimer - elapsed : 0;

        bool holdingSupportPosition = false;
        if (_route.Blocked(wg))
        {
            if (GameObject* building = wg->GetBotWarBuilding(_route.BarrierEntry()))
            {
                bool defender = me->GetFaction() == WGFactionTable[wg->GetDefenderTeam()];
                if ((!defender && IsAssault(_route.Route)) || (defender && IsCounterAttack(_route.Route)))
                {
                    bool demolisher = me->GetEntry() == NPC_WINTERGRASP_DEMOLISHER;
                    bool catapult = me->GetEntry() == NPC_WINTERGRASP_CATAPULT;
                    float minRange = catapult ? 24.f : demolisher ? 12.f : 0.f;
                    float maxRange = catapult ? 48.f : demolisher ? 42.f : 11.f;
                    float dist = me->GetExactDist2d(building);
                    bool canAttack = dist >= minRange && dist <= maxRange
                        && std::fabs(me->GetPositionZ() - building->GetPositionZ()) <= 10.f;

                    if (canAttack)
                    {
                        _route.VehicleNavigation.Cancel(me);
                        if (!catapult)
                        {
                            me->SetFacingToObject(building);
                            if (!_weaponTimer && !me->HasUnitState(UNIT_STATE_CASTING))
                            {
                                SpellCastResult result = demolisher ? me->CastSpell(building->GetPosition(), 50896, false)
                                    : DoCastSelf(54109); // Ram; cannon belongs to the manned turret.
                                _weaponTimer = CastRetryDelay(result == SPELL_CAST_OK, 2200);
                            }
                            return;
                        }
                        holdingSupportPosition = true;
                    }
                    else if (dist < 125.f && ApproachBuilding(building, catapult ? 36.f : demolisher ? 30.f : 8.f, minRange, maxRange))
                        return;
                }
            }
        }

        if (!holdingSupportPosition)
            _route.Update(me, wg, elapsed);
        if (!_weaponTimer && !me->HasUnitState(UNIT_STATE_CASTING))
        {
            // Assault vehicles push to the fortress walls without stopping for infantry
            bool isAssaultVehicle = (me->GetFaction() == WGFactionTable[wg->GetAttackerTeam()] && IsAssault(_route.Route));
            Unit* target = nullptr;
            for (uint32 vEntry : { uint32(NPC_WINTERGRASP_SIEGE_ENGINE_ALLIANCE), uint32(NPC_WINTERGRASP_SIEGE_ENGINE_HORDE),
                                   uint32(NPC_WINTERGRASP_DEMOLISHER), uint32(NPC_WINTERGRASP_CATAPULT) })
            {
                std::list<Creature*> vehList;
                GetCreatureListWithEntryInGrid(vehList, me, vEntry, 45.f);
                for (Creature* v : vehList)
                    if (v->IsAlive() && me->IsValidAttackTarget(v) && me->IsWithinLOSInMap(v))
                    {
                        target = v;
                        break;
                    }
                if (target)
                    break;
            }
            if (!target && (!isAssaultVehicle || !me->isMoving()))
                target = me->SelectNearestTarget(40.f);

            if (target && me->IsWithinLOSInMap(target))
            {
                bool isRangedVehicle = (me->GetEntry() == NPC_WINTERGRASP_CATAPULT || me->GetEntry() == NPC_WINTERGRASP_DEMOLISHER);
                if (isRangedVehicle || !isAssaultVehicle || !me->isMoving() || me->IsWithinDistInMap(target, 12.f))
                {
                    if (!isAssaultVehicle || !me->isMoving())
                        me->SetFacingToObject(target);
                    SpellCastResult result = SPELL_FAILED_OUT_OF_RANGE;
                    if (me->GetEntry() == NPC_WINTERGRASP_CATAPULT)
                        result = me->CastSpell(target->GetPosition(), 57606, false);
                    else if (me->GetEntry() == NPC_WINTERGRASP_DEMOLISHER)
                        result = me->CastSpell(target->GetPosition(), 50896, false);
                    else if (me->IsWithinDistInMap(target, 12.f))
                        result = DoCastSelf(54109);
                    _weaponTimer = CastRetryDelay(result == SPELL_CAST_OK, 2200);
                }
            }
        }
    }

private:
    uint32 FindRouteEntry(BattlefieldWG* wg, Lane lane) const
    {
        uint32 first = 0;
        float bestDistance = std::numeric_limits<float>::max();
        auto const& route = BotRoutes[lane];
        for (uint32 point = 0; point < route.size(); ++point)
        {
            BotWaypoint const& waypoint = route[point];
            float distance = me->GetExactDistSq(&waypoint.Pos);
            if (distance < bestDistance)
            {
                first = point;
                bestDistance = distance;
            }
            if (waypoint.Barrier)
            {
                GameObject* building = wg->GetBotWarBuilding(waypoint.Barrier);
                if (!building || building->GetDestructibleState() != GO_DESTRUCTIBLE_DESTROYED)
                    break;
            }
        }
        return first;
    }

    bool ApproachBuilding(GameObject const* building, float distance, float minRange, float maxRange)
    {
        if (_approachTarget == building->GetGUID() && (me->isMoving() || _approachTimer))
            return true;
        if (_approachTimer)
            return false;
        _approachTarget.Clear();
        _approachTimer = 2500;

        // Approach from the lane's side of the obstacle. Check nearby offsets
        // before falling back to the route's bridge/ramp waypoints.
        auto const& route = BotRoutes[_route.Route];
        uint32 point = _route.Point;
        if (point && building->GetExactDist2d(&route[point].Pos) < 5.f)
            --point;
        Position const& waypoint = route[point].Pos;
        float angle = building->GetAbsoluteAngle(waypoint.GetPositionX(), waypoint.GetPositionY());
        for (float offset : { 0.f, 0.35f, -0.35f, 0.7f, -0.7f })
        {
            Position approach = building->GetPosition();
            approach.m_positionX += std::cos(angle + offset) * distance;
            approach.m_positionY += std::sin(angle + offset) * distance;
            if (!ProjectBotWarGround(me->GetMap(), approach, true))
                continue;
            float firingDistance = approach.GetExactDist2d(building);
            if (firingDistance < minRange + 1.5f || firingDistance > maxRange - 1.5f
                || std::fabs(approach.GetPositionZ() - building->GetPositionZ()) > 8.f)
                continue;
            if (_route.VehicleNavigation.TryMoveDirect(me, approach))
            {
                _approachTarget = building->GetGUID();
                return true;
            }
        }
        return false;
    }

    BotRouteMovement _route;
    ObjectGuid _reservedDriver, _approachTarget;
    uint32 _reservationTimer = 0;
    uint32 _approachTimer = 0;
    uint32 _thinkTimer = 500;
    uint32 _weaponTimer = 1000;
    uint32 _takeoverTimer = 0;
    bool _everDriven = false, _waitingDriver = false;
};

struct npc_wg_bot_soldier : public ScriptedAI
{
    npc_wg_bot_soldier(Creature* creature) : ScriptedAI(creature)
    {
        // unit_class in creature_template is a stat class (1/2/4/8), not a player specialization.
        _profile = me->IsNPCBot() ? uint32(Warrior) : (me->GetEntry() - 80501) % 4;
        _primaryTimer = urand(500, 2000);
        SetBotWarMovementSpeed(me, false);
    }

    bool _nativeCombat = false;

    void ConfigureProfile(uint32 slot)
    {
        _profile = ProfileForSlot(slot);
        if (me->IsNPCBot()) return; // Preserve native resources, equipment and class passives.
        me->SetUnitFlag2(UNIT_FLAG2_REGENERATE_POWER);
        if (_profile == Warrior)
        {
            me->SetPowerType(POWER_RAGE);
            me->SetMaxPower(POWER_RAGE, 1000);
            me->SetPower(POWER_RAGE, 0);
        }
        else if (_profile == Rogue)
        {
            me->SetPowerType(POWER_ENERGY);
            me->SetMaxPower(POWER_ENERGY, 100);
            me->SetPower(POWER_ENERGY, 100);
            me->SetVirtualItem(0, 40386); // dagger in the installed Item.dbc
            me->SetVirtualItem(1, 40386);
            me->SetCanDualWield(true);
        }
        else
            me->SetPowerType(POWER_MANA);
    }

    void ApplyPrecombatBuffs()
    {
        if (me->IsInCombat() || me->GetVehicle() || me->IsNPCBot())
            return;
        switch (_profile)
        {
            case Warrior:
                if (!me->HasAura(47436)) // Battle Shout
                    me->CastSpell(me, 47436, true);
                break;
            case FrostMage:
                if (!me->HasAura(43008)) // Ice Armor
                    me->CastSpell(me, 43008, true);
                if (!me->HasAura(42995)) // Arcane Intellect
                    me->CastSpell(me, 42995, true);
                break;
            case HolyPriest:
                if (!me->HasAura(48161)) // Power Word: Fortitude
                    me->CastSpell(me, 48161, true);
                if (!me->HasAura(48168)) // Inner Fire
                    me->CastSpell(me, 48168, true);
                break;
            case HolyPaladin:
                if (!me->HasAura(48932)) // Blessing of Might
                    me->CastSpell(me, 48932, true);
                if (!me->HasAura(25780)) // Righteous Fury
                    me->CastSpell(me, 25780, true);
                break;
            case RestorationShaman:
                if (!me->HasAura(49281)) // Lightning Shield
                    me->CastSpell(me, 49281, true);
                break;
            case Warlock:
                if (!me->HasAura(47889)) // Fel Armor
                    me->CastSpell(me, 47889, true);
                break;
            default:
                break;
        }
    }

    void SetOrders(WintergraspBotWar::Lane lane, uint32 slot, ObjectGuid vehicle, uint32 point)
    {
        _vehicle = vehicle;
        _route.Assign(me, lane, slot, point);
    }

    void SetEscortVehicle(ObjectGuid vehicle)
    {
        if (_vehicle == vehicle)
            return;
        _vehicle = vehicle;
        _supplyMoveTimer = 0;
        _boardProgressTimer = 0;
    }

    Lane GetLane() const { return _route.Route; }
    ObjectGuid EscortVehicle() const { return _vehicle; }
    bool HasBuildSite() const { return !_buildSite.IsEmpty(); }
    void SetTacticalLane(Lane lane) { _requestedLane = lane; }
    bool HasWorkshopOrder() const { return IsWorkshop(_requestedLane) || IsWorkshop(_route.Route); }
    void SetBuildSite(ObjectGuid site, Position const& position = {})
    {
        if (_buildSite == site)
            return;
        _buildSite = site;
        _buildPosition = position;
        if (!me->IsInCombat() && !me->GetVehicle())
        {
            _supplyNavigation.Cancel(me);
            _route.VehicleNavigation.Cancel(me);
        }
        _supplyMoveTimer = 0;
        if (site.IsEmpty() && !me->IsInCombat() && !me->GetVehicle())
        {
            _route.Arrived = false;
            _route.Move(me);
        }
    }
    uint32 GetData(uint32 id) const override
    {
        return id == RankCreditData ? _killCredits : id == ProfileData ? _profile : id == RoutePointData ? _route.Point : 0;
    }
    void SetData(uint32 id, uint32 value) override
    {
        if (id != RankCreditData)
            return;
        _killCredits = std::min(value, 10u);
        uint32 spell = RankForCredits(_killCredits) == 2 ? SPELL_LIEUTENANT
            : RankForCredits(_killCredits) == 1 ? SPELL_CORPORAL : SPELL_RECRUIT;
        for (uint32 other : { uint32(SPELL_RECRUIT), uint32(SPELL_CORPORAL), uint32(SPELL_LIEUTENANT) })
            if (other != spell)
                me->RemoveAurasDueToSpell(other);
        me->SetAuraStack(spell, me, RankForCredits(_killCredits) == 2 ? 1 : 1 + _killCredits % 5);
    }
    void KilledUnit(Unit* victim) override { AwardBotWarKill(me, victim); }
    void OnSpellCast(SpellInfo const* spell) override
    {
        RecordBotAbility(me, me->GetVictim(), spell->Id);
    }

    bool CanAfford(uint32 spell) const
    {
        SpellInfo const* info = sSpellMgr->GetSpellInfo(spell);
        if (!info)
            return false;
        int32 cost = info->CalcPowerCost(me, info->GetSchoolMask());
        return cost <= 0 || (info->PowerType == POWER_HEALTH ? me->GetHealth() > uint32(cost)
            : me->GetPower(info->PowerType) >= cost);
    }

    bool CastAbility(Unit* target, uint32 spell, uint32& timer, uint32 cooldown, bool triggered = false)
    {
        if (!BotAbilityAvailable(me, target, spell))
        {
            timer = 500;
            return false;
        }
        SpellCastResult result = me->CastSpell(target, spell, triggered);
        if (result == SPELL_CAST_OK)
            RecordBotAbility(me, target, spell);
        timer = CastRetryDelay(result == SPELL_CAST_OK, cooldown);
        return result == SPELL_CAST_OK;
    }

    void DoAction(int32 action) override
    {
        if (action == 1)
        {
            _boardingTimer = 20000;
            SetEscortVehicle(ObjectGuid::Empty);
        }
    }

    void MovementInform(uint32 type, uint32 id) override { _route.MovementInform(type, id); }

    bool HasLogisticsOrder() const
    {
        if (!_buildSite.IsEmpty()) return true;
        if (_vehicle.IsEmpty()) return false;
        if (Creature* vehicle = me->GetMap()->GetCreature(_vehicle))
            if (auto* ai = CAST_AI(npc_wg_bot_vehicle, vehicle->AI()))
                return vehicle->IsAlive() && ai->OwnsDriverReservation(me);
        return false;
    }

    void FindVehicleToDrive()
    {
        Creature* nearest = nullptr;
        float distance = 50.f;
        for (uint32 entry : { uint32(NPC_WINTERGRASP_DEMOLISHER), uint32(NPC_WINTERGRASP_SIEGE_ENGINE_ALLIANCE),
            uint32(NPC_WINTERGRASP_SIEGE_ENGINE_HORDE), uint32(NPC_WINTERGRASP_CATAPULT) })
        {
            std::list<Creature*> vehicles;
            me->GetCreatureListWithEntryInGrid(vehicles, entry, distance);
            for (Creature* vehicle : vehicles)
            {
                float candidate = me->GetExactDist(vehicle);
                if (candidate >= distance || !vehicle->IsAlive() || !vehicle->IsFriendlyTo(me) || HasBotWarHumanPassenger(vehicle))
                    continue;
                if (auto* ai = CAST_AI(npc_wg_bot_vehicle, vehicle->AI()); ai && ai->CanReserveDriver(me))
                {
                    nearest = vehicle;
                    distance = candidate;
                }
            }
        }
        if (nearest && CAST_AI(npc_wg_bot_vehicle, nearest->AI())->ReserveDriver(me))
            SetEscortVehicle(nearest->GetGUID());
        else
            _boardingTimer = 2000;
    }

    bool BoardOrApproachVehicle(Creature* vehicle, uint32 diff)
    {
        Vehicle* kit = vehicle->GetVehicleKit();
        auto* ai = CAST_AI(npc_wg_bot_vehicle, vehicle->AI());
        if (!ai || !kit || !vehicle->IsAlive() || HasBotWarHumanPassenger(vehicle))
            return false;

        if (ai->ReserveDriver(me))
        {
            float distance = me->GetExactDist(vehicle);
            if (!_boardProgressTimer)
                _route.VehicleNavigation.Cancel(me);
            if (!_boardProgressTimer || distance <= _boardProgressDistance - 3.f)
            {
                _boardProgressTimer = 15000;
                _boardProgressDistance = distance;
            }
            else if (_boardProgressTimer <= diff)
            {
                ai->ReleaseDriver(me);
                SetEscortVehicle(ObjectGuid::Empty);
                _boardingTimer = 5000;
                _supplyNavigation.Cancel(me);
                return true;
            }
            else
                _boardProgressTimer -= diff;

            if (me->IsWithinDistInMap(vehicle, 8.f) && me->IsWithinLOSInMap(vehicle))
            {
                if (!_boardingTimer)
                    BoardVehicleSeat(vehicle);
            }
            else
            {
                if (!_supplyMoveTimer)
                {
                    _supplyMoveTimer = 2000;
                    _supplyNavigation.Move(me, vehicle->GetPosition());
                }
                _supplyNavigation.Update(me, diff);
            }
            return true;
        }

        _boardProgressTimer = 0;

        if (!_boardingTimer && IsBotWarSoldier(kit->GetPassenger(0)))
            for (auto const& [seat, info] : kit->Seats)
                if (Unit* accessory = kit->GetPassenger(seat))
                    if ((accessory->GetEntry() == 28319 || accessory->GetEntry() == 32629)
                        && IsBotWarSeatAvailable(accessory->GetVehicleKit(), 0)
                        && me->IsWithinDistInMap(accessory, 8.f) && me->IsWithinLOSInMap(accessory))
                    {
                        BoardVehicleSeat(accessory);
                        return true;
                    }
        return false;
    }

    void BoardVehicleSeat(Unit* vehicle)
    {
        _boardingTimer = 2000;
        _route.VehicleNavigation.Cancel(me);
        _supplyNavigation.Cancel(me);
        me->Dismount();
        SetBotWarMovementSpeed(me, false);
        me->EnterVehicle(vehicle, 0);
    }

    bool CanAIAttack(Unit const* target) const override
    {
        if (!target || !target->IsAlive()) return false;
        if (HasLogisticsOrder() && !target->IsInCombatWith(me) && target->GetVictim() != me)
            return false; // Walk to the factory/driver seat; retaliate if actually attacked.
        if (BattlefieldWG* wg = GetBotWarBattlefield())
            if (_route.Blocked(wg) && !target->IsInCombatWith(me) && target->GetVictim() != me)
                if (GameObject* wall = wg->GetBotWarBuilding(_route.BarrierEntry()))
                    if ((me->GetPositionX()-wall->GetPositionX())*(target->GetPositionX()-wall->GetPositionX())
                        + (me->GetPositionY()-wall->GetPositionY())*(target->GetPositionY()-wall->GetPositionY()) < 0.f)
                        return false;
        return true;
    }

    bool CanEngage(Unit const* target) const
    {
        return target && me->IsValidAttackTarget(target) && me->IsWithinLOSInMap(target)
            && (me->IsNPCBot() ? me->GetBotAI()->CanBotAttack(target) : CanAIAttack(target));
    }

    float EnemyScore(Unit* enemy, BattlefieldWG* wg)
    {
        bool protects = enemy->GetVictim() && enemy->GetVictim()->IsFriendlyTo(me) && IsBotWarHealer(enemy->GetVictim());
        TeamId team = me->GetFaction() == WGFactionTable[TEAM_ALLIANCE] ? TEAM_ALLIANCE : TEAM_HORDE;
        auto const& focus = BotFocus[team][_route.Route];
        float score = TargetPriority(me->GetExactDist2d(enemy), enemy->GetHealthPct(), IsBotWarHealer(enemy),
            enemy->IsNonMeleeSpellCast(false), protects, enemy->IsVehicle() && team==wg->GetDefenderTeam(),
            focus.Target==enemy->GetGUID() && !CanReserveAction(BotBattleStats.Elapsed, focus.Until),
            enemy->HasUnitState(UNIT_STATE_FLEEING | UNIT_STATE_CONFUSED));

        // DEFEND ESCORT VEHICLE: prioritize enemies targeting or attacking our siege engine
        if (team == wg->GetAttackerTeam() && !_vehicle.IsEmpty())
        {
            if (Creature* veh = me->GetMap()->GetCreature(_vehicle))
            {
                if (veh->IsAlive())
                {
                    if (enemy->GetVictim() == veh)
                        score += 60.f; // Massive priority to kill whatever is hitting our vehicle
                    else if (enemy->GetExactDist2d(veh) <= 25.f)
                        score += 30.f; // Intercept enemies approaching our vehicle
                }
            }
        }
        return score;
    }

    Unit* SelectEnemy(BattlefieldWG* wg)
    {
        UpdateCombatReservations();
        float range = IsAssault(_route.Route) && me->GetFaction() == WGFactionTable[wg->GetAttackerTeam()] ? 75.f : 60.f;
        std::list<Unit*> units;
        Trinity::AnyUnfriendlyUnitInObjectRangeCheck check(me, me, range);
        Trinity::UnitListSearcher<Trinity::AnyUnfriendlyUnitInObjectRangeCheck> searcher(me, units, check);
        Cell::VisitAllObjects(me, searcher, range);
        Unit* best = nullptr;
        float score = -std::numeric_limits<float>::max();
        for (Unit* unit : units)
        {
            if (unit->HasUnitState(UNIT_STATE_FLEEING | UNIT_STATE_CONFUSED) || !CanEngage(unit))
                continue;
            if (!unit->ToPlayer() && !unit->IsNPCBot() && !IsBotWarSoldier(unit) && !unit->IsVehicle()
                && !unit->GetCharmerOrOwnerPlayerOrPlayerItself()
                && unit->GetEntry()!=BATTLEFIELD_WG_NPC_GUARD_A && unit->GetEntry()!=BATTLEFIELD_WG_NPC_GUARD_H)
                continue;
            float priority = EnemyScore(unit, wg);
            if (priority > score) { score=priority; best=unit; }
        }
        Unit* current = me->GetVictim();
        if (best && CanEngage(current)
            && !current->HasUnitState(UNIT_STATE_FLEEING | UNIT_STATE_CONFUSED)
            && !ShouldSwitchTarget(EnemyScore(current,wg), score))
            return current;
        return best;
    }

    bool TryInterrupt(Unit* target)
    {
        if (_interruptTimer || !target || !target->IsNonMeleeSpellCast(false) || !me->IsWithinLOSInMap(target))
            return false;
        uint32 spell=0, cooldown=0;
        float range=0;
        switch (_profile)
        {
            case Warrior: spell=6552; cooldown=10000; range=5.f; break;
            case FrostMage: spell=2139; cooldown=24000; range=30.f; break;
            case Marksman: spell=34490; cooldown=20000; range=35.f; break;
            case RestorationShaman: spell=57994; cooldown=6000; range=25.f; break;
            case Rogue: spell=1766; cooldown=10000; range=5.f; break;
            default: return false;
        }
        return me->IsWithinDistInMap(target,range) && CastAbility(target,spell,_interruptTimer,cooldown);
    }

    void AttackStart(Unit* target) override
    {
        if (me->GetVehicle() || !_route.Assigned || !CanEngage(target))
            return;
        if (!me->IsInCombat() || !_combatOriginSet)
        {
            _combatOrigin = me->GetPosition();
            _combatOriginSet=true;
            me->SetHomePosition(_combatOrigin);
        }
        _route.VehicleNavigation.Cancel(me);
        _supplyNavigation.Cancel(me);
        _route.Arrived = false;
        _route.WanderTimer = 0;
        me->Dismount();
        SetBotWarMovementSpeed(me, false);
        if (me->IsNPCBot())
            me->GetBotAI()->AttackStart(target);
        else if (IsMelee(_profile))
            ScriptedAI::AttackStart(target);
        else if (me->Attack(target, false))
            me->GetMotionMaster()->MoveChase(target, IsHealer(_profile) ? 28.f : 22.f);
    }

    void UpdateCombatOrigin()
    {
        if (!me->IsInCombat())
        {
            _combatOriginSet=false;
            me->SetHomePosition(me->GetPosition());
        }
        else if (!_combatOriginSet)
        {
            _combatOrigin=me->GetPosition();
            _combatOriginSet=true;
            me->SetHomePosition(_combatOrigin);
        }
    }

    void EnterEvadeMode(EvadeReason /*why*/) override
    {
        if (me->IsNPCBot())
        {
            me->CombatStop(true);
            me->GetThreatManager().ClearAllThreat();
        }
        else if (!_EnterEvadeMode())
            return;
        me->GetMotionMaster()->Clear();
        _route.Arrived = false;
        _route.WanderTimer = 0;
        _route.Move(me);
    }

    void JustDied(Unit* /*killer*/) override
    {
        if (BotBattleStats.Active)
            ++BotBattleStats.Teams[me->GetFaction() == WGFactionTable[TEAM_ALLIANCE] ? 0 : 1].BotDeaths;

    }

    void UpdateAI(uint32 diff) override
    {
        BattlefieldWG* wg = GetBotWarBattlefield();
        if (!wg || !_route.Assigned)
            return;
        if (me->GetVehicle())
            return; // Driver/gunner is one roster participant, not an extra summon.
        // Capture an origin for both initiators and bots engaged by an enemy.
        UpdateCombatOrigin();
        if (me->IsNPCBot() && me->GetVictim() && !_nativeCombat)
        {
            _route.VehicleNavigation.Cancel(me);
            _supplyNavigation.Cancel(me);
            _route.Arrived = false;
            me->Dismount();
            SetBotWarMovementSpeed(me, false);
            me->GetBotAI()->AttackStart(me->GetVictim());
        }
        _nativeCombat = me->GetVictim() != nullptr;

        auto tick = [diff](uint32& timer) { timer = timer > diff ? timer - diff : 0; };
        tick(_primaryTimer);
        tick(_secondaryTimer);
        tick(_defensiveTimer);
        tick(_interruptTimer);
        tick(_trinketTimer);
        tick(_thinkTimer);
        tick(_healScanTimer);
        tick(_boardingTimer);
        tick(_supplyMoveTimer);
        tick(_buffTimer);
        if (!me->IsNPCBot() && !me->IsInCombat() && !me->GetVehicle() && !_buffTimer)
        {
            _buffTimer = urand(20000, 35000);
            ApplyPrecombatBuffs();
        }
        if (_requestedLane != LaneCount && !me->IsInCombat() && !HasLogisticsOrder())
        {
            if (_route.Route != _requestedLane)
            {
                if (IsAssault(_route.Route) && IsAssault(_requestedLane))
                    _route.RedirectAssault(me, wg, _requestedLane);
                else
                    _route.Assign(me, _requestedLane, _route.Slot);
            }
            _requestedLane = LaneCount;
        }
        if (!me->IsNPCBot() && !_trinketTimer && me->HasUnitState(UNIT_STATE_CONTROLLED | UNIT_STATE_ROOT))
        {
            CastAbility(me, 42292, _trinketTimer, 120000, true);
        }
        if (me->HasUnitState(UNIT_STATE_CASTING | UNIT_STATE_CONTROLLED))
            return;

        // Healers support injured allies even when they have no hostile victim.
        if (!me->IsNPCBot() && IsHealer(_profile) && !_primaryTimer && !_healScanTimer && !HasLogisticsOrder())
        {
            _healScanTimer = 500;
            if (Unit* ally = DoSelectLowestHpFriendly(35.f))
                if (ally->IsAlive() && ally->HealthBelowPct(85) && me->IsWithinLOSInMap(ally))
                {
                    me->Dismount();
                    SetBotWarMovementSpeed(me, false);
                    uint32 hot = _profile==RestorationShaman ? 61301 : 48068;
                    uint32 spell = HealingSpell(_profile, ally->HealthBelowPct(45), ally->HasAura(hot),
                        ally->HasAura(48066) || ally->HasAura(6788));
                    if (_profile==HolyPaladin && spell==48821 && me->GetSpellHistory()->HasCooldown(48821))
                        spell=48785;
                    if (CanAfford(spell))
                    {
                        me->StopMoving();
                        if (CastAbility(ally, spell, _primaryTimer, urand(1600, 2400)))
                            return;
                    }
                    else
                        _healScanTimer = 2000; // keep moving instead of stopping for unaffordable heals
                }
        }

        if (!_thinkTimer)
        {
            _thinkTimer = urand(400, 800);
            if (me->IsInCombat() && me->GetExactDist2d(&_combatOrigin) > 100.f)
            {
                EnterEvadeMode(EVADE_REASON_BOUNDARY);
                return;
            }
            if (!HasLogisticsOrder())
            {
                if (Unit* enemy = SelectEnemy(wg))
                {
                    if (enemy != me->GetVictim()) AttackStart(enemy);
                    TeamId team = me->GetFaction() == WGFactionTable[TEAM_ALLIANCE] ? TEAM_ALLIANCE : TEAM_HORDE;
                    BotFocus[team][_route.Route] = {enemy->GetGUID(), BotBattleStats.Elapsed+3000};
                    if (!me->IsNPCBot() && TryInterrupt(enemy)) return;
                }
            }
        }

        bool canCapture = false;
        if (IsAssault(_route.Route) && wg->CanInteractWithRelic()
            && me->GetFaction() == WGFactionTable[wg->GetAttackerTeam()])
            if (GameObject* relic = wg->GetRelic())
                canCapture = !me->IsInCombat() && me->IsWithinDistInMap(relic, 7.f) && me->IsWithinLOSInMap(relic);
        _captureTimer = canCapture ? _captureTimer + diff : 0;
        if (_captureTimer >= 3000)
        {
            wg->EndBattle(false);
            return;
        }

        if (!(me->IsNPCBot() ? me->GetVictim() != nullptr : UpdateVictim()))
        {
            if (!_buildSite.IsEmpty())
            {
                if (Creature* engineer = me->GetMap()->GetCreature(_buildSite); engineer && engineer->IsAlive())
                {
                    if (me->GetExactDist(&_buildPosition) <= 8.f)
                    {
                        me->Dismount();
                        SetBotWarMovementSpeed(me, false);
                        me->StopMoving();
                        return; // coordinator builds only while still nearby and eligible
                    }
                    if (!_supplyMoveTimer)
                    {
                        _supplyMoveTimer = 3000;
                        _supplyNavigation.Move(me, _buildPosition);
                    }
                    _supplyNavigation.Update(me, diff);
                    return;
                }
                SetBuildSite(ObjectGuid::Empty);
            }
            if (!_vehicle.IsEmpty())
            {
                Creature* assigned = me->GetMap()->GetCreature(_vehicle);
                if (!assigned || !assigned->IsAlive())
                    _vehicle.Clear();
            }
            if (_vehicle.IsEmpty() && !_boardingTimer && !me->GetVehicle() && !IsTowerDefense(_route.Route))
                FindVehicleToDrive();

            // Board or escort vehicle
            if (Creature* vehicle = me->GetMap()->GetCreature(_vehicle))
            {
                if (BoardOrApproachVehicle(vehicle, diff))
                    return;
                // ESCORT: Stay in close formation with our siege vehicle to protect it!
                if (vehicle->IsAlive() && !me->IsInCombat())
                {
                    float distToVeh = me->GetExactDist2d(vehicle);
                    uint32 posIndex = _route.Slot % SquadSize;
                    float lateralOffset = (posIndex % 2 == 1) ? -7.0f : 7.0f;
                    float longitudinalOffset = (posIndex <= 2) ? -3.0f : -9.0f;
                    float vYaw = vehicle->GetOrientation();
                    Position escortPos = vehicle->GetPosition();
                    escortPos.m_positionX += longitudinalOffset * std::cos(vYaw) - lateralOffset * std::sin(vYaw);
                    escortPos.m_positionY += longitudinalOffset * std::sin(vYaw) + lateralOffset * std::cos(vYaw);
                    ProjectBotWarGround(me->GetMap(), escortPos);

                    bool vehMoving = vehicle->isMoving();
                    float distToGoal = me->GetExactDist2d(&escortPos);

                    if (distToVeh > 32.f && vehMoving && !me->IsMounted())
                    {
                        me->Mount(GetWintergraspMount(me));
                        SetBotWarMovementSpeed(me, true);
                    }
                    else if ((distToVeh <= 20.f || !vehMoving) && me->IsMounted())
                    {
                        me->Dismount();
                        SetBotWarMovementSpeed(me, false);
                    }

                    if (vehMoving || distToGoal > 4.5f)
                    {
                        if (!_supplyMoveTimer)
                        {
                            _supplyMoveTimer = vehMoving ? 800 : 1800;
                            _supplyNavigation.Move(me, escortPos);
                        }
                        _supplyNavigation.Update(me, diff);
                    }
                    else
                    {
                        if (me->isMoving())
                            me->StopMoving();
                        float outwardAngle = vehicle->GetAbsoluteAngle(me);
                        me->SetFacingTo(outwardAngle);
                    }
                    return; // Retain escort formation, do not fall through to advance independently!
                }
            }
            Position destination = _route.Destination(me);
            if (me->GetExactDist2d(&destination) < 25.f && me->IsMounted())
            {
                me->Dismount();
                SetBotWarMovementSpeed(me, false);
            }
            if (me->GetExactDist2d(&destination) > 45.f && !me->IsMounted())
            {
                me->Mount(GetWintergraspMount(me));
                SetBotWarMovementSpeed(me, true);
            }
            _route.Update(me, wg, diff);
            return;
        }

        if (me->IsNPCBot()) return; // Native class AI performs attacks, interrupts, healing and cooldowns.
        me->Dismount();
        SetBotWarMovementSpeed(me, false);
        Unit* victim = me->GetVictim();
        if (!victim)
            return;
        if (!me->IsWithinLOSInMap(victim))
        {
            _lostSightTimer += diff;
            if (_lostSightTimer >= 8000)
            {
                _lostSightTimer = 0;
                EnterEvadeMode(EVADE_REASON_NO_HOSTILES);
            }
            else if (!me->isMoving())
                me->GetMotionMaster()->MoveChase(victim, IsMelee(_profile) ? 0.f : 22.f);
            return;
        }
        _lostSightTimer = 0;
        // Close to spell range before stopping to cast. Failed out-of-range
        // casts used to repeatedly stop ranged units while their enemy escaped.
        if (!IsMelee(_profile) && !me->IsWithinDistInMap(victim, 30.f))
        {
            if (!me->isMoving())
                me->GetMotionMaster()->MoveChase(victim, 22.f);
            return;
        }
        if (victim->HasUnitState(UNIT_STATE_FLEEING | UNIT_STATE_CONFUSED))
            return; // preserve breakable control instead of immediately breaking an ally's fear
        me->SetFacingToObject(victim);

        if (_route.Blocked(wg))
            if (GameObject* barrier = wg->GetBotWarBuilding(_route.BarrierEntry()))
            {
                float bx = me->GetPositionX() - barrier->GetPositionX();
                float by = me->GetPositionY() - barrier->GetPositionY();
                float tx = victim->GetPositionX() - barrier->GetPositionX();
                float ty = victim->GetPositionY() - barrier->GetPositionY();
                if (bx * tx + by * ty < 0.f)
                {
                    me->CombatStop(true);
                    return;
                }
            }

        if (!_defensiveTimer)
        {
            uint32 spell = 0, cooldown = 0;
            Unit* target = victim;
            if (_profile == 0)
            {
                if (me->HealthBelowPct(30) && !me->HasAura(871))
                    { spell = 871; cooldown = 30000; target = me; }
                else if (!me->IsWithinMeleeRange(victim) && me->IsWithinDistInMap(victim, 25.f))
                    { spell = 11578; cooldown = 12000; } // Charge, rank 3
                else if (me->GetExactDist2d(victim) <= 8.f && me->HealthBelowPct(40)
                    && !victim->HasUnitState(UNIT_STATE_FLEEING))
                    { spell = 5246; cooldown = 25000; target = me; }
            }
            else if (_profile == 1)
            {
                if (me->HealthBelowPct(20) && !me->HasAura(45438))
                    { spell = 45438; cooldown = 45000; target = me; }
                else if (me->IsWithinDistInMap(victim, 8.f))
                    { spell = 42917; cooldown = 15000; target = me; }
            }
            else if (_profile == 2)
            {
                if (me->HealthBelowPct(50) && !me->HasAura(48066) && !me->HasAura(6788))
                    { spell = 48066; cooldown = 12000; target = me; }
                else if (me->IsWithinDistInMap(victim, 8.f))
                    { spell = 10890; cooldown = 25000; target = me; }
            }
            else if (_profile == 3)
            {
                if (me->IsWithinDistInMap(victim, 8.f))
                    { spell = 781; cooldown = 16000; target = me; }
                else if (!victim->HasAura(5116))
                    { spell = 5116; cooldown = 12000; }
            }
            else if (_profile == HolyPaladin && me->HealthBelowPct(30))
                { spell=498; cooldown=60000; target=me; } // Divine Protection
            else if (_profile == RestorationShaman && me->HealthBelowPct(50) && !me->HasAura(49284))
                { spell=49284; cooldown=15000; target=me; } // Earth Shield
            else if (_profile == Rogue && me->HealthBelowPct(50))
                { spell=5277; cooldown=180000; target=me; } // Evasion
            else if (_profile == Warlock && me->HealthBelowPct(40))
                { spell=47860; cooldown=120000; } // Death Coil
            if (spell && CastAbility(target, spell, _defensiveTimer, cooldown))
                return;
        }
        if (!_secondaryTimer)
        {
            uint32 spell = 0;
            Unit* target = victim;
            if (_profile == 0 && me->IsWithinMeleeRange(victim))
            {
                if (victim->HealthBelowPct(20)) spell = 47471;
                else if (!victim->HasAura(1715) && victim->isMoving()) spell = 1715;
                else { spell = 1680; target = me; } // Whirlwind is self-targeted
            }
            else if (_profile == 1)
            {
                if (me->IsWithinDistInMap(victim, 10.f)) { spell = 42931; target = me; }
                else spell = 42914;
            }
            else if (_profile == 2 && !victim->HasAura(48125)) spell = 48125;
            else if (_profile == 3) spell = !victim->HasAura(49001) ? 49001 : 49048;
            else if (_profile == HolyPaladin && me->IsWithinDistInMap(victim,10.f)
                && victim->GetVictim() && IsBotWarHealer(victim->GetVictim())) spell=10308;
            else if (_profile == RestorationShaman && !victim->HasAura(49236)) spell=49236; // Frost Shock
            else if (_profile == Rogue && me->IsWithinMeleeRange(victim)) spell=51722; // Dismantle, no combo points needed
            else if (_profile == Warlock && !victim->HasAura(47813)) spell=47813;
            if ((spell==1680 || spell==42931 || spell==49048)
                && HasBreakableControlNear(spell==49048?victim:me,me,12.f))
            {
                spell=0;
                _secondaryTimer=500;
            }
            uint32 cooldown = _profile==Rogue ? 60000 : _profile==HolyPaladin ? 60000 : urand(3500,6000);
            if (spell && CastAbility(target, spell, _secondaryTimer, cooldown))
                return;
        }
        if (!_primaryTimer)
        {
            uint32 spell = PrimarySpell(_profile);
            if (!IsMelee(_profile) && !CanAfford(spell))
            {
                me->Attack(victim, true);
                if (!me->isMoving())
                    me->GetMotionMaster()->MoveChase(victim, 0.f);
                DoMeleeAttackIfReady();
                _primaryTimer = 1000;
            }
            else if (!IsMelee(_profile) || me->IsWithinMeleeRange(victim))
            {
                if (!IsMelee(_profile))
                {
                    me->Attack(victim, false);
                    me->StopMoving();
                }
                CastAbility(victim, spell, _primaryTimer, _profile==HolyPaladin ? 15000 : _profile==Warrior ? 3000 : 2000);
            }
        }
        if (IsMelee(_profile))
            DoMeleeAttackIfReady();
    }

private:
    BotRouteMovement _route;
    Position _combatOrigin;
    bool _combatOriginSet=false;
    ObjectGuid _vehicle, _buildSite;
    BotWarVehicleNavigation _supplyNavigation;
    Position _buildPosition;
    Lane _requestedLane = LaneCount;
    uint32 _killCredits = 0, _supplyMoveTimer = 0;
    uint8 _profile;
    uint32 _primaryTimer = 1000;
    uint32 _secondaryTimer = 4000;
    uint32 _defensiveTimer = 0, _interruptTimer = 0;
    uint32 _trinketTimer = 0;
    uint32 _thinkTimer = 1000;
    uint32 _healScanTimer = 500;
    uint32 _captureTimer = 0;
    uint32 _boardingTimer = 0, _boardProgressTimer = 0;
    float _boardProgressDistance = 0.f;
    uint32 _buffTimer = 1000;
    uint32 _lostSightTimer = 0;
};

npc_wg_bot_soldier* GetBotWarSoldierAI(Creature* soldier)
{
    if (!soldier) return nullptr;
    CreatureAI* ai = soldier->IsNPCBot() ? soldier->GetBotAI()->GetBattlefieldAI() : soldier->AI();
    return dynamic_cast<npc_wg_bot_soldier*>(ai);
}

void RetireBotWarSoldier(Creature* soldier)
{
    if (soldier->IsNPCBot() && soldier->GetBotAI()->IsBattlefieldBot())
    {
        if (soldier->GetVehicle()) soldier->ExitVehicle();
        soldier->CombatStop(true);
        soldier->StopMoving();
        soldier->SetVisible(false);
        BotDataMgr::DespawnWandererBot(soldier->GetEntry());
    }
    else soldier->DespawnOrUnsummon();
}

struct npc_wg_bot_siege_turret : public ScriptedAI
{
    npc_wg_bot_siege_turret(Creature* creature) : ScriptedAI(creature) { SetCombatMovement(false); }

    void KilledUnit(Unit* victim) override { AwardBotWarKill(me, victim); }
    void OnSpellCast(SpellInfo const* spell) override
    {
        RecordBotAbility(me, me->GetVictim(), spell->Id);
    }

    void OnCharmed(bool isNew) override
    {
        if (me->IsCharmed() && !IsBotWarSoldier(me->GetCharmer()))
            UnitAI::OnCharmed(isNew);
        if (!me->IsCharmed())
            me->LastCharmerGUID.Clear();
    }

    void MoveInLineOfSight(Unit* /*unit*/) override
    {
    }

    void UpdateAI(uint32 diff) override
    {
        if (!GetBotWarBattlefield() || !me->GetVehicleKit())
            return;
        Unit* gunner = me->GetVehicleKit()->GetPassenger(0);
        if (!IsBotWarSoldier(gunner))
            return;
        if (_timer > diff)
        {
            _timer -= diff;
            return;
        }
        if (YieldBotWarSeat(me))
            return;
        Unit* engine = me->GetVehicleBase();
        if (!engine)
            return;
        me->SetFaction(engine->GetFaction());

        // Prioritize destroying the barrier gate/wall if a siege target is in range!
        if (auto* ai = CAST_AI(npc_wg_bot_vehicle, engine->ToCreature()->AI()))
        {
            if (GameObject* building = ai->SiegeTarget())
            {
                if (me->GetExactDist2d(building) >= 10.f && me->GetExactDist2d(building) <= 55.f)
                {
                    SpellCastResult result = me->CastSpell(building->GetPosition(), 57609, false);
                    _timer = CastRetryDelay(result == SPELL_CAST_OK, 2000);
                    return;
                }
            }
        }

        // Target hostile siege vehicles first (up to 65m)
        Creature* vehTarget = nullptr;
        for (uint32 entry : { uint32(NPC_WINTERGRASP_DEMOLISHER), uint32(NPC_WINTERGRASP_CATAPULT),
                              uint32(NPC_WINTERGRASP_SIEGE_ENGINE_ALLIANCE), uint32(NPC_WINTERGRASP_SIEGE_ENGINE_HORDE) })
        {
            std::list<Creature*> vehs;
            GetCreatureListWithEntryInGrid(vehs, me, entry, 65.f);
            for (Creature* v : vehs)
            {
                if (v->IsAlive() && me->IsValidAttackTarget(v) && me->IsWithinLOSInMap(v))
                {
                    vehTarget = v;
                    break;
                }
            }
            if (vehTarget)
                break;
        }
        if (vehTarget)
        {
            SpellCastResult result = me->CastSpell(vehTarget->GetPosition(), 57609, false);
            _timer = CastRetryDelay(result == SPELL_CAST_OK, 2000);
            return;
        }

        if (Unit* target = engine->ToCreature()->SelectNearestTarget(50.f))
        {
            if (me->IsWithinLOSInMap(target))
            {
                SpellCastResult result = me->CastSpell(target->GetPosition(), 57609, false);
                _timer = CastRetryDelay(result == SPELL_CAST_OK, 2200);
                return;
            }
        }
        _timer = 500;
    }

private:
    uint32 _timer = 1500;
};

struct npc_wg_tower_cannon : public ScriptedAI
{
    npc_wg_tower_cannon(Creature* creature) : ScriptedAI(creature) { SetCombatMovement(false); }

    void KilledUnit(Unit* victim) override { AwardBotWarKill(me, victim); }
    void OnSpellCast(SpellInfo const* spell) override
    {
        RecordBotAbility(me, me->GetVictim(), spell->Id);
    }

    void UpdateAI(uint32 diff) override
    {
        if (!GetBotWarBattlefield())
            return;
        if (Vehicle* kit = me->GetVehicleKit())
            if (Unit* driver = kit->GetPassenger(0))
                if (driver->ToPlayer())
                    return;
        if (_blastTimer > diff)
        {
            _blastTimer -= diff;
            return;
        }
        _blastTimer = 3500;
        Creature* target = nullptr;
        for (uint32 entry : { uint32(NPC_WINTERGRASP_DEMOLISHER), uint32(NPC_WINTERGRASP_CATAPULT),
            uint32(NPC_WINTERGRASP_SIEGE_ENGINE_ALLIANCE), uint32(NPC_WINTERGRASP_SIEGE_ENGINE_HORDE) })
        {
            std::list<Creature*> vehicles;
            GetCreatureListWithEntryInGrid(vehicles, me, entry, 65.f);
            for (Creature* vehicle : vehicles)
                if (vehicle->IsAlive() && me->IsValidAttackTarget(vehicle) && me->IsWithinLOSInMap(vehicle)
                    && (!target || me->GetDistance(vehicle) < me->GetDistance(target)))
                    target = vehicle;
        }
        Unit* victim = target ? target : me->SelectNearestTarget(60.f);
        if (victim && me->IsWithinLOSInMap(victim))
        {
            me->SetFacingToObject(victim);
            SpellCastResult result = me->CastSpell(victim->GetPosition(), 51421, false);
            _blastTimer = CastRetryDelay(result == SPELL_CAST_OK, 3500);
        }
    }

private:
    uint32 _blastTimer = urand(1000, 3000);
};

// A complete snapshot is sent once a second, including vehicles outside the
// player's visibility grid. The companion WGVehicleMap addon draws the pins.
class WintergraspVehicleMapUpdater
{
public:
    void Update(Map* map, uint32 diff)
    {
        if (_timer > diff)
        {
            _timer -= diff;
            return;
        }
        _timer = 1000;
        Battlefield* battlefield = sBattlefieldMgr->GetBattlefieldByBattleId(BATTLEFIELD_BATTLEID_WG);
        if (!battlefield || !battlefield->IsEnabled())
            return;
        auto* wg = static_cast<BattlefieldWG*>(battlefield);

        std::vector<Player*> viewers;
        for (auto const& ref : map->GetPlayers())
            if (Player* player = ref.GetSource())
                if (player->GetZoneId() == wg->GetZoneId())
                    viewers.push_back(player);
        if (viewers.empty())
            return;

        std::string sequence = std::to_string(++_sequence);
        std::vector<std::string> messages = { "B;" + sequence };
        std::string rows = "V;" + sequence + ";";
        if (wg->IsWarTime())
            for (TeamId team : { TEAM_ALLIANCE, TEAM_HORDE })
                for (ObjectGuid guid : wg->GetVehicleGuids(team))
                    if (Creature* vehicle = map->GetCreature(guid))
                        if (vehicle->IsAlive() && vehicle->IsInWorld())
                        {
                            uint32 kind;
                            switch (vehicle->GetEntry())
                            {
                                case NPC_WINTERGRASP_CATAPULT: kind = 1; break;
                                case NPC_WINTERGRASP_DEMOLISHER: kind = 2; break;
                                case NPC_WINTERGRASP_SIEGE_ENGINE_ALLIANCE:
                                case NPC_WINTERGRASP_SIEGE_ENGINE_HORDE: kind = 3; break;
                                default: continue;
                            }
                            float x = vehicle->GetPositionX(), y = vehicle->GetPositionY();
                            Map2ZoneCoordinates(x, y, wg->GetZoneId());
                            if (!std::isfinite(x) || !std::isfinite(y) || x < 0.f || x > 100.f || y < 0.f || y > 100.f)
                                continue;
                            std::string row = std::to_string(uint32(team)) + "," + std::to_string(kind) + ","
                                + std::to_string(uint32(std::lround(x * 100.f))) + ","
                                + std::to_string(uint32(std::lround(y * 100.f))) + "|";
                            // 3.3.5 addon messages have a 255 byte limit, prefix included.
                            if (rows.size() + row.size() > 230)
                            {
                                messages.push_back(rows);
                                rows = "V;" + sequence + ";";
                            }
                            rows += row;
                        }
        messages.push_back(rows);
        messages.push_back("E;" + sequence);
        for (Player* player : viewers)
            for (std::string const& message : messages)
            {
                WorldPackets::Chat::Chat packet;
                packet.Initialize(CHAT_MSG_WHISPER, LANG_ADDON, player, player, message, 0, "", DEFAULT_LOCALE, "WGVM1");
                player->SendDirectMessage(packet.Write());
            }
    }

private:
    uint32 _timer = 1000;
    uint32 _sequence = 0;
};

class WintergraspBotWarKillScript : public PlayerScript
{
public:
    WintergraspBotWarKillScript() : PlayerScript("WintergraspBotWarKillScript") { }
    void OnPVPKill(Player* killer, Player* victim) override { AwardBotWarKill(killer, victim); }
    void OnCreatureKill(Player* killer, Creature* victim) override { AwardBotWarKill(killer, victim); }
};

// Map callbacks keep summons, movement and GUID lookups on Northrend's update thread.
class WintergraspBotWarMapScript : public WorldMapScript
{
public:
    WintergraspBotWarMapScript() : WorldMapScript("WintergraspBotWarMapScript", 571) { }

    void OnUpdate(Map* map, uint32 diff) override
    {
        // ScriptMgr dispatches only one WorldMapScript per map. Keep vehicle
        // telemetry and the bot army in the same callback, including peace time.
        _vehicleMap.Update(map, diff);
        BattlefieldWG* wg = GetBotWarBattlefield();
        bool enabled = sConfigMgr->GetBoolDefault("Wintergrasp.BotWar.Enable", false, true);
        bool hasPlayers = wg && (wg->GetWarPlayerCount(TEAM_ALLIANCE) || wg->GetWarPlayerCount(TEAM_HORDE));
        // The schedule alone must not let two bot armies fight and capture the
        // relic unattended. Late entrants activate the same ongoing battle.
        if (!wg || !enabled || !hasPlayers)
        {
            if (_running)
                Cleanup(map);
            return;
        }
        if (!_running)
        {
            _running = true;
            if (!BotBattleStats.Participated)
            {
                BotBattleStats.Participated=true;
                BotBattleStats.ArmyStartedMs=BotBattleStats.Elapsed;
            }
            _timer = 4000;
            _elapsed = 0;
            _defender = wg->GetDefenderTeam();
            _battleSerial = wg->GetBotWarBattleSerial();
            _nextReport = 15000;
        }
        // A GM can end and restart a battle between map ticks.
        if (_defender != wg->GetDefenderTeam() || _battleSerial != wg->GetBotWarBattleSerial())
        {
            Cleanup(map);
            return;
        }
        _elapsed += diff;
        if (_timer > diff)
        {
            _timer -= diff;
            return;
        }
        _timer = 1000;
        uint32 target = uint32(std::clamp(sConfigMgr->GetIntDefault("Wintergrasp.BotWar.TargetPerTeam", 120, true), 0, 120));
        for (TeamId team : { TEAM_ALLIANCE, TEAM_HORDE })
            Reconcile(map, wg, team, target);
        if (_elapsed >= _nextReport)
        {
            _nextReport = _elapsed + 30000;
            for (TeamId team : { TEAM_ALLIANCE, TEAM_HORDE })
            {
                uint32 alive = 0;
                for (RosterSlot const& slot : _roster[team])
                    if (Creature* soldier = map->GetCreature(slot.Soldier))
                        if (soldier->IsAlive())
                            ++alive;
                TC_LOG_INFO("bg.battlefield", "Wintergrasp BotWar: team={} humans={} bots={} budget={} vehicles={}/{}",
                    team == TEAM_ALLIANCE ? "Alliance" : "Horde", wg->GetWarPlayerCount(team), alive,
                    BotBudget(target, wg->GetMaxPlayersPerTeam(), wg->GetWarPlayerCount(team)),
                    wg->GetData(team == TEAM_ALLIANCE ? BATTLEFIELD_WG_DATA_VEHICLE_A : BATTLEFIELD_WG_DATA_VEHICLE_H),
                    wg->GetData(team == TEAM_ALLIANCE ? BATTLEFIELD_WG_DATA_MAX_VEHICLE_A : BATTLEFIELD_WG_DATA_MAX_VEHICLE_H));
            }
        }
    }

    void OnDestroy(Map* map) override { Cleanup(map); }

private:
    struct RosterSlot
    {
        ObjectGuid Soldier;
        ObjectGuid Vehicle;
        uint32 RespawnAt = 0;
        uint32 VehicleRespawnAt = 0;
        uint32 KillCredits = 0;
        uint32 BuildStartedAt = 0;
        ObjectGuid Engineer;
        uint8 FactoryId = 255;
        Position BuildPoint;
    };

    void RemoveSlot(Map* map, RosterSlot& slot)
    {
        if (Creature* vehicle = map->GetCreature(slot.Vehicle))
        {
            Unit* driver = vehicle->GetVehicleKit() ? vehicle->GetVehicleKit()->GetPassenger(0) : nullptr;
            if (PreserveVehicle(vehicle->IsAlive(), false, HasBotWarHumanPassenger(vehicle), IsBotWarSoldier(driver)))
            {
                if (std::find(_claimedVehicles.begin(), _claimedVehicles.end(), slot.Vehicle) == _claimedVehicles.end())
                    _claimedVehicles.push_back(slot.Vehicle);
            }
            else
                vehicle->DespawnOrUnsummon();
        }
        if (Creature* soldier = map->GetCreature(slot.Soldier))
        {
            slot.KillCredits = soldier->AI()->GetData(RankCreditData);
            RetireBotWarSoldier(soldier);
        }
        slot.Engineer.Clear();
        slot.BuildStartedAt = 0;
        slot.Soldier.Clear();
        slot.Vehicle.Clear();
        slot.RespawnAt = _elapsed + 300000;
    }

    void RetireSoldier(Map* map, RosterSlot& slot)
    {
        if (Creature* soldier = map->GetCreature(slot.Soldier))
        {
            slot.KillCredits = soldier->AI()->GetData(RankCreditData);
            RetireBotWarSoldier(soldier);
        }
        // TEMPSUMMON_MANUAL_DESPAWN vehicles remain registered and available
        // to surviving squad members or to the leader after their resurrection.
        slot.Soldier.Clear();
        slot.Engineer.Clear();
        slot.BuildStartedAt = 0;
        slot.RespawnAt = _elapsed + 300000;
    }

    void Reconcile(Map* map, BattlefieldWG* wg, TeamId team, uint32 target)
    {
        uint32 budget = BotBudget(target, wg->GetMaxPlayersPerTeam(), wg->GetWarPlayerCount(team));
        for (uint32 i = budget; i < MaxParticipants; ++i)
            if (!_roster[team][i].Soldier.IsEmpty() || !_roster[team][i].Vehicle.IsEmpty())
                RemoveSlot(map, _roster[team][i]);

        uint32 spawnBudget = 10; // Bound pathfinding and summon work per map tick.
        for (uint32 i = 0; i < budget; ++i)
        {
            RosterSlot& slot = _roster[team][i];
            Creature* soldier = map->GetCreature(slot.Soldier);
            if (soldier)
                slot.KillCredits = soldier->AI()->GetData(RankCreditData);
            if (!slot.Soldier.IsEmpty() && (!soldier || !soldier->IsAlive()))
            {
                RetireSoldier(map, slot);
                continue;
            }
            if (!soldier && spawnBudget && slot.RespawnAt <= _elapsed)
            {
                --spawnBudget;
                SpawnSoldier(map, wg, team, i, slot);
                soldier = map->GetCreature(slot.Soldier);
            }
            EnsureVehicle(map, wg, team, i, slot, soldier);
        }

        if (_nextTactics[team] <= _elapsed)
        {
            _nextTactics[team] = _elapsed + 15000;
            UpdateTactics(map, wg, team);
            UpdateSiegeTactics(map, wg, team);
            if (BotBattleStats.Active)
            {
                auto const& c=BotBattleStats.Teams[team];
                uint32 fighting=0,moving=0,corporals=0,lieutenants=0,native=0;
                for (RosterSlot const& member:_roster[team])
                    if (Creature* bot=map->GetCreature(member.Soldier))
                        if (bot->IsAlive())
                        {
                            native+=bot->IsNPCBot();
                            fighting+=bot->IsInCombat();
                            moving+=bot->isMoving();
                            uint32 rank=RankForCredits(bot->AI()->GetData(RankCreditData));
                            corporals+=rank==1;
                            lieutenants+=rank==2;
                        }
                TC_LOG_INFO("bg.battlefield.botwar", "WG BotWar event=progress serial={} elapsed_ms={} team={} built={} lost={} salvaged={} captures={} infantry_stalls={} infantry_recovered={} vehicle_stalls={} vehicle_recovered={} interrupt_casts={} control_casts={} fighting={} moving={} corporals={} lieutenants={} native={} kills={} deaths={}",
                    BotBattleStats.Serial,BotBattleStats.Elapsed,team,c.Built,c.Lost,c.RecoveredVehicles,c.Captures,
                    c.InfantryStalls,c.InfantryRecovered,c.VehicleStalls,c.VehicleRecovered,c.Interrupts,c.Controls,fighting,moving,corporals,lieutenants,native,c.Kills,c.BotDeaths);
            }
        }
        for (auto it = _claimedVehicles.begin(); it != _claimedVehicles.end();)
        {
            Creature* vehicle = map->GetCreature(*it);
            if (!vehicle || !vehicle->IsAlive())
            {
                if (vehicle) vehicle->DespawnOrUnsummon();
                it = _claimedVehicles.erase(it);
                continue;
            }
            auto* vehicleAI = CAST_AI(npc_wg_bot_vehicle, vehicle->AI());
            if (vehicleAI && vehicle->GetFaction() == WGFactionTable[team] && !HasBotWarHumanPassenger(vehicle)
                && IsBotWarSeatAvailable(vehicle->GetVehicleKit(), 0))
            {
                Creature* nearest = nullptr;
                float distance = 250.f;
                for (uint32 i = 0; i < budget; ++i)
                    if (Creature* bot = map->GetCreature(_roster[team][i].Soldier))
                        if (bot->IsAlive() && !bot->IsInCombat() && !bot->GetVehicle() && vehicleAI->CanReserveDriver(bot))
                            if (auto* ai = GetBotWarSoldierAI(bot))
                                if (!IsTowerDefense(ai->GetLane()) && !ai->HasBuildSite() && ai->EscortVehicle().IsEmpty()
                                    && bot->GetExactDist(vehicle) < distance)
                                {
                                    nearest = bot;
                                    distance = bot->GetExactDist(vehicle);
                                }
                if (nearest && vehicleAI->ReserveDriver(nearest))
                    GetBotWarSoldierAI(nearest)->SetEscortVehicle(vehicle->GetGUID());
            }
            ++it;
        }
    }

    void SpawnSoldier(Map* map, BattlefieldWG* wg, TeamId team, uint32 index, RosterSlot& slot)
    {
        Lane lane = ResolveSquadLane(wg, team == wg->GetAttackerTeam(), index);
        uint32 profile = ProfileForSlot(index);
        static constexpr uint8 classes[] = { BOT_CLASS_WARRIOR, BOT_CLASS_MAGE, BOT_CLASS_PRIEST, BOT_CLASS_HUNTER,
            BOT_CLASS_PALADIN, BOT_CLASS_SHAMAN, BOT_CLASS_ROGUE, BOT_CLASS_WARLOCK };
        static constexpr uint8 specs[] = { BOT_SPEC_WARRIOR_ARMS, BOT_SPEC_MAGE_FROST, BOT_SPEC_PRIEST_HOLY,
            BOT_SPEC_HUNTER_MARKSMANSHIP, BOT_SPEC_PALADIN_HOLY, BOT_SPEC_SHAMAN_RESTORATION,
            BOT_SPEC_ROGUE_SUBTLETY, BOT_SPEC_WARLOCK_AFFLICTION };
        Position pos = BotRoutes[lane][0].Pos;
        bool forwardSpawn = false;

        if (team == wg->GetDefenderTeam())
        {
            // DEFENDER SPAWN LOGIC: Defenders belong inside the Fortress Keep & Courtyard
            // (or at forward workshops if owned by defenders). Never spawn in the middle of the map!
            if (lane == KeepWest || lane == SouthWest)
            {
                uint32 area = AREA_THE_BROKEN_TEMPLATE;
                if (wg->GetData(area) == uint32(team))
                {
                    if (GameObject* flag = wg->GetBotWarWorkshopFlag(true))
                        pos = flag->GetPosition();
                }
                else
                    pos = Position{5392.91f, 2975.26f, 415.22f, 3.14f}; // Keep West Workshop (inside courtyard)
            }
            else if (lane == KeepEast || lane == SouthEast)
            {
                uint32 area = AREA_THE_SUNKEN_RING;
                if (wg->GetData(area) == uint32(team))
                {
                    if (GameObject* flag = wg->GetBotWarWorkshopFlag(false))
                        pos = flag->GetPosition();
                }
                else
                    pos = Position{5391.61f, 2707.72f, 415.05f, 3.14f}; // Keep East Workshop (inside courtyard)
            }
            else if (lane == KeepCenter || lane == South)
            {
                pos = Position{5392.00f, 2841.00f, 415.00f, 3.14f}; // Keep Courtyard Center (inside courtyard)
            }
            else if (IsWorkshop(lane))
            {
                if (GameObject* flag = wg->GetBotWarWorkshopFlagForArea(WorkshopArea(lane)))
                    pos = flag->GetPosition();
                else
                    pos = Position{5392.00f, 2841.00f, 415.00f, 3.14f};
            }
            else
            {
                pos = Position{5392.00f, 2841.00f, 415.00f, 3.14f};
            }
        }
        else // team == wg->GetAttackerTeam()
        {
            // ATTACKER SPAWN LOGIC: Attackers stage from Southern workshops and camps.
            if (lane == West || lane == GuardSouthWest || lane == WorkshopSouthWest)
            {
                uint32 area = AREA_THE_BROKEN_TEMPLATE;
                if (lane == West && wg->GetData(area) == uint32(team))
                {
                    if (GameObject* flag = wg->GetBotWarWorkshopFlag(true))
                    {
                        pos = flag->GetPosition();
                        forwardSpawn = true;
                    }
                }
                else
                    pos = Position{4360.00f, 3310.00f, 373.00f, 0.f}; // Westpark Workshop (mechanic)
            }
            else if (lane == East || lane == GuardSouthEast || lane == WorkshopSouthEast)
            {
                uint32 area = AREA_THE_SUNKEN_RING;
                if (lane == East && wg->GetData(area) == uint32(team))
                {
                    if (GameObject* flag = wg->GetBotWarWorkshopFlag(false))
                    {
                        pos = flag->GetPosition();
                        forwardSpawn = true;
                    }
                }
                else
                    pos = Position{4362.00f, 2358.00f, 377.00f, 0.f}; // Eastpark Workshop (mechanic)
            }
            else if (lane == Center || lane == GuardSouth)
                pos = Position{4452.86f, 2808.87f, 402.60f, 0.f}; // Southern staging camp near the central bridge.
            else if (IsWorkshop(lane))
            {
                if (GameObject* flag = wg->GetBotWarWorkshopFlagForArea(WorkshopArea(lane)))
                    pos = flag->GetPosition();
            }
        }
        pos=BotFormationOffset(pos,index);
        ProjectBotWarGround(map,pos,true);
        map->LoadGrid(pos.GetPositionX(), pos.GetPositionY());
        WanderNode const* spawn = nullptr;
        float nearestNode = std::numeric_limits<float>::max();
        WanderNode::DoForAllZoneWPs(4197, [&](WanderNode const* node) {
            if (node->GetMapId() != 571 || node->GetLinks().empty()) return;
            float distance = node->GetExactDist2d(&pos);
            if (distance < nearestNode) { nearestNode = distance; spawn = node; }
        });
        uint32 entry = BotDataMgr::SpawnBattlefieldBot(80, spawn, pos,
            team == TEAM_ALLIANCE ? ALLIANCE : HORDE, WGFactionTable[team], classes[profile], specs[profile]);
        Creature* soldier = entry ? const_cast<Creature*>(BotDataMgr::FindBot(entry)) : nullptr;
        if (!soldier)
        {
            if (index % SquadSize == 0)
                TC_LOG_ERROR("bg.battlefield.botwar", "WG BotWar native spawn unavailable: team={} class={} node={}",
                    uint32(team), uint32(classes[profile]), spawn ? spawn->GetWPId() : 0);
            slot.RespawnAt = _elapsed + 30000; // Missing SQL must not cause summon spam.
            return;
        }
        soldier->GetBotAI()->SetBattlefieldAI(std::make_unique<npc_wg_bot_soldier>(soldier));
        auto* ai = GetBotWarSoldierAI(soldier);
        if (!ai)
        {
            RetireBotWarSoldier(soldier);
            slot.RespawnAt = _elapsed + 30000;
            return;
        }
        soldier->SetFaction(WGFactionTable[team]);
        soldier->setActive(true);
        soldier->SetPvP(true);
        soldier->SetReactState(REACT_AGGRESSIVE);
        soldier->GetBotAI()->BreakSpawnTruce(); // These are active PvP participants, not ambient wanderers.
        ai->ConfigureProfile(index);
        soldier->RemoveByteFlag(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_FFA_PVP);
        soldier->GetBotAI()->SetBotCommandState(BOT_COMMAND_NOGOSSIP);
        soldier->RemoveNpcFlag(UNIT_NPC_FLAG_GOSSIP);
        TC_LOG_INFO("bg.battlefield.botwar", "WG BotWar event=spawn name={} entry={} prototype={} class={} team={}",
            soldier->GetName(), entry, soldier->GetCreatureTemplate()->KillCredit[0], classes[profile], uint32(team));
        slot.Soldier = soldier->GetGUID();

        ObjectGuid escortVehicle = _roster[team][index - index % SquadSize].Vehicle;
        uint32 firstPoint = 0;
        if (forwardSpawn)
        {
            auto const& route = BotRoutes[lane];
            auto workshop = std::find_if(route.begin(), route.end(), [](BotWaypoint const& point) { return point.CaptureArea != 0; });
            if (workshop != route.end())
                firstPoint = uint32(workshop - route.begin());
        }
        ai->SetData(RankCreditData, slot.KillCredits);
        ai->SetOrders(lane, index, escortVehicle, firstPoint);
    }

    Creature* FindBuildEngineer(Map* map, BattlefieldWG* wg, TeamId team, Lane lane,
        Creature* soldier, uint8& factoryId)
    {
        Creature* best = nullptr;
        float distance = 220.f;
        for (BotWorkshopSite const& site : BotWorkshopSites)
        {
            if (wg->GetBotWarWorkshopControl(site.Id) != team)
                continue;
            // Do not send field troops through closed keep walls to construct.
            if (!site.Area && (IsAssault(lane) || IsCounterAttack(lane) || soldier->GetPositionX() < 5150.f))
                continue;
            if (site.Area && !IsAssault(lane) && !IsCounterAttack(lane) && soldier->GetPositionX() >= 5150.f)
                continue;
            Position pos = site.Fallback;
            map->LoadGrid(pos.GetPositionX(), pos.GetPositionY());
            GameObject* flag = site.Area ? wg->GetBotWarWorkshopFlagForArea(site.Area) : nullptr;
            if (site.Area && !flag)
                continue;
            if (flag)
                pos = flag->GetPosition();
            uint32 entry = team == TEAM_ALLIANCE ? NPC_GNOMISH_ENGINEER : NPC_GOBLIN_MECHANIC;
            std::list<Creature*> engineers;
            if (flag)
                GetBotWarEngineers(engineers, flag, entry, 100.f, team, false);
            else
                GetBotWarEngineers(engineers, soldier, entry, 250.f, team, true);
            for (Creature* engineer : engineers)
                if (engineer->IsAlive() && engineer->IsFriendlyTo(soldier)
                    && engineer->GetExactDist2d(&pos) <= 100.f && soldier->GetExactDist(&pos) < distance)
                {
                    best = engineer;
                    distance = soldier->GetExactDist(&pos);
                    factoryId = site.Id;
                }
        }
        return best;
    }

    void EnsureVehicle(Map* map, BattlefieldWG* wg, TeamId team, uint32 index, RosterSlot& slot, Creature* soldier)
    {
        if (index % SquadSize != 0 || !soldier || !soldier->IsAlive())
            return;
        auto* ai = GetBotWarSoldierAI(soldier);
        if (!ai)
            return;
        if (Creature* vehicle = map->GetCreature(slot.Vehicle))
        {
            if (vehicle->IsAlive())
                return;
            vehicle->DespawnOrUnsummon();
            slot.Vehicle.Clear();
            slot.VehicleRespawnAt = _elapsed + 10000;
        }
        Lane lane = ResolveSquadLane(wg, team == wg->GetAttackerTeam(), index);
        lane = ai->GetLane(); // live tactical orders take precedence over the initial roster
        uint32 current = wg->GetData(team == TEAM_ALLIANCE ? BATTLEFIELD_WG_DATA_VEHICLE_A : BATTLEFIELD_WG_DATA_VEHICLE_H);
        uint32 maximum = wg->GetData(team == TEAM_ALLIANCE ? BATTLEFIELD_WG_DATA_MAX_VEHICLE_A : BATTLEFIELD_WG_DATA_MAX_VEHICLE_H);
        uint32 humanReserve = wg->GetWarPlayerCount(team) && maximum ? 1 : 0;
        auto cancelBuild = [&] {
            slot.Engineer.Clear();
            slot.BuildStartedAt = 0;
            ai->SetBuildSite(ObjectGuid::Empty);
        };
        bool eligibleLane = IsAssault(lane) || IsCounterAttack(lane) || IsWorkshop(lane)
            || lane == KeepCenter || lane == KeepWest || lane == KeepEast;
        bool pinned = soldier->IsInCombat() && (!soldier->getAttackers().empty()
            || soldier->HealthBelowPct(35) || soldier->HasUnitState(UNIT_STATE_CONTROLLED));
        if (!eligibleLane || !HasVehicleBuildRank(slot.KillCredits, team == wg->GetAttackerTeam()) || pinned || soldier->GetVehicle()
            || !ai->EscortVehicle().IsEmpty()
            || current >= maximum - humanReserve)
        {
            cancelBuild();
            return;
        }
        Creature* engineer = map->GetCreature(slot.Engineer);
        if (!slot.Engineer.IsEmpty() && (!engineer || wg->GetBotWarWorkshopControl(slot.FactoryId) != team
            || !engineer->IsAlive() || !engineer->IsFriendlyTo(soldier)))
        {
            cancelBuild();
            engineer = nullptr;
        }
        if (engineer && !slot.BuildStartedAt && _elapsed > slot.VehicleRespawnAt + 30000)
        {
            cancelBuild();
            slot.VehicleRespawnAt = _elapsed + 45000;
            return;
        }
        if (!engineer)
        {
            uint32 dispatched = 0;
            for (RosterSlot const& other : _roster[team])
            {
                if (!other.Engineer.IsEmpty()) ++dispatched;
                else if (Creature* pending = map->GetCreature(other.Vehicle))
                    if (pending->IsAlive() && pending->GetVehicleKit() && !pending->GetVehicleKit()->GetPassenger(0))
                        ++dispatched; // A waiting driver still occupies a dispatch slot.
            }
            if (dispatched >= MaxConcurrentBuilders)
                return;
            if (slot.VehicleRespawnAt > _elapsed)
                return;
            slot.VehicleRespawnAt = _elapsed + 10000;
            engineer = FindBuildEngineer(map, wg, team, lane, soldier, slot.FactoryId);
            if (!engineer)
            {
                cancelBuild();
                return;
            }
            slot.Engineer = engineer->GetGUID();
            slot.BuildPoint = engineer->GetPosition();
            // Field engineers are often on a workshop platform. Rally the builder on open ground by its flag.
            for (BotWorkshopSite const& site : BotWorkshopSites)
                if (site.Id == slot.FactoryId && site.Area)
                {
                    if (GameObject* flag = wg->GetBotWarWorkshopFlagForArea(site.Area))
                    {
                        Position ground = flag->GetPosition();
                        float angle = engineer->GetAbsoluteAngle(flag);
                        ground.m_positionX += std::cos(angle) * 8.f;
                        ground.m_positionY += std::sin(angle) * 8.f;
                        slot.BuildPoint = ProjectBotWarGround(map, ground, true) ? ground : flag->GetPosition();
                    }
                    break;
                }
            TC_LOG_INFO("bg.battlefield.botwar", "WG BotWar event=builder_dispatch team={} name={} credits={} active_builders={}",
                uint32(team), soldier->GetName(), slot.KillCredits, dispatched + 1);
        }
        if (soldier->IsInCombat())
        {
            soldier->CombatStop(true);
            soldier->GetThreatManager().ClearAllThreat();
        }
        ai->SetBuildSite(slot.Engineer, slot.BuildPoint);
        if (soldier->GetExactDist(&slot.BuildPoint) > 12.f || soldier->HasUnitState(UNIT_STATE_CONTROLLED | UNIT_STATE_CASTING))
        {
            slot.BuildStartedAt = 0;
            return;
        }
        if (!slot.BuildStartedAt)
        {
            slot.BuildStartedAt = _elapsed;
            TC_LOG_INFO("bg.battlefield.botwar", "WG BotWar event=construction_start team={} workshop={}", uint32(team), uint32(slot.FactoryId));
            if (Creature* arms = engineer->FindNearestCreature(NPC_WINTERGRASP_CONTROL_ARMS, 30.f))
                engineer->CastSpell(arms, SPELL_ACTIVATE_CONTROL_ARMS, true);
            return;
        }
        if (_elapsed - slot.BuildStartedAt < 5000)
            return;
        // Ownership, capacity, rank, presence and combat were all rechecked above.
        Position pos = slot.BuildPoint;
        pos.m_positionX += std::cos(pos.GetOrientation()) * 4.f;
        pos.m_positionY += std::sin(pos.GetOrientation()) * 4.f;
        if (!ProjectBotWarGround(map, pos, true))
            pos = soldier->GetPosition();
        uint32 catapults = 0, demolishers = 0, siegeEngines = 0;
        for (ObjectGuid guid : wg->GetVehicleGuids(team))
            if (Creature* vehicle = map->GetCreature(guid); vehicle && vehicle->IsAlive())
            {
                uint32 vehicleEntry = vehicle->GetEntry();
                catapults += vehicleEntry == NPC_WINTERGRASP_CATAPULT;
                demolishers += vehicleEntry == NPC_WINTERGRASP_DEMOLISHER;
                siegeEngines += vehicleEntry == NPC_WINTERGRASP_SIEGE_ENGINE_ALLIANCE || vehicleEntry == NPC_WINTERGRASP_SIEGE_ENGINE_HORDE;
            }
        VehicleRole role = NextVehicle(team == wg->GetAttackerTeam(), catapults, demolishers, siegeEngines);
        uint32 entry = role == VehicleRole::Catapult ? NPC_WINTERGRASP_CATAPULT : NPC_WINTERGRASP_DEMOLISHER;
        if (role == VehicleRole::SiegeEngine)
            entry = team == TEAM_ALLIANCE ? NPC_WINTERGRASP_SIEGE_ENGINE_ALLIANCE : NPC_WINTERGRASP_SIEGE_ENGINE_HORDE;

        Lane vehLane = lane;
        if (slot.FactoryId == BATTLEFIELD_WG_WORKSHOP_SW || slot.FactoryId == BATTLEFIELD_WG_WORKSHOP_NW)
            vehLane = West;
        else if (slot.FactoryId == BATTLEFIELD_WG_WORKSHOP_SE || slot.FactoryId == BATTLEFIELD_WG_WORKSHOP_NE)
            vehLane = East;
        else if (slot.FactoryId == BATTLEFIELD_WG_WORKSHOP_KEEP_WEST)
            vehLane = KeepWest;
        else if (slot.FactoryId == BATTLEFIELD_WG_WORKSHOP_KEEP_EAST)
            vehLane = KeepEast;

        if (Creature* vehicle = soldier->SummonCreature(entry, pos, TEMPSUMMON_MANUAL_DESPAWN))
        {
            if (auto* vehicleAI = CAST_AI(npc_wg_bot_vehicle, vehicle->AI()))
            {
                vehicle->setActive(true);
                vehicleAI->SetRoute(vehLane, index);
                vehicleAI->ReserveDriver(soldier);
                slot.Vehicle = vehicle->GetGUID();
                ai->SetTacticalLane(vehLane);
                TC_LOG_INFO("bg.battlefield.botwar", "WG BotWar event=vehicle_assigned team={} name={} entry={} credits={} lane={}",
                    uint32(team), soldier->GetName(), entry, slot.KillCredits, uint32(vehLane));
                for (uint32 i = index; i < std::min(index + SquadSize, MaxParticipants); ++i)
                    if (Creature* escort = map->GetCreature(_roster[team][i].Soldier))
                        if (auto* escortAI = GetBotWarSoldierAI(escort))
                        {
                            escortAI->SetTacticalLane(vehLane);
                            escortAI->SetEscortVehicle(slot.Vehicle);
                        }
            }
            else
                vehicle->DespawnOrUnsummon();
        }
        cancelBuild();
        slot.VehicleRespawnAt = _elapsed + 10000;
    }

    void UpdateTactics(Map* map, BattlefieldWG* wg, TeamId team)
    {
        bool attacker = team == wg->GetAttackerTeam();
        auto canReinforce = [attacker](Lane lane)
        {
            return attacker ? lane == West || lane == East : lane == KeepWest || lane == KeepEast;
        };
        struct Objective { Lane LaneId; Position Pos; uint32 Need; int Pressure; bool Hold; };
        std::vector<Objective> objectives;
        for (BotWorkshopSite const& site : BotWorkshopSites)
        {
            if (!site.Area)
                continue;
            map->LoadGrid(site.Fallback.GetPositionX(), site.Fallback.GetPositionY());
            GameObject* flag = wg->GetBotWarWorkshopFlagForArea(site.Area);
            if (!flag)
                continue;
            uint32 allies = 0, enemies = 0;
            std::list<Creature*> bots;
            GetCreatureListWithEntryInGrid(bots, flag, 0, 80.f);
            for (Creature* bot : bots)
                if (bot->IsAlive() && IsBotWarSoldier(bot))
                    (bot->GetFaction() == WGFactionTable[team] ? allies : enemies)++;
            std::list<Player*> players;
            GetPlayerListInGrid(players, flag, 80.f);
            for (Player* player : players)
                if (player->IsAlive() && player->IsPvP())
                    (player->GetTeamId() == team ? allies : enemies)++;
            bool owned = wg->GetBotWarWorkshopControl(site.Id) == team;
            uint32 need = WorkshopResponseSquads(owned, allies, enemies);
            objectives.push_back({site.Objective, flag->GetPosition(), need,
                (owned ? 0 : 100) + int(enemies) - int(allies), !owned || enemies != 0});
        }
        std::stable_sort(objectives.begin(), objectives.end(), [](Objective const& a, Objective const& b) {
            return a.Pressure > b.Pressure;
        });
        std::array<Lane, MaxParticipants / SquadSize> orders;
        for (uint32 squad = 0; squad < orders.size(); ++squad)
            orders[squad] = ResolveSquadLane(wg, team == wg->GetAttackerTeam(), squad * SquadSize);
        std::array<bool, MaxParticipants / SquadSize> assigned{};
        uint32 reserve = 4; // at most twenty infantry, never the keep core or intact tower guards
        for (uint32 squad = 0; squad < orders.size() && reserve; ++squad)
            if (Creature* leader = map->GetCreature(_roster[team][squad * SquadSize].Soldier))
                if (auto* ai = GetBotWarSoldierAI(leader))
                    for (Objective const& objective : objectives)
                        if (objective.Hold && ai->GetLane() == objective.LaneId && canReinforce(orders[squad]))
                        {
                            orders[squad] = objective.LaneId;
                            assigned[squad] = true;
                            --reserve;
                            break;
                        }
        for (Objective const& objective : objectives)
            for (uint32 n = 0; n < objective.Need && reserve; ++n)
            {
                uint32 best = uint32(orders.size());
                float distance = std::numeric_limits<float>::max();
                for (uint32 squad = 0; squad < orders.size(); ++squad)
                {
                    if (assigned[squad] || !canReinforce(orders[squad]))
                        continue;
                    RosterSlot const& slot = _roster[team][squad * SquadSize];
                    Creature* leader = map->GetCreature(slot.Soldier);
                    if (!leader || !leader->IsAlive() || leader->IsInCombat() || leader->GetVehicle()
                        || !slot.Vehicle.IsEmpty() || leader->GetPositionX() >= 5150.f)
                        continue; // never retreat through an intact keep wall
                    auto* ai = GetBotWarSoldierAI(leader);
                    if (!ai || ai->HasBuildSite())
                        continue;
                    float d = leader->GetExactDist2d(&objective.Pos);
                    // Keep existing response orders stable instead of swapping squads every scan.
                    if (ai->GetLane() == objective.LaneId)
                        d = std::max(0.f, d - 100.f);
                    if (d < distance)
                    {
                        distance = d;
                        best = squad;
                    }
                }
                if (best == orders.size())
                    break;
                orders[best] = objective.LaneId;
                assigned[best] = true;
                --reserve;
            }
        uint32 patrols = 0;
        for (uint32 squad = 0; !attacker && squad < orders.size() && patrols < 4; ++squad)
        {
            if (assigned[squad]) continue; // Capturing and saving a contested workshop always wins.
            if (!canReinforce(orders[squad])) continue;
            RosterSlot const& slot = _roster[team][squad * SquadSize];
            Creature* leader = map->GetCreature(slot.Soldier);
            auto* ai = GetBotWarSoldierAI(leader);
            if (!leader || !leader->IsAlive() || !ai || ai->HasBuildSite() || leader->GetVehicle()
                || !slot.Vehicle.IsEmpty()) continue;
            auto& patrol = _patrols[team][squad];
            if (patrol.Target == LaneCount || (_elapsed >= patrol.Until && !leader->IsInCombat()
                && (leader->GetExactDist2d(&BotRoutes[patrol.Target][0].Pos) < 80.f || _elapsed > patrol.Until + 90000)))
            {
                std::array<Lane, 6> candidates = { WorkshopWest, WorkshopEast, WorkshopSouthWest, WorkshopSouthEast,
                    SouthWest, SouthEast };
                Lane next;
                do { next = candidates[urand(0, uint32(candidates.size()-1))]; } while (next == patrol.Target);
                patrol.Target = next;
                patrol.Until = _elapsed + urand(35000, 75000);
            }
            orders[squad] = patrol.Target;
            ++patrols;
        }

        for (uint32 i = 0; i < MaxParticipants; ++i)
        {
            Lane base = ResolveSquadLane(wg, team == wg->GetAttackerTeam(), i);
            if (!IsAssault(base) && base != KeepWest && base != KeepEast)
                continue; // counterattacks retain their tower -> workshop progression
            if (Creature* soldier = map->GetCreature(_roster[team][i].Soldier))
                if (auto* ai = GetBotWarSoldierAI(soldier))
                    ai->SetTacticalLane(orders[i / SquadSize]);
        }
    }

    void UpdateSiegeTactics(Map* map, BattlefieldWG* wg, TeamId team)
    {
        if (team != wg->GetAttackerTeam()) return;
        std::array<Lane,3> lanes={Center,West,East};
        std::array<uint32,3> entries={190375,190371,190372};
        std::array<uint32,3> scores;
        scores.fill(std::numeric_limits<uint32>::max());
        for (uint32 i=0;i<lanes.size();++i)
        {
            if (lanes[i]!=Center && wg->GetData(lanes[i]==West?AREA_THE_BROKEN_TEMPLATE:AREA_THE_SUNKEN_RING)!=uint32(team))
                continue;
            GameObject* wall=wg->GetBotWarBuilding(entries[i]);
            if (!wall) continue;
            uint32 defenders=0;
            for (RosterSlot const& slot:_roster[TeamId(1-team)])
                if (Creature* bot=map->GetCreature(slot.Soldier))
                    if (bot->IsAlive() && bot->GetExactDist2d(wall)<80.f) ++defenders;
            std::list<Player*> players;
            GetPlayerListInGrid(players,wall,80.f);
            for (Player* player:players)
                if (player->IsAlive() && player->GetTeamId()!=team && player->IsPvP()) ++defenders;
            auto const& building=wall->GetGOValue()->Building;
            uint32 health=building.MaxHealth?uint32(100ull*building.Health/building.MaxHealth):100;
            scores[i]=BreachScore(wall->GetDestructibleState()==GO_DESTRUCTIBLE_DESTROYED,health,defenders);
        }
        uint32 best=uint32(std::min_element(scores.begin(),scores.end())-scores.begin());
        if (scores[best]==std::numeric_limits<uint32>::max()) return;
        uint32 previous=uint32(_siegeLane);
        // Hysteresis keeps convoys from oscillating when local defenders move.
        if (previous<3 && scores[previous]!=std::numeric_limits<uint32>::max()
            && scores[best]+25>=scores[previous]) best=previous;
        _siegeLane=lanes[best];
        uint32 convoys=0;
        for (uint32 i=0;i<MaxParticipants && convoys<3;i+=SquadSize)
        {
            RosterSlot const& slot=_roster[team][i];
            Creature* vehicle=map->GetCreature(slot.Vehicle);
            Creature* leader=map->GetCreature(slot.Soldier);
            if (!vehicle || !vehicle->IsAlive() || !leader || !leader->IsAlive()
                || vehicle->GetEntry()==NPC_WINTERGRASP_CATAPULT || vehicle->GetPositionX() < 4950.f || vehicle->GetPositionX()>=5190.f
                || HasBotWarHumanPassenger(vehicle)) continue;
            auto* ai=GetBotWarSoldierAI(leader);
            if (!ai || !IsAssault(ai->GetLane()) || ai->HasWorkshopOrder()) continue;
            if (auto* siege=CAST_AI(npc_wg_bot_vehicle,vehicle->AI()))
            {
                siege->SetAssaultLane(_siegeLane);
                ++convoys;
                for (uint32 j=i;j<i+SquadSize;++j)
                    if (Creature* escort=map->GetCreature(_roster[team][j].Soldier))
                        if (auto* escortAI=GetBotWarSoldierAI(escort))
                            if (!escortAI->HasWorkshopOrder()) escortAI->SetTacticalLane(_siegeLane);
            }
        }
    }

    void Cleanup(Map* map)
    {
        for (auto& team : _roster)
            for (RosterSlot& slot : team)
            {
                if (Creature* vehicle = map->GetCreature(slot.Vehicle))
                    vehicle->DespawnOrUnsummon();
                if (Creature* soldier = map->GetCreature(slot.Soldier))
                    RetireBotWarSoldier(soldier);
                slot = {};
            }
        for (ObjectGuid guid : _claimedVehicles)
            if (Creature* vehicle = map->GetCreature(guid))
                vehicle->DespawnOrUnsummon();
        _claimedVehicles.clear();
        _nextTactics.fill(0);
        _patrols = {};
        _siegeLane=LaneCount;
        _running = false;
    }

    std::array<std::array<RosterSlot, MaxParticipants>, 2> _roster;
    WintergraspVehicleMapUpdater _vehicleMap;
    std::vector<ObjectGuid> _claimedVehicles;
    struct PatrolOrder { Lane Target = LaneCount; uint32 Until = 0; };
    std::array<std::array<PatrolOrder, MaxParticipants / SquadSize>, 2> _patrols{};
    std::array<uint32, 2> _nextTactics{};
    Lane _siegeLane=LaneCount;
    uint32 _timer = 0;
    uint32 _elapsed = 0;
    uint32 _nextReport = 0;
    uint32 _battleSerial = 0;
    TeamId _defender = TEAM_NEUTRAL;
    bool _running = false;
};

void AddSC_wintergrasp()
{
    RegisterCreatureAI(npc_wg_queue);
    RegisterCreatureAI(npc_wg_spirit_guide);
    RegisterCreatureAI(npc_wg_demolisher_engineer);
    RegisterGameObjectAI(go_wg_vehicle_teleporter);
    RegisterSpellScript(spell_wintergrasp_force_building);
    RegisterSpellScript(spell_wintergrasp_grab_passenger);
    RegisterSpellScript(spell_wintergrasp_activate_robotic_arms);
    new achievement_wg_didnt_stand_a_chance();
    RegisterSpellScript(spell_wintergrasp_defender_teleport);
    RegisterSpellScript(spell_wintergrasp_defender_teleport_trigger);
    RegisterSpellScript(spell_wintergrasp_tenacity_refresh);
    new condition_is_wintergrasp_horde();
    new condition_is_wintergrasp_alliance();

    // Wintergrasp Bot War
    RegisterCreatureAI(npc_wg_bot_vehicle);
    RegisterCreatureAI(npc_wg_bot_soldier);
    RegisterCreatureAI(npc_wg_tower_cannon);
    RegisterCreatureAI(npc_wg_bot_siege_turret);
    new WintergraspBotWarMapScript();
    new WintergraspBotWarKillScript();
}
