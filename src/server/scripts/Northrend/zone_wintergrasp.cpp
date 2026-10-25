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
#include "BattlefieldMgr.h"
#include "Battlefield/BattlefieldWG.h"
#include "CellImpl.h"
#include "Config.h"
#include "TemporarySummon.h"
#include "wintergrasp_bot_war.h"
#include <array>
#include <cmath>
#include "DBCStores.h"
#include "GameObject.h"
#include "GameObjectAI.h"
#include "GameTime.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "MapManager.h"
#include "Log.h"
#include "MotionMaster.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "ScriptSystem.h"
#include "SpellAuras.h"
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

struct BotWaypoint
{
    Position Pos;
    uint32 Barrier = 0;
};

// Checkpoints are on the approach side of each obstacle. A missing obstacle is
// treated as closed: unloaded buildings must never let a bot skip the siege.
std::vector<BotWaypoint> const BotRoutes[LaneCount] =
{
    { // Center
        { {4550.f, 2840.f, 385.f, 0.f} }, { {4750.f, 2840.f, 380.f, 0.f} },
        { {4920.f, 2840.f, 375.f, 0.f} }, { {5050.f, 2840.f, 385.f, 0.f} },
        { {5135.f, 2841.f, 405.f, 0.f}, 190375 },
        { {5200.f, 2841.f, 409.f, 0.f} }, { {5250.f, 2841.f, 409.5f, 0.f}, 191805 },
        { {5320.f, 2841.f, 409.5f, 0.f} }, { {5370.f, 2841.f, 425.f, 0.f}, 191810 },
        { {5420.f, 2840.5f, 428.f, 0.f} }, { {5440.38f, 2840.49f, 430.28f, 0.f} }
    },
    { // West: pass through the outer wall, then the inner wall, then vault gate.
        { {4948.524f, 3342.338f, 376.875f, 0.f} }, { {5020.f, 3250.f, 380.f, 0.f} },
        { {5100.f, 3100.f, 385.f, 0.f} }, { {5180.f, 2934.f, 409.f, 0.f}, 190371 },
        { {5235.f, 2934.f, 409.f, 0.f} }, { {5250.f, 2841.f, 409.5f, 0.f}, 191805 },
        { {5320.f, 2841.f, 409.5f, 0.f} }, { {5370.f, 2841.f, 425.f, 0.f}, 191810 },
        { {5420.f, 2840.5f, 428.f, 0.f} }, { {5440.38f, 2840.49f, 430.28f, 0.f} }
    },
    { // East
        { {4949.344f, 2432.586f, 320.177f, 0.f} }, { {5030.f, 2550.f, 360.f, 0.f} },
        { {5110.f, 2680.f, 385.f, 0.f} }, { {5180.f, 2747.f, 409.f, 0.f}, 190372 },
        { {5235.f, 2747.f, 409.f, 0.f} }, { {5250.f, 2841.f, 409.5f, 0.f}, 191805 },
        { {5320.f, 2841.f, 409.5f, 0.f} }, { {5370.f, 2841.f, 425.f, 0.f}, 191810 },
        { {5420.f, 2840.5f, 428.f, 0.f} }, { {5440.38f, 2840.49f, 430.28f, 0.f} }
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
    { { {5200.f, 2841.f, 409.f, 0.f} } }, // KeepCenter
    { { {5225.f, 2934.f, 409.f, 0.f} } }, // KeepWest
    { { {5225.f, 2747.f, 409.f, 0.f} } }, // KeepEast
    { { {4948.524f, 3342.338f, 376.875f, 0.f} } }, // WorkshopWest (relocated to actual flag below)
    { { {4949.344f, 2432.586f, 320.177f, 0.f} } }  // WorkshopEast
};

// Stable formation slots, so bots keep their place rather than jittering randomly.
Position FormationPosition(Creature* bot, Position pos, uint32 slot)
{
    static float const offsets[5][2] = { {0.f, 0.f}, {-4.f, -4.f}, {-7.f, 0.f}, {-4.f, 4.f}, {2.f, 3.f} };
    uint32 squad = slot / SquadSize;
    pos.m_positionX += offsets[slot % SquadSize][0] - float(squad % 3) * 2.f;
    pos.m_positionY += offsets[slot % SquadSize][1] + (int32(squad % 5) - 2) * 2.f;
    bot->UpdateAllowedPositionZ(pos.m_positionX, pos.m_positionY, pos.m_positionZ);
    return pos;
}

struct BotRouteMovement
{
    Lane Route = Center;
    uint32 Point = 0;
    uint32 Slot = 0;
    uint32 RetryTimer = 0;
    bool Assigned = false;
    bool Arrived = false;

    void Assign(Creature* bot, Lane lane, uint32 slot)
    {
        Route = lane;
        Slot = slot;
        Point = 0;
        Assigned = true;
        Arrived = false;
        Move(bot);
    }

    Position Destination(Creature* bot) const
    {
        Position pos = BotRoutes[Route][Point].Pos;
        if (Route == KeepCenter)
            if (BattlefieldWG* wg = GetBotWarBattlefield())
                if (GameObject* innerWall = wg->GetBotWarBuilding(191805))
                    if (innerWall->GetDestructibleState() == GO_DESTRUCTIBLE_DESTROYED)
                        pos = { 5350.f, 2841.f, 415.f, 0.f };
        if (IsWorkshop(Route) || (Point == 0 && (Route == West || Route == East)))
            if (BattlefieldWG* wg = GetBotWarBattlefield())
                if (GameObject* flag = wg->GetBotWarWorkshopFlag(Route == WorkshopWest || Route == West))
                    pos = flag->GetPosition();
        return FormationPosition(bot, pos, Slot);
    }

    uint32 BarrierEntry() const { return BotRoutes[Route][Point].Barrier; }

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
        bot->GetMotionMaster()->MovePoint(Point, Destination(bot), true);
        RetryTimer = 5000;
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
        if (Arrived)
        {
            if (IsCounterAttack(Route) && Point + 1 == BotRoutes[Route].size() && !Blocked(wg))
            {
                Assign(bot, Route == SouthWest ? WorkshopWest : WorkshopEast, Slot);
                return;
            }
            // Even a small roster secures the northern workshop before advancing.
            if (Point == 0 && (Route == West || Route == East))
            {
                uint32 area = Route == West ? AREA_THE_BROKEN_TEMPLATE : AREA_THE_SUNKEN_RING;
                TeamId team = bot->GetFaction() == WGFactionTable[TEAM_ALLIANCE] ? TEAM_ALLIANCE : TEAM_HORDE;
                if (wg->GetData(area) != uint32(team))
                    return;
            }
            Position destination = Destination(bot);
            if (bot->GetExactDist2d(&destination) > 15.f)
            {
                Arrived = false;
                Move(bot);
                return;
            }
            if (Blocked(wg) || Point + 1 >= BotRoutes[Route].size())
                return;
            ++Point;
            Arrived = false;
            Move(bot);
        }
        else if (RetryTimer <= diff)
        {
            RetryTimer = 5000;
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

    void SetRoute(WintergraspBotWar::Lane lane, uint32 slot)
    {
        _route.Assign(me, lane, slot);
    }

    void MovementInform(uint32 type, uint32 id) override { _route.MovementInform(type, id); }

    GameObject* SiegeTarget()
    {
        BattlefieldWG* wg = GetBotWarBattlefield();
        return wg && _route.Assigned && _route.Arrived && _route.Blocked(wg)
            ? wg->GetBotWarBuilding(_route.BarrierEntry()) : nullptr;
    }

    void OnCharmed(bool isNew) override
    {
        // A friendly driver leaving is a handover, not a hostile charm ending.
        UnitAI::OnCharmed(isNew);
        if (isNew && !me->IsCharmed())
        {
            me->LastCharmerGUID.Clear();
            _takeoverTimer = 0;
            _route.Arrived = false;
            _route.Move(me);
        }
    }

    void MoveInLineOfSight(Unit* unit) override
    {
        if (!_route.Assigned || !unit->ToPlayer() || !me->IsFriendlyTo(unit) || !me->IsWithinDistInMap(unit, 8.f))
            return;
        if (Vehicle* kit = me->GetVehicleKit())
            if (Unit* driver = kit->GetPassenger(0))
                if (Creature* bot = driver->ToCreature())
                    if (bot->GetEntry() >= 80501 && bot->GetEntry() <= 80508)
                    {
                        bot->AI()->DoAction(1);
                        bot->ExitVehicle();
                        me->StopMoving();
                        _takeoverTimer = 15000;
                    }
    }

    void PassengerBoarded(Unit* passenger, int8 seat, bool apply) override
    {
        if (seat == 0 && passenger->ToCreature() && apply)
        {
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
                            float d = me->GetExactDist2d(&BotRoutes[l][0].Pos);
                            if (d < bestDist) { bestDist = d; bestLane = l; }
                        }
                    }
                    _route.Assign(me, bestLane, 0);
                }
            }
            if (_route.Assigned)
                _route.Move(me);
        }
        if (seat == 0 && passenger->ToPlayer())
        {
            if (apply)
                me->StopMoving();
            else if (_route.Assigned)
                _route.Move(me);
        }
    }

    void UpdateAI(uint32 diff) override
    {
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
                me->StopMoving();
            return;
        }

        if (me->GetHealthPct() < 15.0f)
        {
            if (Creature* bot = driver->ToCreature(); bot && bot->GetEntry() >= 80501 && bot->GetEntry() <= 80508)
                bot->AI()->DoAction(1);
            driver->ExitVehicle();
            me->StopMoving();
            _takeoverTimer = 15000;
            return;
        }

        if (_thinkTimer > diff)
        {
            _thinkTimer -= diff;
            return;
        }
        uint32 elapsed = 500;
        _thinkTimer = 500;
        _weaponTimer = _weaponTimer > elapsed ? _weaponTimer - elapsed : 0;

        if (_route.Arrived && _route.Blocked(wg) && me->GetEntry() != NPC_WINTERGRASP_CATAPULT)
        {
            if (GameObject* building = wg->GetBotWarBuilding(_route.BarrierEntry()))
            {
                bool defender = me->GetFaction() == WGFactionTable[wg->GetDefenderTeam()];
                if (defender != IsCounterAttack(_route.Route))
                    return;

                bool demolisher = me->GetEntry() == NPC_WINTERGRASP_DEMOLISHER;
                float range = demolisher ? 40.f : 12.f;
                if (me->GetExactDist2d(building) > range)
                {
                    Position approach = building->GetPosition();
                    float angle = building->GetAbsoluteAngle(me);
                    approach.m_positionX += std::cos(angle) * (demolisher ? 30.f : 9.f);
                    approach.m_positionY += std::sin(angle) * (demolisher ? 30.f : 9.f);
                    me->UpdateAllowedPositionZ(approach.m_positionX, approach.m_positionY, approach.m_positionZ);
                    if (!me->isMoving())
                        me->GetMotionMaster()->MovePoint(1000, approach, true);
                    return;
                }

                me->StopMoving();
                me->SetFacingToObject(building);
                if (!_weaponTimer && !me->HasUnitState(UNIT_STATE_CASTING))
                {
                    if (demolisher)
                    {
                        SpellCastResult res = me->CastSpell(building, 50896, false);
                        if (res != SPELL_CAST_OK)
                            me->CastSpell(building->GetPosition(), 50896, false);
                    }
                    else if (me->GetEntry() == NPC_WINTERGRASP_CATAPULT)
                    {
                        SpellCastResult res = me->CastSpell(building, 50989, false);
                        if (res != SPELL_CAST_OK)
                            me->CastSpell(building->GetPosition(), 50989, false);
                    }
                    else
                        DoCastSelf(54109);
                    _weaponTimer = demolisher ? 2500 : 4000;
                }
                return;
            }
        }

        _route.Update(me, wg, elapsed);
        if (!_weaponTimer && !me->HasUnitState(UNIT_STATE_CASTING))
            if (Unit* target = me->SelectNearestTarget(40.f))
            {
                me->SetFacingToObject(target);
                if (me->GetEntry() == NPC_WINTERGRASP_CATAPULT)
                    me->CastSpell(target->GetPosition(), 57606, false); // Plague Barrel, anti-infantry
                else if (me->GetEntry() == NPC_WINTERGRASP_DEMOLISHER)
                    me->CastSpell(target->GetPosition(), 50896, false);
                else if (me->IsWithinDistInMap(target, 12.f))
                    DoCastSelf(54109);
                _weaponTimer = 3000;
            }
    }

