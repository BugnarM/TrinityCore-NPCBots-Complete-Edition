/*
 * Custom Scripts:
 * 1. The Dark Knight (Nemesis of the Living) - 5-Man Dungeon Boss roaming the open world.
 * 2. Companion Hearthstone - Item to summon or dismiss a random companion bot (Max 1).
 */

#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "Player.h"
#include "Item.h"
#include "Bag.h"
#include "Creature.h"
#include "TemporarySummon.h"
#include "Map.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "SpellInfo.h"
#include "MotionMaster.h"
#include "botwanderful.h"
#include "Containers.h"
#include "botdatamgr.h"
#include "botmgr.h"
#include "bot_ai.h"
#include "botconfig.h"
#include "Log.h"
#include "Chat.h"
#include "WorldSession.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "SpellHistory.h"
#include "SpellPackets.h"
#include "Trainer.h"
#include "DBCStores.h"
#include "Config.h"
#include "ObjectMgr.h"
#include "ObjectAccessor.h"
#include "CompanionPartyPolicy.h"
#include <cmath>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>

enum DarkKnightSpells
{
    SPELL_DEATH_GRIP        = 49576,
    SPELL_CHAINS_OF_ICE     = 45524,
    SPELL_OBLITERATE        = 51425,
    SPELL_DEATH_STRIKE      = 49998,
    SPELL_BLOOD_BOIL        = 48721,
    SPELL_DEATH_AND_DECAY   = 49938,
    SPELL_ANTI_MAGIC_SHELL  = 48707,
    SPELL_ICEBOUND_FORT     = 48792,
    SPELL_FRENZY            = 28747,
    SPELL_RAISE_DEAD        = 46584,
    SPELL_BLOOD_PRESENCE    = 48266,
    // Doom Lord Kazzak signature abilities
    SPELL_SHADOW_BOLT_VOLLEY_HIGH = 32963, // Doom Lord Kazzak (Hellfire Peninsula)
    SPELL_SHADOW_BOLT_VOLLEY_MED  = 21341, // Classic Kazzak
    SPELL_SHADOW_BOLT_VOLLEY_LOW  = 17228, // Low-level scaled Shadow Bolt Volley
    SPELL_THUNDERCLAP_HIGH        = 36706, // Kazzak's Thunderclap
    SPELL_THUNDERCLAP_LOW         = 15588
};

enum DarkKnightEvents
{
    EVENT_DEATH_GRIP = 1,
    EVENT_CHAINS_OF_ICE,
    EVENT_OBLITERATE,
    EVENT_DEATH_STRIKE,
    EVENT_BLOOD_BOIL,
    EVENT_DEATH_AND_DECAY,
    EVENT_ANTI_MAGIC_SHELL,
    EVENT_SHADOW_BOLT_VOLLEY,
    EVENT_THUNDERCLAP
};

enum DarkKnightSounds
{
    SOUND_LK_LAUGH          = 14975
};

enum DarkKnightEntries
{
    NPC_DARK_KNIGHT         = 70900,
    NPC_RISEN_GHOUL         = 26111,
    ITEM_COMPANION_STONE    = 70901
};

struct boss_dark_knight_nemesis : public ScriptedAI
{
    boss_dark_knight_nemesis(Creature* creature) : ScriptedAI(creature), summons(creature)
    {
        Initialize();
    }

    EventMap events;
    SummonList summons;
    WanderNode const* _currentNode;
    uint32 _pauseTimer;
    bool _iceboundUsed;
    bool _frenzyUsed;
    bool _ghoulsSummoned;

    void Initialize()
    {
        _currentNode = nullptr;
        _pauseTimer = 2000;
        _iceboundUsed = false;
        _frenzyUsed = false;
        _ghoulsSummoned = false;
    }

    void Reset() override
    {
        events.Reset();
        summons.DespawnAll();
        Initialize();

        me->SetFaction(14); // Hostile to Horde and Alliance (Monster)
        me->SetSpeed(MOVE_WALK, 1.2f);
        me->SetSpeed(MOVE_RUN, 1.6f);
        me->CastSpell(me, SPELL_BLOOD_PRESENCE, true);
    }

    void JustEngagedWith(Unit* who) override
    {
        _pauseTimer = 0;
        events.Reset();
        _iceboundUsed = false;
        _frenzyUsed = false;
        _ghoulsSummoned = false;

        me->PlayDistanceSound(SOUND_LK_LAUGH);
        std::string targetName = who ? who->GetName() : "mortal";
        me->Yell("The shadows consume the living! Die, " + targetName + "!", LANG_UNIVERSAL);

        if (!me->HasAura(SPELL_BLOOD_PRESENCE))
            me->CastSpell(me, SPELL_BLOOD_PRESENCE, true);

        events.ScheduleEvent(EVENT_DEATH_GRIP, 2s);
        events.ScheduleEvent(EVENT_CHAINS_OF_ICE, 4s);
        events.ScheduleEvent(EVENT_BLOOD_BOIL, 3s);
        events.ScheduleEvent(EVENT_OBLITERATE, 5s);
        events.ScheduleEvent(EVENT_DEATH_STRIKE, 8s);
        events.ScheduleEvent(EVENT_SHADOW_BOLT_VOLLEY, 6s);
        events.ScheduleEvent(EVENT_THUNDERCLAP, 10s);
        events.ScheduleEvent(EVENT_DEATH_AND_DECAY, 14s);
        events.ScheduleEvent(EVENT_ANTI_MAGIC_SHELL, 18s);
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type == POINT_MOTION_TYPE && id == 1)
        {
            _pauseTimer = urand(2000, 5000); // Pause 2-5 seconds at node before riding onward
        }
    }

    void MoveToNextWanderNode()
    {
        if (!me->IsAlive() || me->IsInCombat())
            return;

        if (!_currentNode)
        {
            float minD = 50000.0f;
            WanderNode::DoForAllZoneWPs(me->GetZoneId(), [&](WanderNode const* wp) {
                if (wp->GetMapId() == me->GetMapId() && !wp->GetLinks().empty())
                {
                    float d = me->GetExactDist2d(wp);
                    if (d < minD)
                    {
                        minD = d;
                        _currentNode = wp;
                    }
                }
            });
        }

        if (_currentNode && !_currentNode->GetLinks().empty())
        {
            std::vector<WanderNode const*> validLinks;
            for (auto const& link : _currentNode->GetLinks())
            {
                if (link.wp && link.wp->GetMapId() == me->GetMapId())
                    validLinks.push_back(link.wp);
            }

            if (!validLinks.empty())
            {
                _currentNode = Trinity::Containers::SelectRandomContainerElement(validLinks);
                me->GetMotionMaster()->MovePoint(1, _currentNode->GetPosition());
                return;
            }
        }

        _pauseTimer = 4000;
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
        {
            if (_pauseTimer > 0)
            {
                if (diff >= _pauseTimer)
                {
                    _pauseTimer = 0;
                    MoveToNextWanderNode();
                }
                else
                    _pauseTimer -= diff;
            }
            else if (me->GetMotionMaster()->GetCurrentMovementGeneratorType() == IDLE_MOTION_TYPE)
            {
                _pauseTimer = urand(1000, 3000);
            }
            return;
        }

        events.Update(diff);

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        // Icebound Fortitude at <= 50% HP
        if (!_iceboundUsed && me->HealthBelowPct(50))
        {
            _iceboundUsed = true;
            DoCastSelf(SPELL_ICEBOUND_FORT);
            me->Yell("Cold steel and dark blood... you cannot break me!", LANG_UNIVERSAL);
        }

        // Frenzy at <= 35% HP
        if (!_frenzyUsed && me->HealthBelowPct(35))
        {
            _frenzyUsed = true;
            DoCastSelf(SPELL_FRENZY);
            me->Yell("Taste the fury of the Scourge! Shadows consume you all!", LANG_UNIVERSAL);
            uint32 volleySpell = me->GetLevel() < 45 ? SPELL_SHADOW_BOLT_VOLLEY_LOW : (me->GetLevel() < 70 ? SPELL_SHADOW_BOLT_VOLLEY_MED : SPELL_SHADOW_BOLT_VOLLEY_HIGH);
            DoCastSelf(volleySpell);
        }

        // Summon Ghouls at <= 25% HP
        if (!_ghoulsSummoned && me->HealthBelowPct(25))
        {
            _ghoulsSummoned = true;
            me->Yell("Rise, minions of the grave! Tear them apart!", LANG_UNIVERSAL);
            for (int i = 0; i < 3; ++i)
            {
                Position spawnPos = me->GetNearPosition(3.0f, float(i) * (2.0f * float(M_PI) / 3.0f));
                if (TempSummon* ghoul = me->SummonCreature(NPC_RISEN_GHOUL, spawnPos, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 30s))
                {
                    ghoul->SetLevel(me->GetLevel() > 2 ? me->GetLevel() - 2 : 1);
                    ghoul->UpdateLevelDependantStats();
                    summons.Summon(ghoul);
                }
            }
        }

        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
                case EVENT_DEATH_GRIP:
                {
                    Unit* victim = me->GetVictim();
                    if (victim && me->GetDistance(victim) > 8.0f && me->GetDistance(victim) < 30.0f)
                    {
                        DoCast(victim, SPELL_DEATH_GRIP);
                    }
                    else if (Unit* farTarget = SelectTarget(SelectTargetMethod::Random, 0, 30.0f, true))
                    {
                        if (me->GetDistance(farTarget) > 8.0f)
                            DoCast(farTarget, SPELL_DEATH_GRIP);
                    }
                    events.ScheduleEvent(EVENT_DEATH_GRIP, 15s, 20s);
                    break;
                }
                case EVENT_CHAINS_OF_ICE:
                {
                    if (Unit* victim = me->GetVictim())
                        DoCast(victim, SPELL_CHAINS_OF_ICE);
                    events.ScheduleEvent(EVENT_CHAINS_OF_ICE, 10s, 14s);
                    break;
                }
                case EVENT_BLOOD_BOIL:
                {
                    DoCastSelf(me->GetLevel() >= 75 ? SPELL_BLOOD_BOIL : 48721);
                    events.ScheduleEvent(EVENT_BLOOD_BOIL, 7s, 11s);
                    break;
                }
                case EVENT_OBLITERATE:
                {
                    if (Unit* victim = me->GetVictim())
                        DoCast(victim, me->GetLevel() >= 75 ? SPELL_OBLITERATE : 49020);
                    events.ScheduleEvent(EVENT_OBLITERATE, 6s, 9s);
                    break;
                }
                case EVENT_DEATH_STRIKE:
                {
                    if (Unit* victim = me->GetVictim())
                    {
                        DoCast(victim, SPELL_DEATH_STRIKE);
                        // Heal 8% max HP on death strike hit
                        me->ModifyHealth(int32(me->CountPctFromMaxHealth(8)));
                    }
                    events.ScheduleEvent(EVENT_DEATH_STRIKE, 8s, 12s);
                    break;
                }
                case EVENT_DEATH_AND_DECAY:
                {
                    if (Unit* victim = me->GetVictim())
                        DoCast(victim, me->GetLevel() >= 75 ? SPELL_DEATH_AND_DECAY : 43265);
                    events.ScheduleEvent(EVENT_DEATH_AND_DECAY, 18s, 24s);
                    break;
                }
                case EVENT_ANTI_MAGIC_SHELL:
                {
                    DoCastSelf(SPELL_ANTI_MAGIC_SHELL);
                    events.ScheduleEvent(EVENT_ANTI_MAGIC_SHELL, 25s, 35s);
                    break;
                }
                case EVENT_SHADOW_BOLT_VOLLEY:
                {
                    me->Yell("Shadows of Doom! Flee or be consumed!", LANG_UNIVERSAL);
                    uint32 volleySpell = SPELL_SHADOW_BOLT_VOLLEY_HIGH;
                    if (me->GetLevel() < 45)
                        volleySpell = SPELL_SHADOW_BOLT_VOLLEY_LOW;
                    else if (me->GetLevel() < 70)
                        volleySpell = SPELL_SHADOW_BOLT_VOLLEY_MED;

                    DoCastSelf(volleySpell);
                    events.ScheduleEvent(EVENT_SHADOW_BOLT_VOLLEY, 12s, 18s);
                    break;
                }
                case EVENT_THUNDERCLAP:
                {
                    uint32 clapSpell = me->GetLevel() >= 70 ? SPELL_THUNDERCLAP_HIGH : SPELL_THUNDERCLAP_LOW;
                    DoCastSelf(clapSpell);
                    events.ScheduleEvent(EVENT_THUNDERCLAP, 15s, 22s);
                    break;
                }
            }
        }

        DoMeleeAttackIfReady();
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim)
        {
            // Kazzak Soul Consume: Heals 15% max HP whenever anyone dies to him!
            me->ModifyHealth(int32(me->CountPctFromMaxHealth(15)));
            me->Yell("Another soul swallowed by the abyss! Your death feeds my dark power!", LANG_UNIVERSAL);
            me->PlayDistanceSound(SOUND_LK_LAUGH);
        }
    }

    void JustDied(Unit* /*killer*/) override
    {
        me->Yell("The darkness... will rise... again...", LANG_UNIVERSAL);
        summons.DespawnAll();
    }
};

