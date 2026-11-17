/*
 * Custom Scripts:
 * 1. The Dark Knight (Nemesis of the Living) - 5-Man Dungeon Boss roaming the open world.
 * 2. Companion Hearthstone - Item to summon or dismiss a random companion bot (Max 1).
 */

#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "Player.h"
#include "Creature.h"
#include "SpellAuras.h"
#include "SpellMgr.h"
#include "MotionMaster.h"
#include "botwanderful.h"
#include "Containers.h"
#include "botdatamgr.h"
#include "botmgr.h"
#include "bot_ai.h"
#include "Chat.h"
#include "WorldSession.h"
#include "DatabaseEnv.h"
#include "GameTime.h"
#include "SpellHistory.h"
#include "SpellPackets.h"
#include <cmath>

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
 * For Non-GM: strictly 1 companion limit. Toggling dismisses or switches class. 5h cooldown after 1st use.
 * For GM: UNLIMITED companions! Can summon as many companions as desired. 0 cooldown.
 * Dismissing:
 * - Targeting an owned bot and using .companion / stone or .companion dismiss: dismisses targeted bot.
 * - .companion dismiss (without target) for GM: dismisses all companion bots.
 */
static bool ToggleCompanionBot(Player* player, uint8 requestedClass = 0, bool dismissAction = false)
{
    if (!player || !player->IsInWorld())
        return false;

    bool const isGM = (player->GetSession() && player->GetSession()->GetSecurity() >= SEC_GAMEMASTER);
    uint32 const guidLow = player->GetGUID().GetCounter();
    time_t const now = GameTime::GetGameTime();
    constexpr uint64 COOLDOWN_SECONDS = 5 * 3600; // 5 hours

    // 0. Handle explicit dismiss action (.companion dismiss / clear)
    if (dismissAction)
    {
        if (Unit* target = player->GetSelectedUnit())
        {
            if (target->IsCreature() && player->GetBotMgr()->GetBot(target->GetGUID()))
            {
                std::string name = target->GetName();
                player->GetBotMgr()->RemoveBot(target->GetGUID(), BOT_REMOVE_DISMISS);
                ChatHandler(player->GetSession()).PSendSysMessage("Companion bot %s has been dismissed.", name.c_str());
                player->CastSpell(player, 64034, true); // Visual puff
                return true;
            }
        }

        if (isGM)
        {
            if (player->HaveBot() && player->GetBotMgr()->GetBotMap() && !player->GetBotMgr()->GetBotMap()->empty())
            {
                player->GetBotMgr()->RemoveAllBots(BOT_REMOVE_DISMISS);
                ChatHandler(player->GetSession()).SendSysMessage("All companion bots have been dismissed.");
                player->CastSpell(player, 64034, true);
                return true;
            }
            else
            {
                ChatHandler(player->GetSession()).SendSysMessage("You have no companion bots to dismiss.");
                return false;
            }
        }
        else
        {
            // Non-GM dismiss active bot
            if (player->HaveBot() && player->GetBotMgr()->GetBotMap() && !player->GetBotMgr()->GetBotMap()->empty())
            {
                Creature* activeBot = player->GetBotMgr()->GetBotMap()->begin()->second;
                if (activeBot)
                {
                    std::string name = activeBot->GetName();
                    player->GetBotMgr()->RemoveBot(activeBot->GetGUID(), BOT_REMOVE_DISMISS);
                    ChatHandler(player->GetSession()).PSendSysMessage("Companion bot %s has been dismissed.", name.c_str());
                    player->CastSpell(player, 64034, true);
                    return true;
                }
            }
            return false;
        }
    }

    // Check first use and cooldown in database (only for non-GM)
    bool firstUseDone = false;
    uint64 lastUseTime = 0;

    QueryResult cResult = CharacterDatabase.Query(Trinity::StringFormat("SELECT first_use_done, last_use_time FROM character_companion_stone WHERE guid = {}", guidLow).c_str());
    if (cResult)
    {
        Field* fields = cResult->Fetch();
        firstUseDone = (fields[0].GetUInt8() != 0);
        lastUseTime = fields[1].GetUInt64();
    }

    if (firstUseDone && !isGM)
    {
        uint64 const diff = (now > lastUseTime) ? (now - lastUseTime) : 0;
        if (diff < COOLDOWN_SECONDS)
        {
            uint32 const remSec = static_cast<uint32>(COOLDOWN_SECONDS - diff);
            uint32 const hours = remSec / 3600;
            uint32 const mins = (remSec % 3600) / 60;
            ChatHandler(player->GetSession()).PSendSysMessage("|cffff0000[Companion Hearthstone]|r Piatra este in cooldown! Timp ramas: %u ore si %u minute.", hours, mins);
            return false;
        }
    }

    // 1. Non-GM single-companion toggle / switch logic
    if (!isGM)
    {
        if (player->HaveBot() && player->GetBotMgr()->GetBotMap() && !player->GetBotMgr()->GetBotMap()->empty())
        {
            Creature* activeBot = nullptr;
            for (auto const& [guid, bot] : *player->GetBotMgr()->GetBotMap())
            {
                if (bot)
                {
                    activeBot = bot;
                    break;
                }
            }

            if (activeBot)
            {
                // If player requested the same class or toggled without argument, dismiss it
                if (requestedClass == 0 || activeBot->GetBotClass() == requestedClass)
                {
                    std::string name = activeBot->GetName();
                    player->GetBotMgr()->RemoveBot(activeBot->GetGUID(), BOT_REMOVE_DISMISS);
                    ChatHandler(player->GetSession()).PSendSysMessage("Companion bot %s has been dismissed.", name.c_str());
                    player->CastSpell(player, 64034, true); // Visual puff

                    // Update cooldown
                    if (!firstUseDone)
                    {
                        CharacterDatabase.Execute(Trinity::StringFormat("REPLACE INTO character_companion_stone (guid, first_use_done, last_use_time) VALUES ({}, 1, 0)", guidLow).c_str());
                        player->GetSpellHistory()->ResetCooldown(55884, true);
                        ChatHandler(player->GetSession()).SendSysMessage("|cff00ff00[Companion Hearthstone]|r Insotitorul a fost retras. (Prima utilizare: fara cooldown. Urmatoarele utilizari vor avea cooldown de 5 ore.)");
                    }
                    else
                    {
                        CharacterDatabase.Execute(Trinity::StringFormat("REPLACE INTO character_companion_stone (guid, first_use_done, last_use_time) VALUES ({}, 1, {})", guidLow, uint64(now)).c_str());
                        const uint32 cdMs = COOLDOWN_SECONDS * 1000;
                        player->GetSpellHistory()->AddCooldown(55884, ITEM_COMPANION_STONE, std::chrono::hours(5));

                        WorldPackets::Spells::SpellCooldown spellCooldown;
                        spellCooldown.Caster = player->GetGUID();
                        spellCooldown.Flags = 0;
                        spellCooldown.SpellCooldowns.emplace_back(55884, cdMs);
                        player->SendDirectMessage(spellCooldown.Write());

                        ChatHandler(player->GetSession()).SendSysMessage("|cff00ccff[Companion Hearthstone]|r Cooldown de 5 ore activat.");
                    }
                    return true;
                }
                else
                {
                    // Switching to a different class -> dismiss current one first
                    player->GetBotMgr()->RemoveBot(activeBot->GetGUID(), BOT_REMOVE_DISMISS);
                }
            }
        }
    }
    else
    {
        // GM: if GM has an owned companion bot selected as target and ran .companion without class argument, dismiss that targeted bot
        if (requestedClass == 0)
        {
            if (Unit* target = player->GetSelectedUnit())
            {
                if (target->IsCreature() && player->GetBotMgr()->GetBot(target->GetGUID()))
                {
                    std::string name = target->GetName();
                    player->GetBotMgr()->RemoveBot(target->GetGUID(), BOT_REMOVE_DISMISS);
                    ChatHandler(player->GetSession()).PSendSysMessage("Companion bot %s has been dismissed.", name.c_str());
                    player->CastSpell(player, 64034, true);
                    return true;
                }
            }
        }
    }

    // 2. Summon companion bot
    if (!isGM && player->IsInCombat())
    {
        ChatHandler(player->GetSession()).SendSysMessage("You cannot summon a companion bot while in combat!");
        return false;
    }

    // Pick race matching player's team (Alliance / Horde)
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
        if (player->GetClass() == CLASS_DEATH_KNIGHT || player->GetLevel() >= 55)
            query = "SELECT entry FROM creature_template_npcbot_extras WHERE race IN (" + raceFilter +
                    ") AND class BETWEEN 1 AND 11 ORDER BY RAND() LIMIT 20";
        else
            query = "SELECT entry FROM creature_template_npcbot_extras WHERE race IN (" + raceFilter +
                    ") AND class IN (1, 2, 3, 4, 5, 7, 8, 9, 11) ORDER BY RAND() LIMIT 20";
    }

    QueryResult result = WorldDatabase.Query(query.c_str());
    if (!result && requestedClass != 0)
    {
        // Fallback: any race for requested class
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
        if (!BotDataMgr::FindBot(candidate) && !alreadyOwned)
        {
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
    uint8 spec = BotDataMgr::SelectSpecForClass(extras->bclass);
    uint32 roles = BotDataMgr::DefaultRolesForClass(extras->bclass, spec);

    if (!BotDataMgr::SelectNpcBotData(botEntry))
        BotDataMgr::AddNpcBotData(botEntry, roles, spec, faction);

    uint32 ownerGuidLow = player->GetGUID().GetCounter();
    BotDataMgr::UpdateNpcBotData(botEntry, NPCBOT_UPDATE_OWNER, &ownerGuidLow);

    Position pos = player->GetPosition();
    Creature* bot = new Creature();
    if (!bot->LoadBotCreatureFromDB(0, player->GetMap(), true, true, botEntry, &pos))
    {
        delete bot;
        ChatHandler(player->GetSession()).SendSysMessage("Failed to spawn companion bot!");
        return false;
    }

    // Align bot phase mask with player immediately (crucial in Acherus / phased zones)
    bot->SetPhaseMask(player->GetPhaseMask(), true);

    if (player->GetBotMgr()->AddBot(bot) == BOT_ADD_SUCCESS)
    {
        player->CastSpell(player, 64036, true); // Visual light effect
        ChatHandler(player->GetSession()).PSendSysMessage("Companion bot %s has arrived to fight by your side!", bot->GetName().c_str());

        // Update cooldown only for non-GM
        if (!isGM)
        {
            if (!firstUseDone)
            {
                CharacterDatabase.Execute(Trinity::StringFormat("REPLACE INTO character_companion_stone (guid, first_use_done, last_use_time) VALUES ({}, 1, 0)", guidLow).c_str());
                player->GetSpellHistory()->ResetCooldown(55884, true);
                ChatHandler(player->GetSession()).SendSysMessage("|cff00ff00[Companion Hearthstone]|r Insotitorul tau de leveling a sosit! (Prima utilizare: fara cooldown. Urmatoarele utilizari vor avea cooldown de 5 ore.)");
            }
            else
            {
                CharacterDatabase.Execute(Trinity::StringFormat("REPLACE INTO character_companion_stone (guid, first_use_done, last_use_time) VALUES ({}, 1, {})", guidLow, uint64(now)).c_str());
                const uint32 cdMs = COOLDOWN_SECONDS * 1000;
                player->GetSpellHistory()->AddCooldown(55884, ITEM_COMPANION_STONE, std::chrono::hours(5));

                WorldPackets::Spells::SpellCooldown spellCooldown;
                spellCooldown.Caster = player->GetGUID();
                spellCooldown.Flags = 0;
                spellCooldown.SpellCooldowns.emplace_back(55884, cdMs);
                player->SendDirectMessage(spellCooldown.Write());

                ChatHandler(player->GetSession()).SendSysMessage("|cff00ccff[Companion Hearthstone]|r Cooldown de 5 ore activat.");
            }
        }
        return true;
    }
    else
    {
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

class companion_playerscript : public PlayerScript
{
public:
    companion_playerscript() : PlayerScript("companion_playerscript") { }

    void OnLogin(Player* player, bool /*firstLogin*/) override
    {
        if (!player)
            return;

        // 1. Give Companion Hearthstone to existing characters if missing
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

        // 2. Synchronize active cooldown upon login
        uint32 const guidLow = player->GetGUID().GetCounter();
        QueryResult cResult = CharacterDatabase.Query(Trinity::StringFormat("SELECT first_use_done, last_use_time FROM character_companion_stone WHERE guid = {}", guidLow).c_str());
        if (cResult)
        {
            Field* fields = cResult->Fetch();
            bool firstUseDone = (fields[0].GetUInt8() != 0);
            uint64 lastUseTime = fields[1].GetUInt64();

            if (firstUseDone && lastUseTime > 0)
            {
                time_t const now = GameTime::GetGameTime();
                uint64 const diff = (now > lastUseTime) ? (now - lastUseTime) : 0;
                constexpr uint64 COOLDOWN_SECONDS = 5 * 3600;
                if (diff < COOLDOWN_SECONDS)
                {
                    uint32 const remSec = static_cast<uint32>(COOLDOWN_SECONDS - diff);
                    player->GetSpellHistory()->AddCooldown(55884, ITEM_COMPANION_STONE, std::chrono::seconds(remSec));

                    WorldPackets::Spells::SpellCooldown spellCooldown;
                    spellCooldown.Caster = player->GetGUID();
                    spellCooldown.Flags = 0;
                    spellCooldown.SpellCooldowns.emplace_back(55884, remSec * 1000);
                    player->SendDirectMessage(spellCooldown.Write());
                }
            }
        }
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
            { "companion", HandleCompanionCommand, rbac::RBAC_PERM_COMMAND_NPCBOT_INFO, Console::No }
        };
        return commandTable;
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

        // Otherwise toggle/summon companion
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

void AddCustomScripts()
{
    new boss_dark_knight_nemesis_script();
    new item_companion_stone();
    new companion_commandscript();
    new companion_playerscript();
    new boss_illidan_worldboss_script();
}