private:
    BotRouteMovement _route;
    uint32 _thinkTimer = 500;
    uint32 _weaponTimer = 1000;
    uint32 _takeoverTimer = 0;
};

struct npc_wg_bot_soldier : public ScriptedAI
{
    npc_wg_bot_soldier(Creature* creature) : ScriptedAI(creature)
    {
        // unit_class in creature_template is a stat class (1/2/4/8), not a player specialization.
        _profile = (me->GetEntry() - 80501) % 4;
        _primaryTimer = urand(500, 2000);
    }

    void SetOrders(WintergraspBotWar::Lane lane, uint32 slot, ObjectGuid vehicle = ObjectGuid::Empty)
    {
        _vehicle = vehicle;
        _route.Assign(me, lane, slot);
    }

    void SetEscortVehicle(ObjectGuid vehicle) { _vehicle = vehicle; }

    void DoAction(int32 action) override
    {
        if (action == 1)
            _boardingTimer = 20000;
    }

    void MovementInform(uint32 type, uint32 id) override { _route.MovementInform(type, id); }

    void AttackStart(Unit* target) override
    {
        if (!target || me->GetVehicle() || !_route.Assigned || !me->IsWithinLOSInMap(target))
            return;
        if (BattlefieldWG* wg = GetBotWarBattlefield())
            if (_route.Blocked(wg))
                if (GameObject* barrier = wg->GetBotWarBuilding(_route.BarrierEntry()))
                {
                    float bx = me->GetPositionX() - barrier->GetPositionX();
                    float by = me->GetPositionY() - barrier->GetPositionY();
                    float tx = target->GetPositionX() - barrier->GetPositionX();
                    float ty = target->GetPositionY() - barrier->GetPositionY();
                    if (bx * tx + by * ty < 0.f)
                        return;
                }
        if (!me->IsInCombat())
            _combatOrigin = me->GetPosition();
        me->Dismount();
        me->SetSpeed(MOVE_RUN, 1.f);
        if (_profile == 0)
            ScriptedAI::AttackStart(target);
        else if (me->Attack(target, false))
            me->GetMotionMaster()->MoveChase(target, _profile == 2 ? 28.f : 22.f);
    }