class boss_dark_knight_nemesis_script : public CreatureScript
{
public:
    boss_dark_knight_nemesis_script() : CreatureScript("boss_dark_knight_nemesis") { }

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new boss_dark_knight_nemesis(creature);
    }
};

/*
 * Companion Hearthstone System (Item 70901) & .companion command:
 * In open world (via Companion Hearthstone or .companion):
 * - Players can have up to 4 companions (forming a balanced 5-player party: 1 Tank, 1 Healer, 2 DPS).
 * - Allocation sequence & rules:
 *     1) Companion #1 MUST be Healer & DPS (Priest, Paladin, Shaman, Druid with BOT_ROLE_HEAL | BOT_ROLE_DPS).
 *     2) Maximum 1 Healer across all companions (no 2 healers allowed).
 *     3) Composition: 1 Tank, 1 Healer, 2 DPS.
 *        - Slot 1: Healer & DPS
 *        - Slot 2: Tank & DPS
 *        - Slot 3: DPS
 *        - Slot 4: DPS
 * - Dismissing:
 *     - Targeting a personal companion: dismisses that specific companion.
 *     - Using .companion dismiss or stone without a target when at max (4) companions: dismisses all companions.
 */
static bool IsPersonalCompanion(Creature const* bot)
{
    return bot && !bot->GetSpawnId() && !bot->IsSummon() && !bot->IsTempBot();
}

static void DismissCompanion(Player* player, Creature* bot)
{
    if (!bot)
        return;
    player->GetBotMgr()->RemoveBot(bot->GetGUID(), BOT_REMOVE_DISMISS);
    bot->DespawnOrUnsummon();
}

static uint32 GetCompanionCooldown(Player const* player, uint64 now, uint32 duration)
{
    if (!duration)
        return 0;
    if (QueryResult result = CharacterDatabase.Query(Trinity::StringFormat(
        "SELECT last_use_time FROM character_companion_stone WHERE guid = {}", player->GetGUID().GetCounter()).c_str()))
        return NPCBots::Companions::CooldownRemaining(now, result->Fetch()[0].GetUInt64(), duration);
    return 0;
}

static void SetCompanionItemCooldown(Player* player, uint32 seconds)
{
    player->GetSpellHistory()->ResetCooldown(55884, true);
    if (!seconds)
        return;
    player->GetSpellHistory()->AddCooldown(55884, ITEM_COMPANION_STONE, std::chrono::seconds(seconds));
    WorldPackets::Spells::SpellCooldown packet;
    packet.Caster = player->GetGUID();
    packet.Flags = 0;
    packet.SpellCooldowns.emplace_back(55884, seconds * 1000);
    player->SendDirectMessage(packet.Write());
}

static bool ToggleCompanionBot(Player* player, uint8 requestedClass = 0, bool dismissAction = false)
{
    using namespace NPCBots::Companions;
    if (!player || !player->IsInWorld() || player->IsBeingTeleported() || !player->GetSession())
        return false;

    // Serialize the shared template selection and its attachment.
    static std::mutex companionInvocationMutex;
    std::lock_guard invocationLock(companionInvocationMutex);
    bool const isGM = player->GetSession()->GetSecurity() >= SEC_GAMEMASTER;
    uint64 const now = GameTime::GetGameTime();
    uint32 const duration = isGM ? 0 : BotCfg::GetHealingSettings().companionCooldownSeconds;
    ChatHandler chat(player->GetSession());
    auto startCooldown = [&] {
        if (!duration)
            return;
        CharacterDatabase.DirectExecute(Trinity::StringFormat(
            "REPLACE INTO character_companion_stone (guid, first_use_done, last_use_time) VALUES ({}, 1, {})",
            player->GetGUID().GetCounter(), now).c_str());
        SetCompanionItemCooldown(player, duration);
    };

    std::vector<ObjectGuid> activeCompanions;
    unsigned healers = 0;
    unsigned tanks = 0;
    std::set<uint8> existingClasses;
    for (auto const& [guid, bot] : *player->GetBotMgr()->GetBotMap())
        if (IsPersonalCompanion(bot))
        {
            activeCompanions.push_back(guid);
            existingClasses.insert(bot->GetBotClass());
            healers += bot->GetBotAI()->HasRole(BOT_ROLE_HEAL);
            tanks += bot->GetBotAI()->HasRole(BOT_ROLE_TANK);
        }

    if (!requestedClass)
        if (Unit* target = player->GetSelectedUnit(); target && target->IsCreature() &&
            player->GetBotMgr()->GetBot(target->GetGUID()) && IsPersonalCompanion(target->ToCreature()))
        {
            chat.PSendSysMessage("Companionul %s a fost concediat.", target->GetName().c_str());
            DismissCompanion(player, target->ToCreature());
            player->CastSpell(player, 64034, true);
            return true;
        }

    if (dismissAction || (!requestedClass && activeCompanions.size() >= MaxCompanions))
    {
        if (activeCompanions.empty())
        {
            chat.SendSysMessage("Nu ai niciun companion activ de concediat.");
            return false;
        }
        for (ObjectGuid guid : activeCompanions)
            DismissCompanion(player, player->GetBotMgr()->GetBot(guid));
        chat.SendSysMessage("Toti companionii au fost concediati.");
        player->CastSpell(player, 64034, true);
        if (!GetCompanionCooldown(player, now, duration))
            startCooldown();
        return true;
    }
    if (activeCompanions.size() >= MaxCompanions)
    {
        chat.SendSysMessage("Ai deja patru companioni. Foloseste .companion dismiss pentru a-i retrage.");
        return false;
    }
    if ((!isGM && player->IsInCombat()) || player->GetMap()->GetInstanceId())
    {
        chat.SendSysMessage("Cheama companionii in afara luptei si inainte de a intra intr-o instanta.");
        return false;
    }
    if (uint32 remaining = GetCompanionCooldown(player, now, duration))
    {
        chat.PSendSysMessage("Piatra este in cooldown pentru inca %u secunde.", remaining);
        SetCompanionItemCooldown(player, remaining);
        return false;
    }

    Role const targetRole = NextRole(healers, tanks);
    bool const canUseDeathKnight = player->GetClass() == CLASS_DEATH_KNIGHT || player->GetLevel() >= 55;
    if (requestedClass)
    {
        if (requestedClass == BOT_CLASS_DEATH_KNIGHT && !canUseDeathKnight)
        {
            chat.SendSysMessage("Companionii Death Knight necesita nivelul 55.");
            return false;
        }
        bool const healing = requestedClass == BOT_CLASS_PRIEST || requestedClass == BOT_CLASS_PALADIN ||
            requestedClass == BOT_CLASS_SHAMAN || requestedClass == BOT_CLASS_DRUID;
        bool const tanking = requestedClass == BOT_CLASS_WARRIOR || requestedClass == BOT_CLASS_PALADIN ||
            requestedClass == BOT_CLASS_DEATH_KNIGHT || requestedClass == BOT_CLASS_DRUID;
        if (!CanFill(targetRole, healing, tanking))
        {
            chat.SendSysMessage(targetRole == Role::Healer ?
                "Urmatorul companion trebuie sa poata vindeca: priest, paladin, shaman sau druid." :
                "Urmatorul companion trebuie sa poata tanka: warrior, paladin, death knight sau druid.");
            return false;
        }
    }

    // 5. Query matching bot templates
    bool isAlliance = (player->GetRaceMask() & RACEMASK_ALLIANCE) != 0;
    std::string raceFilter = isAlliance ? "1, 3, 4, 7, 11" : "2, 5, 6, 8, 10";

    std::string query;
    if (requestedClass != 0)
    {
        query = "SELECT entry FROM creature_template_npcbot_extras WHERE race IN (" + raceFilter +
                ") AND class = " + std::to_string(requestedClass) + " ORDER BY RAND() LIMIT 20";
    }
    else
    {
        if (targetRole == Role::Healer)
        {
            // Healing classes: Paladin (2), Priest (5), Shaman (7), Druid (11)
            query = "SELECT entry FROM creature_template_npcbot_extras WHERE race IN (" + raceFilter +
                    ") AND class IN (2, 5, 7, 11) ORDER BY RAND() LIMIT 20";
        }
        else if (targetRole == Role::Tank)
        {
            // Tank classes: Warrior (1), Paladin (2), Druid (11), DK (6 if lvl >= 55)
            if (canUseDeathKnight)
                query = "SELECT entry FROM creature_template_npcbot_extras WHERE race IN (" + raceFilter +
                        ") AND class IN (1, 2, 6, 11) ORDER BY RAND() LIMIT 20";
            else
                query = "SELECT entry FROM creature_template_npcbot_extras WHERE race IN (" + raceFilter +
                        ") AND class IN (1, 2, 11) ORDER BY RAND() LIMIT 20";
        }
        else // Role::Damage
        {
            // DPS classes: Hunter (3), Rogue (4), Mage (8), Warlock (9)
            query = "SELECT entry FROM creature_template_npcbot_extras WHERE race IN (" + raceFilter +
                    ") AND class IN (3, 4, 8, 9) ORDER BY RAND() LIMIT 20";
        }
    }

    QueryResult result = WorldDatabase.Query(query.c_str());
    if (!result && requestedClass != 0)
    {
        std::string fallbackQuery = "SELECT entry FROM creature_template_npcbot_extras WHERE class = " +
                                    std::to_string(requestedClass) + " ORDER BY RAND() LIMIT 20";
        result = WorldDatabase.Query(fallbackQuery.c_str());
    }

    if (!result)
    {
        ChatHandler(player->GetSession()).SendSysMessage("No companion bot templates found!");
        return false;
    }

    uint32 botEntry = 0;
    do
    {
        uint32 candidate = (*result)[0].GetUInt32();
        bool alreadyOwned = false;
        if (player->GetBotMgr()->GetBotMap())
        {
            for (auto const& [guid, b] : *player->GetBotMgr()->GetBotMap())
            {
                if (b && b->GetEntry() == candidate)
                {
                    alreadyOwned = true;
                    break;
                }
            }
        }
        NpcBotData const* data = BotDataMgr::SelectNpcBotData(candidate);
        if (!BotDataMgr::FindBot(candidate) && !alreadyOwned && (!data || !data->owner))
        {
            // Avoid duplicate class if possible for DPS variety
            NpcBotExtras const* candExtras = BotDataMgr::SelectNpcBotExtras(candidate);
            if (candExtras && existingClasses.contains(candExtras->bclass) && result->GetRowCount() > 1)
            {
                if (!botEntry) botEntry = candidate;
                continue;
            }
            botEntry = candidate;
            break;
        }
    } while (result->NextRow());

    if (!botEntry)
    {
        ChatHandler(player->GetSession()).SendSysMessage("No available companion bots found at this time!");
        return false;
    }

    NpcBotExtras const* extras = BotDataMgr::SelectNpcBotExtras(botEntry);
    if (!extras)
    {
        ChatHandler(player->GetSession()).SendSysMessage("Failed to load companion bot extras!");
        return false;
    }

    uint32 faction = BotDataMgr::GetDefaultFactionForBotRaceClass(extras->bclass, extras->race);
    uint8 spec = BOT_SPEC_DEFAULT;
    uint32 roles = BOT_ROLE_PARTY;

    if (targetRole == Role::Healer)
    {
        switch (extras->bclass)
        {
            case BOT_CLASS_PRIEST:
                spec = (urand(0, 1) == 0) ? BOT_SPEC_PRIEST_HOLY : BOT_SPEC_PRIEST_DISCIPLINE;
                roles |= BOT_ROLE_HEAL | BOT_ROLE_DPS | BOT_ROLE_RANGED;
                break;
            case BOT_CLASS_PALADIN:
                spec = BOT_SPEC_PALADIN_HOLY;
                roles |= BOT_ROLE_HEAL | BOT_ROLE_DPS;
                break;
            case BOT_CLASS_SHAMAN:
                spec = BOT_SPEC_SHAMAN_RESTORATION;
                roles |= BOT_ROLE_HEAL | BOT_ROLE_DPS | BOT_ROLE_RANGED;
                break;
            case BOT_CLASS_DRUID:
                spec = BOT_SPEC_DRUID_RESTORATION;
                roles |= BOT_ROLE_HEAL | BOT_ROLE_DPS | BOT_ROLE_RANGED;
                break;
            default:
                spec = BotDataMgr::SelectSpecForClass(extras->bclass);
                roles |= BOT_ROLE_HEAL | BOT_ROLE_DPS;
                break;
        }
    }
    else if (targetRole == Role::Tank)
    {
        switch (extras->bclass)
        {
            case BOT_CLASS_WARRIOR:
                spec = BOT_SPEC_WARRIOR_PROTECTION;
                roles |= BOT_ROLE_TANK | BOT_ROLE_DPS;
                break;
            case BOT_CLASS_PALADIN:
                spec = BOT_SPEC_PALADIN_PROTECTION;
                roles |= BOT_ROLE_TANK | BOT_ROLE_DPS;
                break;
            case BOT_CLASS_DEATH_KNIGHT:
                spec = BOT_SPEC_DK_BLOOD;
                roles |= BOT_ROLE_TANK | BOT_ROLE_DPS;
                break;
            case BOT_CLASS_DRUID:
                spec = BOT_SPEC_DRUID_FERAL;
                roles |= BOT_ROLE_TANK | BOT_ROLE_DPS;
                break;
            default:
                spec = BotDataMgr::SelectSpecForClass(extras->bclass);
                roles |= BOT_ROLE_TANK | BOT_ROLE_DPS;
                break;
        }
    }
    else // Role::Damage
    {
        switch (extras->bclass)
        {
            case BOT_CLASS_MAGE:
                spec = BOT_SPEC_MAGE_FROST;
                roles |= BOT_ROLE_DPS | BOT_ROLE_RANGED;
                break;
            case BOT_CLASS_WARLOCK:
                spec = BOT_SPEC_WARLOCK_AFFLICTION;
                roles |= BOT_ROLE_DPS | BOT_ROLE_RANGED;
                break;
            case BOT_CLASS_HUNTER:
                spec = BOT_SPEC_HUNTER_MARKSMANSHIP;
                roles |= BOT_ROLE_DPS | BOT_ROLE_RANGED;
                break;
            case BOT_CLASS_ROGUE:
                spec = BOT_SPEC_ROGUE_COMBAT;
                roles |= BOT_ROLE_DPS;
                break;
            case BOT_CLASS_WARRIOR:
                spec = BOT_SPEC_WARRIOR_FURY;
                roles |= BOT_ROLE_DPS;
                break;
            case BOT_CLASS_PALADIN:
                spec = BOT_SPEC_PALADIN_RETRIBUTION;
                roles |= BOT_ROLE_DPS;
                break;
            case BOT_CLASS_PRIEST:
                spec = BOT_SPEC_PRIEST_SHADOW;
                roles |= BOT_ROLE_DPS | BOT_ROLE_RANGED;
                break;
            case BOT_CLASS_DEATH_KNIGHT:
                spec = BOT_SPEC_DK_FROST;
                roles |= BOT_ROLE_DPS;
                break;
            case BOT_CLASS_SHAMAN:
                spec = BOT_SPEC_SHAMAN_ELEMENTAL;
                roles |= BOT_ROLE_DPS | BOT_ROLE_RANGED;
                break;
            case BOT_CLASS_DRUID:
                spec = BOT_SPEC_DRUID_BALANCE;
                roles |= BOT_ROLE_DPS | BOT_ROLE_RANGED;
                break;
            default:
                spec = BotDataMgr::SelectSpecForClass(extras->bclass);
                roles |= BOT_ROLE_DPS;
                break;
        }
    }

    NpcBotData const* oldData = BotDataMgr::SelectNpcBotData(botEntry);
    uint8 oldSpec = oldData ? oldData->spec : spec;
    uint32 oldRoles = oldData ? oldData->roles : roles;
    auto restoreTemplate = [&] {
        if (oldData)
        {
            BotDataMgr::UpdateNpcBotData(botEntry, NPCBOT_UPDATE_SPEC, &oldSpec);
            BotDataMgr::UpdateNpcBotData(botEntry, NPCBOT_UPDATE_ROLES, &oldRoles);
        }
    };
    if (!oldData)
        BotDataMgr::AddNpcBotData(botEntry, roles, spec, faction);
    else
    {
        BotDataMgr::UpdateNpcBotData(botEntry, NPCBOT_UPDATE_SPEC, &spec);
        BotDataMgr::UpdateNpcBotData(botEntry, NPCBOT_UPDATE_ROLES, &roles);
    }

    Position pos = player->GetPosition();
    Creature* bot = new Creature();
    if (!bot->LoadBotCreatureFromDB(0, player->GetMap(), true, true, botEntry, &pos))
    {
        delete bot;
        restoreTemplate();
        ChatHandler(player->GetSession()).SendSysMessage("Failed to spawn companion bot!");
        return false;
    }

    bot->SetPhaseMask(player->GetPhaseMask(), true);

    if (player->GetBotMgr()->AddBot(bot, true) == BOT_ADD_SUCCESS)
    {
        bot->GetBotAI()->SetWanderer(false);
        bot->SetNpcFlag(UNIT_NPC_FLAG_GOSSIP);
        if (!player->GetBotMgr()->AddBotToGroup(bot))
        {
            DismissCompanion(player, bot);
            restoreTemplate();
            chat.SendSysMessage("Companionul nu a putut intra in grup.");
            return false;
        }

        // Explicitly enforce roles on the spawned bot
        if (targetRole == Role::Healer)
        {
            if (!bot->GetBotAI()->HasRole(BOT_ROLE_HEAL))
                bot->GetBotAI()->ToggleRole(BOT_ROLE_HEAL, true);
            if (!bot->GetBotAI()->HasRole(BOT_ROLE_DPS))
                bot->GetBotAI()->ToggleRole(BOT_ROLE_DPS, true);
            if (bot->GetBotAI()->HasRole(BOT_ROLE_TANK))
                bot->GetBotAI()->ToggleRole(BOT_ROLE_TANK, true);
        }
        else if (targetRole == Role::Tank)
        {
            if (!bot->GetBotAI()->HasRole(BOT_ROLE_TANK))
                bot->GetBotAI()->ToggleRole(BOT_ROLE_TANK, true);
            if (!bot->GetBotAI()->HasRole(BOT_ROLE_DPS))
                bot->GetBotAI()->ToggleRole(BOT_ROLE_DPS, true);
            if (bot->GetBotAI()->HasRole(BOT_ROLE_HEAL))
                bot->GetBotAI()->ToggleRole(BOT_ROLE_HEAL, true);
        }
        else // Role::Damage
        {
            if (!bot->GetBotAI()->HasRole(BOT_ROLE_DPS))
                bot->GetBotAI()->ToggleRole(BOT_ROLE_DPS, true);
            if (bot->GetBotAI()->HasRole(BOT_ROLE_HEAL))
                bot->GetBotAI()->ToggleRole(BOT_ROLE_HEAL, true);
            if (bot->GetBotAI()->HasRole(BOT_ROLE_TANK))
                bot->GetBotAI()->ToggleRole(BOT_ROLE_TANK, true);
        }
        if (!bot->GetBotAI()->HasRole(BOT_ROLE_PARTY))
            bot->GetBotAI()->ToggleRole(BOT_ROLE_PARTY, true);

        player->CastSpell(player, 64036, true); // Visual light effect

        uint32 const newCount = uint32(activeCompanions.size() + 1);
        char const* roleLabel = (targetRole == Role::Healer) ? "Healer & DPS" :
                                (targetRole == Role::Tank)   ? "Tank" : "DPS";

        ChatHandler(player->GetSession()).PSendSysMessage(
            "|cff00ff00[Companion Hearthstone]|r Companion #%u: %s (%s) a sosit! (%u/%u companioni)",
            newCount, bot->GetName().c_str(), roleLabel, newCount, uint32(MaxCompanions));

        if (newCount == MaxCompanions)
        {
            ChatHandler(player->GetSession()).SendSysMessage(
                "|cff00ccff[Companion Hearthstone]|r Ai format grupul complet de 4 companioni: 1 Healer, 1 Tank, 2 DPS!");
        }

        if (newCount == MaxCompanions && duration)
            startCooldown();
        else
            SetCompanionItemCooldown(player, 0);
        return true;
    }
    else
    {
        bot->DespawnOrUnsummon();
        restoreTemplate();
        ChatHandler(player->GetSession()).SendSysMessage("Could not attach companion bot!");
        return false;
    }
}