    void EnterEvadeMode(EvadeReason /*why*/) override
    {
        if (!_EnterEvadeMode())
            return;
        me->GetMotionMaster()->Clear();
        _route.Arrived = false;
        _route.Move(me);
    }

    void JustDied(Unit* killer) override
    {
        if (BattlefieldWG* wg = GetBotWarBattlefield())
            if (Player* player = killer ? killer->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr)
                wg->HandlePromotion(player, me);
    }

    void UpdateAI(uint32 diff) override
    {
        BattlefieldWG* wg = GetBotWarBattlefield();
        if (!wg || !_route.Assigned)
            return;
        if (me->GetVehicle())
            return; // Driver/gunner is one roster participant, not an extra summon.

        auto tick = [diff](uint32& timer) { timer = timer > diff ? timer - diff : 0; };
        tick(_primaryTimer);
        tick(_secondaryTimer);
        tick(_defensiveTimer);
        tick(_trinketTimer);
        tick(_thinkTimer);
        tick(_healScanTimer);
        tick(_boardingTimer);
        if (!_trinketTimer && me->HasUnitState(UNIT_STATE_CONTROLLED | UNIT_STATE_ROOT))
        {
            DoCastSelf(42292, true);
            _trinketTimer = 120000;
        }
        if (me->HasUnitState(UNIT_STATE_CASTING | UNIT_STATE_CONTROLLED))
            return;

        // Healers support injured allies even when they have no hostile victim.
        if (_profile == 2 && !_primaryTimer && !_healScanTimer)
        {
            _healScanTimer = 750;
            if (Unit* ally = DoSelectLowestHpFriendly(35.f))
                if (ally->IsAlive() && ally->HealthBelowPct(85) && me->IsWithinLOSInMap(ally))
                {
                    me->Dismount();
                    me->SetSpeed(MOVE_RUN, 1.f);
                    me->StopMoving();
                    DoCast(ally, 48071); // Flash Heal, level 80
                    _primaryTimer = urand(2200, 3200);
                    return;
                }
        }

        if (!_thinkTimer)
        {
            _thinkTimer = urand(700, 1300);
            if (me->IsInCombat() && me->GetExactDist2d(&_combatOrigin) > 100.f)
            {
                EnterEvadeMode(EVADE_REASON_BOUNDARY);
                return;
            }
            if (!me->GetVictim())
                if (Unit* enemy = me->SelectNearestTarget(_profile == 0 ? 30.f : 40.f))
                    AttackStart(enemy);
        }

        bool canCapture = false;
        if (IsAssault(_route.Route) && wg->CanInteractWithRelic()
            && me->GetFaction() == WGFactionTable[wg->GetAttackerTeam()] && !me->IsInCombat())
            if (GameObject* relic = wg->GetRelic())
                canCapture = me->IsWithinDistInMap(relic, 5.f) && me->IsWithinLOSInMap(relic);
        _captureTimer = canCapture ? _captureTimer + diff : 0;
        if (_captureTimer >= 3000)
        {
            wg->EndBattle(false);
            return;
        }

        if (!UpdateVictim())
        {
            // Check for unpiloted friendly vehicles nearby
            if (_vehicle.IsEmpty() && !_boardingTimer && !me->GetVehicle())
            {
                std::list<Creature*> vehList;
                for (uint32 vEntry : { uint32(NPC_WINTERGRASP_DEMOLISHER), uint32(NPC_WINTERGRASP_SIEGE_ENGINE_ALLIANCE), uint32(NPC_WINTERGRASP_SIEGE_ENGINE_HORDE), uint32(NPC_WINTERGRASP_CATAPULT) })
                {
                    me->GetCreatureListWithEntryInGrid(vehList, vEntry, 40.f);
                    for (Creature* v : vehList)
                    {
                        if (v->IsAlive() && v->IsFriendlyTo(me) && v->GetVehicleKit() && v->GetVehicleKit()->HasEmptySeat(0))
                        {
                            _vehicle = v->GetGUID();
                            break;
                        }
                    }
                    if (!_vehicle.IsEmpty())
                        break;
                }
            }

            // Board or escort vehicle
            if (Creature* vehicle = me->GetMap()->GetCreature(_vehicle))
            {
                if (!_boardingTimer && vehicle->IsAlive() && me->IsWithinDistInMap(vehicle, 14.f))
                {
                    _boardingTimer = 2000;
                    if (Vehicle* kit = vehicle->GetVehicleKit())
                    {
                        if (kit->HasEmptySeat(0))
                        {
                            me->Dismount();
                            me->EnterVehicle(vehicle, 0);
                            return;
                        }
                        for (auto const& seat : kit->Seats)
                            if (Unit* accessory = kit->GetPassenger(seat.first))
                                if ((accessory->GetEntry() == 28319 || accessory->GetEntry() == 32629)
                                    && accessory->GetVehicleKit() && accessory->GetVehicleKit()->HasEmptySeat(0))
                                {
                                    me->Dismount();
                                    me->EnterVehicle(accessory, 0);
                                    return;
                                }
                    }
                }
                if (vehicle->IsAlive() && me->GetExactDist2d(vehicle) > 35.f)
                {
                    if (!me->isMoving())
                        me->GetMotionMaster()->MoveFollow(vehicle, 10.f + _route.Slot % 5, float(_route.Slot % 5));
                    return;
                }
            }
            Position destination = _route.Destination(me);
            if (me->GetExactDist2d(&destination) < 25.f && me->IsMounted())
            {
                me->Dismount();
                me->SetSpeed(MOVE_RUN, 1.f);
            }
            if (me->GetExactDist2d(&destination) > 45.f && !me->IsMounted())
            {
                me->Mount(me->GetFaction() == WGFactionTable[TEAM_ALLIANCE] ? 14337 : 14334);
                me->SetSpeed(MOVE_RUN, 2.f);
            }
            _route.Update(me, wg, diff);
            return;
        }

        me->Dismount();
        me->SetSpeed(MOVE_RUN, 1.f);
        Unit* victim = me->GetVictim();
        if (!victim || !me->IsWithinLOSInMap(victim))
            return;

        if (!_defensiveTimer)
        {
            if (_profile == 1 && me->IsWithinDistInMap(victim, 8.f))
            {
                DoCastSelf(42917); // Frost Nova
                _defensiveTimer = 25000;
                return;
            }
            if (_profile == 2 && me->HealthBelowPct(45) && !me->HasAura(6788))
            {
                DoCastSelf(48066); // Power Word: Shield
                _defensiveTimer = 15000;
                return;
            }
        }

        if (!_secondaryTimer)
        {
            if (_profile == 0 && me->IsWithinMeleeRange(victim))
                DoCastVictim(1715); // Hamstring
            else if (_profile == 1 && victim->HasUnitState(UNIT_STATE_CASTING))
                DoCastVictim(2139); // Counterspell
            else if (_profile == 3)
                DoCastVictim(5116); // Concussive Shot
            else if (_profile == 2)
                DoCastVictim(48127); // Mind Blast
            _secondaryTimer = urand(8000, 12000);
            return;
        }

        if (!_primaryTimer)
        {
            if (_profile != 0)
                me->StopMoving();
            switch (_profile)
            {
                case 0:
                    if (me->IsWithinMeleeRange(victim))
                        DoCastVictim(47486); // Mortal Strike (16857 is Faerie Fire!)
                    _primaryTimer = 6000;
                    break;
                case 1: DoCastVictim(42842); _primaryTimer = 3000; break; // Frostbolt
                case 2: DoCastVictim(48123); _primaryTimer = 3000; break; // Smite
                case 3: DoCastVictim(49050); _primaryTimer = 5000; break; // Aimed Shot
            }
        }
        if (_profile == 0)
            DoMeleeAttackIfReady();
    }

private:
    BotRouteMovement _route;
    Position _combatOrigin;
    ObjectGuid _vehicle;
    uint8 _profile;
    uint32 _primaryTimer = 1000;
    uint32 _secondaryTimer = 4000;
    uint32 _defensiveTimer = 0;
    uint32 _trinketTimer = 0;
    uint32 _thinkTimer = 1000;
    uint32 _healScanTimer = 500;
    uint32 _captureTimer = 0;
    uint32 _boardingTimer = 0;
};