class item_companion_stone : public ItemScript
{
public:
    item_companion_stone() : ItemScript("item_companion_stone") { }

    bool OnUse(Player* player, Item* /*item*/, SpellCastTargets const& /*targets*/) override
    {
        ToggleCompanionBot(player);
        return true;
    }
};

class item_junk_scrapper : public ItemScript
{
public:
    item_junk_scrapper() : ItemScript("item_junk_scrapper") { }

    bool OnUse(Player* player, Item* /*item*/, SpellCastTargets const& /*targets*/) override
    {
        uint32 totalCopper = 0;
        uint32 itemsSold = 0;

        for (uint8 slot = INVENTORY_SLOT_ITEM_START; slot < INVENTORY_SLOT_ITEM_END; ++slot)
        {
            Item* pItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
            if (!pItem)
                continue;
            ItemTemplate const* pProto = pItem->GetTemplate();
            if (pProto && pProto->Quality == ITEM_QUALITY_POOR && pProto->SellPrice > 0 && !pItem->IsRefundable())
            {
                uint32 const count = pItem->GetCount();
                uint32 const price = pItem->GetSellPrice(true) * count;
                totalCopper += price;
                itemsSold += count;

                player->DestroyItem(INVENTORY_SLOT_BAG_0, slot, true);
            }
        }

        for (uint8 bagSlot = INVENTORY_SLOT_BAG_START; bagSlot < INVENTORY_SLOT_BAG_END; ++bagSlot)
        {
            Bag* bag = player->GetBagByPos(bagSlot);
            if (!bag)
                continue;

            for (uint32 slot = 0; slot < bag->GetBagSize(); ++slot)
            {
                Item* pItem = bag->GetItemByPos(slot);
                if (!pItem)
                    continue;
                ItemTemplate const* pProto = pItem->GetTemplate();
                if (pProto && pProto->Quality == ITEM_QUALITY_POOR && pProto->SellPrice > 0 && !pItem->IsRefundable())
                {
                    uint32 const count = pItem->GetCount();
                    uint32 const price = pItem->GetSellPrice(true) * count;
                    totalCopper += price;
                    itemsSold += count;

                    player->DestroyItem(bagSlot, slot, true);
                }
            }
        }

        if (itemsSold > 0)
        {
            player->ModifyMoney(totalCopper);
            player->PlayDirectSound(120, player);
            uint32 const gold = totalCopper / 10000;
            uint32 const silver = (totalCopper % 10000) / 100;
            uint32 const copper = totalCopper % 100;

            ChatHandler(player->GetSession()).SendSysMessage(
                Trinity::StringFormat("|cff00ff00[Junk Scrapper]|r Sold {} grey item(s) for {}g {}s {}c.",
                    itemsSold, gold, silver, copper));
        }
        else
        {
            ChatHandler(player->GetSession()).SendSysMessage("|cffffcc00[Junk Scrapper]|r You have no grey items to scrap in your bags.");
        }

        return true;
    }
};

namespace
{
std::mutex heirloomDeliveryMutex;
AsyncCallbackProcessor<TransactionCallback> heirloomDeliveryCallbacks;
std::unordered_set<uint32> pendingHeirloomDeliveries;
std::unordered_set<uint32> failedHeirloomDeliveries;

bool HeirloomDeliveryUnavailable(uint32 guid)
{
    std::lock_guard lock(heirloomDeliveryMutex);
    return pendingHeirloomDeliveries.contains(guid) || failedHeirloomDeliveries.contains(guid);
}

void QueueHeirloomDelivery(ObjectGuid guid, CharacterDatabaseTransaction transaction, bool complete, bool announce)
{
    std::lock_guard lock(heirloomDeliveryMutex);
    pendingHeirloomDeliveries.insert(guid.GetCounter());
    heirloomDeliveryCallbacks.AddCallback(CharacterDatabase.AsyncCommitTransaction(std::move(transaction))).AfterComplete(
        [guid, complete, announce](bool success)
        {
            pendingHeirloomDeliveries.erase(guid.GetCounter());
            Player* player = ObjectAccessor::FindConnectedPlayer(guid);
            if (!success)
            {
                TC_LOG_ERROR("scripts", "Heirloom delivery could not be saved for {}", guid.ToString());
                if (player && player->GetSession())
                {
                    failedHeirloomDeliveries.insert(guid.GetCounter());
                    ChatHandler(player->GetSession()).SendSysMessage("Heirloom delivery could not be saved. Reconnect before retrying.");
                }
                return;
            }
            if (announce && player && player->GetSession())
                ChatHandler(player->GetSession()).SendSysMessage(complete ? "Heirloom package delivered." :
                    "Partial heirloom delivery saved. Free bag space and reconnect to receive the remaining items.");
        });
}

class companion_delivery_worldscript : public WorldScript
{
public:
    companion_delivery_worldscript() : WorldScript("companion_delivery_worldscript") { }

    void OnUpdate(uint32 /*diff*/) override
    {
        std::lock_guard lock(heirloomDeliveryMutex);
        heirloomDeliveryCallbacks.ProcessReadyCallbacks();
    }
};
}