struct npc_wg_bot_siege_turret : public ScriptedAI
{
    npc_wg_bot_siege_turret(Creature* creature) : ScriptedAI(creature) { SetCombatMovement(false); }

    void OnCharmed(bool isNew) override
    {
        UnitAI::OnCharmed(isNew);
        if (isNew && !me->IsCharmed())
            me->LastCharmerGUID.Clear();
    }

    void MoveInLineOfSight(Unit* unit) override
    {
        Unit* engine = me->GetVehicleBase();
        if (!engine || !unit->ToPlayer() || !engine->IsFriendlyTo(unit) || !me->IsWithinDistInMap(unit, 8.f))
            return;
        if (Vehicle* kit = me->GetVehicleKit())
            if (Unit* gunner = kit->GetPassenger(0))
                if (Creature* bot = gunner->ToCreature())
                    if (bot->GetEntry() >= 80501 && bot->GetEntry() <= 80508)
                    {
                        bot->AI()->DoAction(1);
                        bot->ExitVehicle();
                    }
    }

    void UpdateAI(uint32 diff) override
    {
        if (!GetBotWarBattlefield() || !me->GetVehicleKit())
            return;
        Unit* gunner = me->GetVehicleKit()->GetPassenger(0);
        if (!gunner || !gunner->ToCreature() || gunner->GetEntry() < 80501 || gunner->GetEntry() > 80508)
            return;
        if (_timer > diff)
        {
            _timer -= diff;
            return;
        }
        _timer = 3500;
        Unit* engine = me->GetVehicleBase();
        if (!engine)
            return;
        me->SetFaction(engine->GetFaction());
        if (Unit* target = engine->ToCreature()->SelectNearestTarget(50.f))
        {
            if (me->IsWithinLOSInMap(target))
                me->CastSpell(target->GetPosition(), 57609, false);
        }
        else if (auto* ai = CAST_AI(npc_wg_bot_vehicle, engine->ToCreature()->AI()))
            if (GameObject* building = ai->SiegeTarget())
                if (me->GetExactDist2d(building) < 50.f)
                    me->CastSpell(building->GetPosition(), 57609, false);
    }

private:
    uint32 _timer = 1500;
};