static bool GrantClassHeirlooms(Player* player, bool announce = true)
{
    if (!player || !BotCfg::GetHealingSettings().heirlooms || player->IsBeingTeleported())
        return false;
    uint32 const guid = player->GetGUID().GetCounter();
    if (HeirloomDeliveryUnavailable(guid))
        return false;
    if (CharacterDatabase.Query(Trinity::StringFormat("SELECT 1 FROM character_heirlooms_received WHERE guid = {}", guid).c_str()))
        return true;
    std::unordered_map<uint32, uint32> delivered;
    if (QueryResult result = CharacterDatabase.Query(Trinity::StringFormat("SELECT item, count FROM character_heirloom_delivery WHERE guid = {}", guid).c_str()))
        do { delivered[(*result)[0].GetUInt32()] = (*result)[1].GetUInt32(); } while (result->NextRow());
    bool complete = true;

    // 2. Ensure weapon proficiencies for the player's class
    std::vector<uint32> weaponSpells;
    switch (player->GetClass())
    {
        case CLASS_WARRIOR:
            weaponSpells = { 196, 197, 198, 199, 201, 202, 227, 264, 266, 1180, 5011, 200, 15590 };
            break;
        case CLASS_PALADIN:
            weaponSpells = { 196, 197, 198, 199, 201, 202, 200 };
            break;
        case CLASS_HUNTER:
            weaponSpells = { 196, 197, 201, 202, 227, 264, 266, 1180, 5011, 200, 15590 };
            break;
        case CLASS_ROGUE:
            weaponSpells = { 196, 198, 201, 264, 266, 1180, 5011, 15590 };
            break;
        case CLASS_PRIEST:
            weaponSpells = { 198, 227, 1180, 5009 };
            break;
        case CLASS_DEATH_KNIGHT:
            weaponSpells = { 196, 197, 198, 199, 201, 202, 200 };
            break;
        case CLASS_SHAMAN:
            weaponSpells = { 196, 197, 198, 199, 227, 1180, 15590 };
            break;
        case CLASS_MAGE:
            weaponSpells = { 201, 227, 1180, 5009 };
            break;
        case CLASS_WARLOCK:
            weaponSpells = { 201, 227, 1180, 5009 };
            break;
        case CLASS_DRUID:
            weaponSpells = { 198, 199, 200, 227, 1180, 15590 };
            break;
        default:
            break;
    }

    for (uint32 spellId : weaponSpells)
    {
        if (!player->HasSpell(spellId))
            player->LearnSpell(spellId, false);
    }

    // 3. Collect matching heirlooms
    std::vector<uint32> heirloomItems{41600, 41600, 41600, 41600};

    // Rings (Dread Pirate Ring x2: +5% XP each)
    heirloomItems.push_back(50255);
    heirloomItems.push_back(50255);

    // Faction PvP Insignia
    uint32 insignia = (player->GetTeamId() == TEAM_ALLIANCE) ? 44098 : 44097;
    heirloomItems.push_back(insignia);

    // Class specific heirlooms
    switch (player->GetClass())
    {
        case CLASS_WARRIOR:
            // Shoulders & Chest (+10% XP each)
            heirloomItems.push_back(42949); // Polished Spaulders of Valor (Plate DPS)
            heirloomItems.push_back(44099); // Strengthened Stockade Pauldrons (Plate Tank)
            heirloomItems.push_back(48685); // Polished Breastplate of Valor (Plate Chest)
            // Trinkets
            heirloomItems.push_back(42991); // Swift Hand of Justice x2
            heirloomItems.push_back(42991);
            // Weapons
            heirloomItems.push_back(42943); // Bloodied Arcanite Reaper (2H Axe)
            heirloomItems.push_back(44092); // Reforged Truesilver Champion (2H Sword)
            heirloomItems.push_back(42945); // Venerable Dal'Rend's Sacred Charge (1H Sword MH)
            heirloomItems.push_back(44096); // Battleworn Thrash Blade (1H Sword OH)
            heirloomItems.push_back(42946); // Charmed Ancient Bone Bow (Ranged)
            break;

        case CLASS_PALADIN:
            // Shoulders & Chest
            heirloomItems.push_back(42949); // Polished Spaulders of Valor (DPS/Prot)
            heirloomItems.push_back(44100); // Pristine Lightforge Spaulders (Holy)
            heirloomItems.push_back(48685); // Polished Breastplate of Valor
            // Trinkets
            heirloomItems.push_back(42991); // Swift Hand of Justice (Melee)
            heirloomItems.push_back(42992); // Discerning Eye of the Beast (Holy)
            // Weapons
            heirloomItems.push_back(42943); // Bloodied Arcanite Reaper (2H Axe)
            heirloomItems.push_back(44092); // Reforged Truesilver Champion (2H Sword)
            heirloomItems.push_back(42948); // Devout Aurastone Hammer (1H Caster Mace)
            heirloomItems.push_back(44096); // Battleworn Thrash Blade (1H Sword)
            break;

        case CLASS_HUNTER:
            // Shoulders & Chest
            heirloomItems.push_back(42950); // Champion Herod's Shoulder
            heirloomItems.push_back(48677); // Champion's Deathdealer Breastplate
            // Trinkets
            heirloomItems.push_back(42991); // Swift Hand of Justice x2
            heirloomItems.push_back(42991);
            // Weapons
            heirloomItems.push_back(42946); // Charmed Ancient Bone Bow
            heirloomItems.push_back(44093); // Upgraded Dwarven Hand Cannon (Gun)
            heirloomItems.push_back(42944); // Balanced Heartseeker (Dagger)
            heirloomItems.push_back(42945); // Venerable Dal'Rend's Sacred Charge (1H Sword)
            heirloomItems.push_back(42943); // Bloodied Arcanite Reaper (2H Stat stick)
            break;

        case CLASS_ROGUE:
            // Shoulders & Chest
            heirloomItems.push_back(42952); // Stained Shadowcraft Spaulders
            heirloomItems.push_back(48689); // Stained Shadowcraft Tunic
            // Trinkets
            heirloomItems.push_back(42991); // Swift Hand of Justice x2
            heirloomItems.push_back(42991);
            // Weapons
            heirloomItems.push_back(42944); // Balanced Heartseeker (Dagger MH)
            heirloomItems.push_back(42944); // Balanced Heartseeker (Dagger OH)
            heirloomItems.push_back(44096); // Battleworn Thrash Blade (1H Sword)
            heirloomItems.push_back(42945); // Venerable Dal'Rend's Sacred Charge (1H Sword)
            heirloomItems.push_back(48716); // Venerable Mass of McGowan (1H Mace)
            heirloomItems.push_back(42946); // Charmed Ancient Bone Bow
            break;

        case CLASS_PRIEST:
            // Shoulders & Chest
            heirloomItems.push_back(42985); // Tattered Dreadmist Mantle
            heirloomItems.push_back(48691); // Tattered Dreadmist Robe
            // Trinkets
            heirloomItems.push_back(42992); // Discerning Eye of the Beast x2
            heirloomItems.push_back(42992);
            // Weapons
            heirloomItems.push_back(42947); // Dignified Headmaster's Charge (Staff)
            heirloomItems.push_back(42948); // Devout Aurastone Hammer (1H Caster Mace)
            heirloomItems.push_back(42944); // Balanced Heartseeker (Dagger)
            break;

        case CLASS_DEATH_KNIGHT:
            // Shoulders & Chest
            heirloomItems.push_back(42949); // Polished Spaulders of Valor
            heirloomItems.push_back(44099); // Strengthened Stockade Pauldrons (Tank)
            heirloomItems.push_back(48685); // Polished Breastplate of Valor
            // Trinkets
            heirloomItems.push_back(42991); // Swift Hand of Justice x2
            heirloomItems.push_back(42991);
            // Weapons
            heirloomItems.push_back(42943); // Bloodied Arcanite Reaper (2H Axe)
            heirloomItems.push_back(44092); // Reforged Truesilver Champion (2H Sword)
            heirloomItems.push_back(42945); // Venerable Dal'Rend's Sacred Charge (1H Sword MH)
            heirloomItems.push_back(44096); // Battleworn Thrash Blade (1H Sword OH)
            break;

        case CLASS_SHAMAN:
            // Shoulders & Chest
            heirloomItems.push_back(42950); // Champion Herod's Shoulder (Enhancement)
            heirloomItems.push_back(42951); // Mystical Pauldrons of Elements (Ele/Resto)
            heirloomItems.push_back(48677); // Champion's Deathdealer Breastplate (Enhancement)
            heirloomItems.push_back(48683); // Mystical Vest of Elements (Ele/Resto)
            // Trinkets
            heirloomItems.push_back(42991); // Swift Hand of Justice (Enhancement)
            heirloomItems.push_back(42992); // Discerning Eye of the Beast (Ele/Resto)
            // Weapons
            heirloomItems.push_back(48716); // Venerable Mass of McGowan (1H Mace MH)
            heirloomItems.push_back(48716); // Venerable Mass of McGowan (1H Mace OH)
            heirloomItems.push_back(42948); // Devout Aurastone Hammer (1H Caster Mace)
            heirloomItems.push_back(48718); // Repurposed Lava Dredger (2H Mace)
            heirloomItems.push_back(42947); // Dignified Headmaster's Charge (Staff)
            break;

        case CLASS_MAGE:
            // Shoulders & Chest
            heirloomItems.push_back(42985); // Tattered Dreadmist Mantle
            heirloomItems.push_back(48691); // Tattered Dreadmist Robe
            // Trinkets
            heirloomItems.push_back(42992); // Discerning Eye of the Beast x2
            heirloomItems.push_back(42992);
            // Weapons
            heirloomItems.push_back(42947); // Dignified Headmaster's Charge (Staff)
            heirloomItems.push_back(44095); // Grand Staff of Jordan (PvP Staff)
            heirloomItems.push_back(42944); // Balanced Heartseeker (Dagger)
            heirloomItems.push_back(44096); // Battleworn Thrash Blade (1H Sword)
            break;

        case CLASS_WARLOCK:
            // Shoulders & Chest
            heirloomItems.push_back(42985); // Tattered Dreadmist Mantle
            heirloomItems.push_back(48691); // Tattered Dreadmist Robe
            // Trinkets
            heirloomItems.push_back(42992); // Discerning Eye of the Beast x2
            heirloomItems.push_back(42992);
            // Weapons
            heirloomItems.push_back(42947); // Dignified Headmaster's Charge (Staff)
            heirloomItems.push_back(44095); // Grand Staff of Jordan (PvP Staff)
            heirloomItems.push_back(42944); // Balanced Heartseeker (Dagger)
            heirloomItems.push_back(44096); // Battleworn Thrash Blade (1H Sword)
            break;

        case CLASS_DRUID:
            // Shoulders & Chest
            heirloomItems.push_back(42952); // Stained Shadowcraft Spaulders (Feral)
            heirloomItems.push_back(42984); // Preened Ironfeather Shoulders (Balance/Resto)
            heirloomItems.push_back(48689); // Stained Shadowcraft Tunic (Feral)
            heirloomItems.push_back(48687); // Preened Ironfeather Breastplate (Balance/Resto)
            // Trinkets
            heirloomItems.push_back(42991); // Swift Hand of Justice (Feral)
            heirloomItems.push_back(42992); // Discerning Eye of the Beast (Balance/Resto)
            // Weapons
            heirloomItems.push_back(48718); // Repurposed Lava Dredger (2H Mace)
            heirloomItems.push_back(42947); // Dignified Headmaster's Charge (Staff)
            heirloomItems.push_back(48716); // Venerable Mass of McGowan (1H Mace)
            heirloomItems.push_back(42948); // Devout Aurastone Hammer (1H Caster Mace)
            break;

        default:
            break;
    }

    // Tome of Cold Weather Flight for when they reach 68
    heirloomItems.push_back(49177);

    // 4. Distribute items
    uint32 grantedCount = 0;
    std::unordered_map<uint32, uint32> countsAdded;

    for (uint32 itemId : heirloomItems)
    {
        countsAdded[itemId]++;
        if (delivered[itemId] >= countsAdded[itemId])
            continue; // A delivered item remains delivered after it is sold or moved.
        if (player->GetItemCount(itemId, true) >= countsAdded[itemId])
        {
            delivered[itemId] = countsAdded[itemId];
            continue;
        }
        bool itemDelivered = false;

        // Try equip into empty equipment slot first
        uint16 eDest;
        if (player->CanEquipNewItem(NULL_SLOT, eDest, itemId, false) == EQUIP_ERR_OK)
        {
            Item* equipped = player->EquipNewItem(eDest, itemId, true);
            if (equipped)
            {
                player->AutoUnequipOffhandIfNeed();
                player->SendNewItem(equipped, 1, true, false);
                grantedCount++;
                delivered[itemId] = countsAdded[itemId];
                continue;
            }
        }

        // Otherwise store in bags (NULL_BAG scans all equipped bags)
        ItemPosCountVec sDest;
        if (player->CanStoreNewItem(NULL_BAG, NULL_SLOT, sDest, itemId, 1) == EQUIP_ERR_OK)
        {
            Item* item = player->StoreNewItem(sDest, itemId, true);
            if (item)
            {
                player->SendNewItem(item, 1, true, false);
                grantedCount++;
                itemDelivered = true;
                delivered[itemId] = countsAdded[itemId];
            }
        }
        if (!itemDelivered)
            complete = false;
    }
    CharacterDatabaseTransaction transaction = CharacterDatabase.BeginTransaction();
    player->SaveToDB(transaction);
    for (auto const& [item, count] : delivered)
        transaction->PAppend("REPLACE INTO character_heirloom_delivery (guid, item, count) VALUES ({}, {}, {})", guid, item, count);
    if (complete)
        transaction->PAppend("INSERT IGNORE INTO character_heirlooms_received (guid) VALUES ({})", guid);
    QueueHeirloomDelivery(player->GetGUID(), std::move(transaction), complete, announce && grantedCount > 0);
    return complete;
}

class companion_playerscript : public PlayerScript
{
public:
    companion_playerscript() : PlayerScript("companion_playerscript") { }

    void OnLogout(Player* player) override
    {
        std::lock_guard lock(heirloomDeliveryMutex);
        failedHeirloomDeliveries.erase(player->GetGUID().GetCounter());
    }

    void OnLogin(Player* player, bool /*firstLogin*/) override
    {
        if (!player)
            return;

        GrantClassHeirlooms(player, true);

        // 2. Give Companion Hearthstone to existing characters if missing
        if (!player->HasItemCount(ITEM_COMPANION_STONE, 1))
        {
            ItemPosCountVec dest;
            InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, ITEM_COMPANION_STONE, 1);
            if (msg == EQUIP_ERR_OK)
            {
                Item* item = player->StoreNewItem(dest, ITEM_COMPANION_STONE, true);
                player->SendNewItem(item, 1, true, false);
                ChatHandler(player->GetSession()).SendSysMessage("|cff00ccff[Companion]|r Ai primit |cffffffff|Hitem:70901:0:0:0:0:0:0:0:0|h[Companion Hearthstone]|h|r!");
            }
        }

        if (player->GetSession()->GetSecurity() < SEC_GAMEMASTER)
            if (uint32 remaining = GetCompanionCooldown(player, GameTime::GetGameTime(), BotCfg::GetHealingSettings().companionCooldownSeconds))
                SetCompanionItemCooldown(player, remaining);
    }
};

using namespace Trinity::ChatCommands;

class companion_commandscript : public CommandScript
{
public:
    companion_commandscript() : CommandScript("companion_commandscript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable commandTable =
        {
            { "companion", HandleCompanionCommand, rbac::RBAC_PERM_COMMAND_NPCBOT_INFO, Console::No },
            { "heirlooms", HandleHeirloomsCommand, rbac::RBAC_PERM_COMMAND_NPCBOT_INFO, Console::No },
            { "heirloom",  HandleHeirloomsCommand, rbac::RBAC_PERM_COMMAND_NPCBOT_INFO, Console::No }
        };
        return commandTable;
    }

    static bool HandleHeirloomsCommand(ChatHandler* handler, Optional<PlayerIdentifier> playerTarget)
    {
        if (!playerTarget)
            playerTarget = PlayerIdentifier::FromTargetOrSelf(handler);

        if (!playerTarget)
        {
            handler->SendSysMessage("Player not found.");
            return false;
        }

        Player* player = playerTarget->GetConnectedPlayer();
        if (!player)
        {
            handler->SendSysMessage("Player not found.");
            return false;
        }

        if (player != handler->GetSession()->GetPlayer() && handler->GetSession()->GetSecurity() < SEC_GAMEMASTER)
            return false;
        GrantClassHeirlooms(player, true);

        if (handler->GetSession() && player != handler->GetSession()->GetPlayer())
            handler->PSendSysMessage("Checked heirloom delivery for %s.", player->GetName().c_str());
        return true;
    }

    static bool HandleCompanionCommand(ChatHandler* handler, Optional<std::string_view> classArg)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        uint8 requestedClass = 0;
        if (classArg)
        {
            std::string cArg(classArg->data(), classArg->size());
            std::transform(cArg.begin(), cArg.end(), cArg.begin(), [](unsigned char c) { return std::tolower(c); });

            if (cArg == "heirlooms" || cArg == "heirloom")
            {
                GrantClassHeirlooms(player, true);
                return true;
            }

            if (cArg == "dk" || cArg == "deathknight" || cArg == "death_knight" || cArg == "darkknight" || cArg == "dark_knight")
                requestedClass = BOT_CLASS_DEATH_KNIGHT;
            else if (cArg == "warrior" || cArg == "warr")
                requestedClass = BOT_CLASS_WARRIOR;
            else if (cArg == "paladin" || cArg == "pala")
                requestedClass = BOT_CLASS_PALADIN;
            else if (cArg == "hunter")
                requestedClass = BOT_CLASS_HUNTER;
            else if (cArg == "rogue")
                requestedClass = BOT_CLASS_ROGUE;
            else if (cArg == "priest")
                requestedClass = BOT_CLASS_PRIEST;
            else if (cArg == "shaman" || cArg == "sham")
                requestedClass = BOT_CLASS_SHAMAN;
            else if (cArg == "mage")
                requestedClass = BOT_CLASS_MAGE;
            else if (cArg == "warlock" || cArg == "lock")
                requestedClass = BOT_CLASS_WARLOCK;
            else if (cArg == "druid")
                requestedClass = BOT_CLASS_DRUID;
            else if (cArg == "bm" || cArg == "blademaster")
                requestedClass = BOT_CLASS_BM;
            else if (cArg == "dreadlord")
                requestedClass = BOT_CLASS_DREADLORD;
            else if (cArg == "spellbreaker")
                requestedClass = BOT_CLASS_SPELLBREAKER;
            else if (cArg == "darkranger" || cArg == "dr")
                requestedClass = BOT_CLASS_DARK_RANGER;
            else if (cArg == "necromancer" || cArg == "necro")
                requestedClass = BOT_CLASS_NECROMANCER;
            else if (cArg == "seawitch")
                requestedClass = BOT_CLASS_SEA_WITCH;
            else if (cArg == "cryptlord")
                requestedClass = BOT_CLASS_CRYPT_LORD;
        }

        // If player doesn't have the item, give it!
        if (!player->HasItemCount(ITEM_COMPANION_STONE, 1))
        {
            ItemPosCountVec dest;
            InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, ITEM_COMPANION_STONE, 1);
            if (msg == EQUIP_ERR_OK)
            {
                Item* item = player->StoreNewItem(dest, ITEM_COMPANION_STONE, true);
                player->SendNewItem(item, 1, true, false);
                handler->PSendSysMessage("Ai primit Piatra Insotitorului (Companion Hearthstone)!");
            }
        }

        bool dismissAction = false;
        if (classArg)
        {
            std::string cArg(classArg->data(), classArg->size());
            std::transform(cArg.begin(), cArg.end(), cArg.begin(), [](unsigned char c) { return std::tolower(c); });
            if (cArg == "dismiss" || cArg == "clear" || cArg == "remove" || cArg == "reset" || cArg == "leave")
                dismissAction = true;
        }

        if (classArg && !requestedClass && !dismissAction)
        {
            handler->SendSysMessage("Clasa companionului nu este recunoscuta.");
            handler->SetSentErrorMessage(true);
            return false;
        }
        return ToggleCompanionBot(player, requestedClass, dismissAction);
    }
};


/*
 * Illidan Stormrage (World Boss Edition - Level 83):
 * Fully standalone open-world encounter with full voice acting, Warglaives dual-wielding,
 * Demon Form Metamorphosis, Shadow Blasts, Flame Bursts, Shear, and Draw Soul!
 */
struct boss_illidan_worldboss : public ScriptedAI
{
    boss_illidan_worldboss(Creature* creature) : ScriptedAI(creature), summons(creature)
    {
        Initialize();
    }

    EventMap events;
    SummonList summons;
    bool _demonForm;
    bool _enraged;
    uint32 _demonTimer;

    void Initialize()
    {
        _demonForm = false;
        _enraged = false;
        _demonTimer = 0;
    }

    void Reset() override
    {
        events.Reset();
        summons.DespawnAll();
        Initialize();

        me->SetFaction(14); // Hostile to all
        me->SetCanDualWield(true);
        me->LoadEquipment(1, true); // Twin Warglaives of Azzinoth
        me->SetSheath(SHEATH_STATE_MELEE);
        me->SetDisplayId(21135); // Normal Illidan
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        events.Reset();
        Initialize();

        me->SetCanDualWield(true);
        me->LoadEquipment(1, true);
        me->SetSheath(SHEATH_STATE_MELEE);

        me->PlayDistanceSound(11466); // "You are not prepared!"
        me->Yell("You are not prepared!", LANG_UNIVERSAL);

        events.ScheduleEvent(1, 4s);   // Shear
        events.ScheduleEvent(2, 7s);   // Draw Soul
        events.ScheduleEvent(3, 10s);  // Flame Crash
        events.ScheduleEvent(4, 15s);  // Agonizing Flames
        events.ScheduleEvent(5, 18s);  // Parasitic Shadowfiend
        events.ScheduleEvent(6, 45s);  // Metamorphosis / Demon Form
    }