struct npc_wg_tower_cannon : public ScriptedAI
{
    npc_wg_tower_cannon(Creature* creature) : ScriptedAI(creature) { SetCombatMovement(false); }

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
            me->CastSpell(victim->GetPosition(), 51421, false); // Fire Cannon takes a destination
        }
    }

private:
    uint32 _blastTimer = urand(1000, 3000);
};

// Map callbacks keep summons, movement and GUID lookups on Northrend's update thread.
class WintergraspBotWarMapScript : public WorldMapScript
{
public:
    WintergraspBotWarMapScript() : WorldMapScript("WintergraspBotWarMapScript", 571) { }

    void OnUpdate(Map* map, uint32 diff) override
    {
        BattlefieldWG* wg = GetBotWarBattlefield();
        bool enabled = sConfigMgr->GetBoolDefault("Wintergrasp.BotWar.Enable", false, true);
        if (!wg || !enabled)
        {
            if (_running)
                Cleanup(map);
            return;
        }
        if (!_running)
        {
            _running = true;
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
    };

    void RemoveSlot(Map* map, RosterSlot& slot)
    {
        if (Creature* vehicle = map->GetCreature(slot.Vehicle))
        {
            // Preserve any human passenger, including a gunner in a nested turret.
            if (!HasBotWarHumanPassenger(vehicle))
                vehicle->DespawnOrUnsummon();
            else
                _claimedVehicles.push_back(slot.Vehicle);
        }
        if (Creature* soldier = map->GetCreature(slot.Soldier))
            soldier->DespawnOrUnsummon();
        slot.Soldier.Clear();
        slot.Vehicle.Clear();
        slot.RespawnAt = _elapsed + 30000;
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
            if (!slot.Soldier.IsEmpty() && (!soldier || !soldier->IsAlive()))
            {
                RemoveSlot(map, slot);
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

        for (auto it = _claimedVehicles.begin(); it != _claimedVehicles.end();)
        {
            Creature* vehicle = map->GetCreature(*it);
            if (!vehicle || !HasBotWarHumanPassenger(vehicle))
            {
                if (vehicle)
                    vehicle->DespawnOrUnsummon();
                it = _claimedVehicles.erase(it);
            }
            else
                ++it;
        }
    }

    void SpawnSoldier(Map* map, BattlefieldWG* wg, TeamId team, uint32 index, RosterSlot& slot)
    {
        Lane lane = SquadLane(team == wg->GetAttackerTeam(), index);
        static uint32 const profiles[5] = { 0, 1, 2, 3, 0 };
        uint32 entry = (team == TEAM_ALLIANCE ? 80501 : 80505) + profiles[index % SquadSize];
        Position pos = BotRoutes[lane][0].Pos;
        if (IsWorkshop(lane) || lane == West || lane == East)
            if (GameObject* flag = wg->GetBotWarWorkshopFlag(lane == WorkshopWest || lane == West))
                pos = flag->GetPosition();
        map->LoadGrid(pos.GetPositionX(), pos.GetPositionY());
        Creature* soldier = map->SummonCreature(entry, pos);
        if (!soldier)
        {
            slot.RespawnAt = _elapsed + 30000; // Missing SQL must not cause summon spam.
            return;
        }
        auto* ai = CAST_AI(npc_wg_bot_soldier, soldier->AI());
        if (!ai)
        {
            soldier->DespawnOrUnsummon();
            slot.RespawnAt = _elapsed + 30000;
            return;
        }
        soldier->SetFaction(WGFactionTable[team]);
        soldier->setActive(true);
        soldier->SetPvP(true);
        soldier->SetReactState(REACT_AGGRESSIVE);
        if (profiles[index % SquadSize] == 0)
        {
            soldier->SetPowerType(POWER_RAGE);
            soldier->SetMaxPower(POWER_RAGE, 1000);
            soldier->SetPower(POWER_RAGE, 0);
        }
        else
            soldier->SetPowerType(POWER_MANA);
        static char const* const names[] = { "Frost", "Shadow", "Iron", "Storm", "Blood", "Dawn", "Night", "Stone",
            "Silver", "Fire", "Thunder", "Winter", "Ash", "Dark", "Moon", "Raven", "Steel", "Wild", "Sun", "Ice", "Dusk", "Wolf", "Ember", "Star" };
        static char const* const titles[] = { "blade", "weaver", "warden", "shot", "fang" };
        soldier->SetName(std::string(names[index / SquadSize]) + titles[index % SquadSize]);
        slot.Soldier = soldier->GetGUID();

        ObjectGuid escortVehicle = _roster[team][index - index % SquadSize].Vehicle;
        ai->SetOrders(lane, index, escortVehicle);
    }

    void EnsureVehicle(Map* map, BattlefieldWG* wg, TeamId team, uint32 index, RosterSlot& slot, Creature* soldier)
    {
        if (index % SquadSize != 0 || !soldier || !soldier->IsAlive())
            return;
        if (Creature* vehicle = map->GetCreature(slot.Vehicle))
        {
            if (vehicle->IsAlive())
                return;
            vehicle->DespawnOrUnsummon();
            slot.Vehicle.Clear();
            slot.VehicleRespawnAt = _elapsed + 30000;
        }
        if (slot.VehicleRespawnAt > _elapsed)
            return;
        slot.VehicleRespawnAt = _elapsed + 30000;
        Lane lane = SquadLane(team == wg->GetAttackerTeam(), index);
        // Replacements are built at the squad's supply point, not conjured at the frontline.
        Position pos = BotRoutes[lane][0].Pos;
        uint32 squad = index / SquadSize;
        // A workshop grants four vehicles through the normal BattlefieldWG counter.
        // Use only half the available capacity, leaving room for human builders.
        if (index % SquadSize == 0 && (IsAssault(lane) || IsCounterAttack(lane)))
        {
            uint32 current = wg->GetData(team == TEAM_ALLIANCE ? BATTLEFIELD_WG_DATA_VEHICLE_A : BATTLEFIELD_WG_DATA_VEHICLE_H);
            uint32 maximum = wg->GetData(team == TEAM_ALLIANCE ? BATTLEFIELD_WG_DATA_MAX_VEHICLE_A : BATTLEFIELD_WG_DATA_MAX_VEHICLE_H);
            if (current < maximum / 2)
            {
                map->LoadGrid(pos.GetPositionX(), pos.GetPositionY());
                uint32 vehicleEntry = squad % 3 == 0
                    ? (team == TEAM_ALLIANCE ? NPC_WINTERGRASP_SIEGE_ENGINE_ALLIANCE : NPC_WINTERGRASP_SIEGE_ENGINE_HORDE)
                    : NPC_WINTERGRASP_DEMOLISHER;
                if (Creature* vehicle = soldier->SummonCreature(vehicleEntry, pos, TEMPSUMMON_MANUAL_DESPAWN))
                {
                    if (auto* vehicleAI = CAST_AI(npc_wg_bot_vehicle, vehicle->AI()))
                    {
                        vehicle->setActive(true);
                        vehicleAI->SetRoute(lane, index);
                        slot.Vehicle = vehicle->GetGUID();
                        if (auto* leaderAI = CAST_AI(npc_wg_bot_soldier, soldier->AI()))
                            leaderAI->SetOrders(lane, index, slot.Vehicle);
                    }
                    else
                        vehicle->DespawnOrUnsummon();
                }
            }
        }
        // Update surviving escorts when a replacement vehicle is built.
        for (uint32 i = index; i < std::min(index + SquadSize, MaxParticipants); ++i)
            if (Creature* escort = map->GetCreature(_roster[team][i].Soldier))
                if (auto* escortAI = CAST_AI(npc_wg_bot_soldier, escort->AI()))
                    escortAI->SetEscortVehicle(slot.Vehicle);
    }

    void Cleanup(Map* map)
    {
        for (auto& team : _roster)
            for (RosterSlot& slot : team)
            {
                if (Creature* vehicle = map->GetCreature(slot.Vehicle))
                    vehicle->DespawnOrUnsummon();
                if (Creature* soldier = map->GetCreature(slot.Soldier))
                    soldier->DespawnOrUnsummon();
                slot = {};
            }
        for (ObjectGuid guid : _claimedVehicles)
            if (Creature* vehicle = map->GetCreature(guid))
                vehicle->DespawnOrUnsummon();
        _claimedVehicles.clear();
        _running = false;
    }

    std::array<std::array<RosterSlot, MaxParticipants>, 2> _roster;
    std::vector<ObjectGuid> _claimedVehicles;
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
}