    void UpdateAI(uint32 diff) override
    {
        if (!UpdateVictim())
            return;

        events.Update(diff);

        // Frenzy at <= 20% HP
        if (!_enraged && me->HealthBelowPct(20))
        {
            _enraged = true;
            me->PlayDistanceSound(11476); // "Feel the hatred of ten thousand years!"
            me->Yell("Feel the hatred of ten thousand years!", LANG_UNIVERSAL);
            DoCastSelf(40683); // SPELL_FRENZY
            DoCastSelf(40585); // SPELL_DARK_BARRAGE
        }

        if (me->HasUnitState(UNIT_STATE_CASTING))
            return;

        while (uint32 eventId = events.ExecuteEvent())
        {
            switch (eventId)
            {
                case 1: // Shear
                {
                    if (Unit* victim = me->GetVictim())
                        DoCast(victim, 41032);
                    events.ScheduleEvent(1, 25s, 35s);
                    break;
                }
                case 2: // Draw Soul
                {
                    if (Unit* victim = me->GetVictim())
                        DoCast(victim, 40904);
                    events.ScheduleEvent(2, 12s, 16s);
                    break;
                }
                case 3: // Flame Crash
                {
                    if (Unit* victim = me->GetVictim())
                        DoCast(victim, 40832);
                    events.ScheduleEvent(3, 15s, 22s);
                    break;
                }
                case 4: // Agonizing Flames
                {
                    if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 40.0f, true))
                        DoCast(target, 40932);
                    events.ScheduleEvent(4, 20s, 30s);
                    break;
                }
                case 5: // Parasitic Shadowfiend
                {
                    if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 30.0f, true))
                        DoCast(target, 41917);
                    events.ScheduleEvent(5, 25s, 35s);
                    break;
                }
                case 6: // Metamorphosis (Demon Form)
                {
                    if (!_demonForm)
                    {
                        _demonForm = true;
                        me->PlayDistanceSound(11475); // "Behold the power of the demon within!"
                        me->Yell("Behold the power of the demon within!", LANG_UNIVERSAL);
                        DoCastSelf(40506); // SPELL_DEMON_FORM
                        DoCastSelf(41142); // SPELL_AURA_OF_DREAD
                        _demonTimer = 45000; // 45 seconds in Demon Form

                        events.ScheduleEvent(7, 2s);  // Shadow Blast
                        events.ScheduleEvent(8, 6s);  // Flame Burst
                        events.ScheduleEvent(9, 15s); // Summon Shadow Demons
                    }
                    events.ScheduleEvent(6, 75s);
                    break;
                }
                case 7: // Shadow Blast (Demon Form)
                {
                    if (_demonForm)
                    {
                        if (Unit* victim = me->GetVictim())
                            DoCast(victim, 41078);
                        events.ScheduleEvent(7, 6s, 9s);
                    }
                    break;
                }
                case 8: // Flame Burst (Demon Form)
                {
                    if (_demonForm)
                    {
                        DoCastSelf(41126);
                        events.ScheduleEvent(8, 8s, 12s);
                    }
                    break;
                }
                case 9: // Summon Shadow Demons
                {
                    if (_demonForm)
                    {
                        DoCastSelf(41117);
                        events.ScheduleEvent(9, 20s);
                    }
                    break;
                }
            }
        }

        // Handle Demon Form expiration back to Night Elf Illidan
        if (_demonForm)
        {
            if (diff >= _demonTimer)
            {
                _demonForm = false;
                _demonTimer = 0;
                me->RemoveAurasDueToSpell(40506);
                me->RemoveAurasDueToSpell(41142);
                me->SetDisplayId(21135);
                me->Yell("I shall return to my blades!", LANG_UNIVERSAL);
            }
            else
                _demonTimer -= diff;
        }

        DoMeleeAttackIfReady();
    }

    void KilledUnit(Unit* victim) override
    {
        if (victim && (victim->IsPlayer() || victim->IsNPCBot()))
        {
            me->PlayDistanceSound(11473); // "Who shall be next to taste my blades?"
            me->Yell("Who shall be next to taste my blades?", LANG_UNIVERSAL);
            // Heals 2% of max HP on kill
            me->ModifyHealth(int32(me->CountPctFromMaxHealth(0.25f)));
        }
    }

    void JustDied(Unit* /*killer*/) override
    {
        me->PlayDistanceSound(11478);
        me->Yell("You have won... but the huntress is nothing without the hunt...", LANG_UNIVERSAL);
        summons.DespawnAll();
    }
};

class boss_illidan_worldboss_script : public CreatureScript
{
public:
    boss_illidan_worldboss_script() : CreatureScript("boss_illidan_worldboss") { }

    CreatureAI* GetAI(Creature* creature) const override
    {
        return new boss_illidan_worldboss(creature);
    }
};

/*
 * Auto Learn Spells on Level Up and Login
 * Automatically learns all available class spells and ranks from class trainers
 * whenever the player gains a level or logs into the game, without needing an NPC trainer.
 */
class AutoLearnSpellsScript : public PlayerScript
{
public:
    AutoLearnSpellsScript() : PlayerScript("AutoLearnSpellsScript") { }

    static void LearnClassSpells(Player* player)
    {
        if (!player || !player->IsInWorld())
            return;

        if (!sConfigMgr->GetBoolDefault("AutoLearnSpells.Enable", true))
            return;

        ChrClassesEntry const* classEntry = sChrClassesStore.LookupEntry(player->GetClass());
        if (!classEntry)
            return;

        std::vector<Trainer::Trainer const*> const& trainers = sObjectMgr->GetClassTrainers(player->GetClass());

        bool hadNew;
        uint32 iterations = 0;
        uint32 totalLearned = 0;
        do
        {
            hadNew = false;
            ++iterations;
            for (Trainer::Trainer const* trainer : trainers)
            {
                if (!trainer || !trainer->IsTrainerValidForPlayer(player))
                    continue;

                for (Trainer::Spell const& trainerSpell : trainer->GetSpells())
                {
                    if (!trainer->CanTeachSpell(player, &trainerSpell))
                        continue;

                    SpellInfo const* info = sSpellMgr->GetSpellInfo(trainerSpell.SpellId);
                    if (!info)
                        continue;
                    std::vector<std::pair<uint32, bool>> learned;
                    for (auto const& effect : info->GetEffects())
                        if (effect.IsEffect(SPELL_EFFECT_LEARN_SPELL) && effect.TriggerSpell)
                            learned.emplace_back(effect.TriggerSpell, player->HasSpell(effect.TriggerSpell));
                    if (learned.empty())
                        learned.emplace_back(trainerSpell.SpellId, player->HasSpell(trainerSpell.SpellId));
                    if (trainerSpell.IsCastable())
                        player->CastSpell(player, trainerSpell.SpellId, true);
                    else
                        player->LearnSpell(trainerSpell.SpellId, false);
                    for (auto const& [spell, known] : learned)
                        if (!known && player->HasSpell(spell))
                        {
                            hadNew = true;
                            ++totalLearned;
                        }
                }
            }
        } while (hadNew && iterations < 15);

        if (hadNew && iterations == 15)
            TC_LOG_WARN("scripts", "AutoLearn reached prerequisite pass limit for {}", player->GetGUID().ToString());
        if (totalLearned > 0 && sConfigMgr->GetBoolDefault("AutoLearnSpells.Announce", true))
        {
            ChatHandler(player->GetSession()).SendSysMessage(Trinity::StringFormat("|cff00ff00[Auto Learn]|r Ai invatat automat {} abilitati noi de clasa!", totalLearned));
        }
    }

    void OnLevelChanged(Player* player, uint8 /*oldLevel*/) override
    {
        LearnClassSpells(player);
    }

    void OnLogin(Player* player, bool /*firstLogin*/) override
    {
        LearnClassSpells(player);
        if (!player->HasItemCount(70902, 1, true))
        {
            ItemPosCountVec dest;
            InventoryResult msg = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, 70902, 1);
            if (msg == EQUIP_ERR_OK)
            {
                if (Item* item = player->StoreNewItem(dest, 70902, true))
                    player->SendNewItem(item, 1, true, false);
            }
        }
    }
};

void AddCustomScripts()
{
    new boss_dark_knight_nemesis_script();
    new item_companion_stone();
    new item_junk_scrapper();
    new companion_commandscript();
    new companion_playerscript();
    new companion_delivery_worldscript();
    new boss_illidan_worldboss_script();
    new AutoLearnSpellsScript();
}

