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
#include "AchievementMgr.h"
#include "CellImpl.h"
#include "Containers.h"
#include "CreatureAIImpl.h"
#include "GameObject.h"
#include "GridNotifiers.h"
#include "GridNotifiersImpl.h"
#include "MotionMaster.h"
#include "ObjectAccessor.h"
#include "PassiveAI.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "SpellAuraEffects.h"
#include "SpellInfo.h"
#include "SpellScript.h"

enum HallowEndCandysSpells
{
    SPELL_HALLOWS_END_CANDY_ORANGE_GIANT          = 24924, // Effect 1: Apply Aura: Mod Size, Value: 30%
    SPELL_HALLOWS_END_CANDY_SKELETON              = 24925, // Effect 1: Apply Aura: Change Model (Skeleton). Effect 2: Apply Aura: Underwater Breathing
    SPELL_HALLOWS_END_CANDY_PIRATE                = 24926, // Effect 1: Apply Aura: Increase Swim Speed, Value: 50%
    SPELL_HALLOWS_END_CANDY_GHOST                 = 24927, // Effect 1: Apply Aura: Levitate / Hover. Effect 2: Apply Aura: Slow Fall, Effect 3: Apply Aura: Water Walking
    SPELL_HALLOWS_END_CANDY_FEMALE_DEFIAS_PIRATE  = 44742, // Effect 1: Apply Aura: Change Model (Defias Pirate, Female). Effect 2: Increase Swim Speed, Value: 50%
    SPELL_HALLOWS_END_CANDY_MALE_DEFIAS_PIRATE    = 44743  // Effect 1: Apply Aura: Change Model (Defias Pirate, Male).   Effect 2: Increase Swim Speed, Value: 50%
};

std::array<uint32, 4> const CandysSpells =
{
    SPELL_HALLOWS_END_CANDY_ORANGE_GIANT,
    SPELL_HALLOWS_END_CANDY_SKELETON,
    SPELL_HALLOWS_END_CANDY_PIRATE,
    SPELL_HALLOWS_END_CANDY_GHOST
};

// 24930 - Hallow's End Candy
class spell_hallow_end_candy : public SpellScript
{
    PrepareSpellScript(spell_hallow_end_candy);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo(CandysSpells);
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        GetCaster()->CastSpell(GetCaster(), Trinity::Containers::SelectRandomContainerElement(CandysSpells), true);
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_hallow_end_candy::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

// 24926 - Hallow's End Candy
class spell_hallow_end_candy_pirate : public AuraScript
{
    PrepareAuraScript(spell_hallow_end_candy_pirate);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo(
        {
            SPELL_HALLOWS_END_CANDY_FEMALE_DEFIAS_PIRATE,
            SPELL_HALLOWS_END_CANDY_MALE_DEFIAS_PIRATE
        });
    }

    void HandleApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        uint32 spell = GetTarget()->GetNativeGender() == GENDER_FEMALE ? SPELL_HALLOWS_END_CANDY_FEMALE_DEFIAS_PIRATE : SPELL_HALLOWS_END_CANDY_MALE_DEFIAS_PIRATE;
        GetTarget()->CastSpell(GetTarget(), spell, true);
    }

    void HandleRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        uint32 spell = GetTarget()->GetNativeGender() == GENDER_FEMALE ? SPELL_HALLOWS_END_CANDY_FEMALE_DEFIAS_PIRATE : SPELL_HALLOWS_END_CANDY_MALE_DEFIAS_PIRATE;
        GetTarget()->RemoveAurasDueToSpell(spell);
    }

    void Register() override
    {
        AfterEffectApply += AuraEffectApplyFn(spell_hallow_end_candy_pirate::HandleApply, EFFECT_0, SPELL_AURA_MOD_INCREASE_SWIM_SPEED, AURA_EFFECT_HANDLE_REAL);
        AfterEffectRemove += AuraEffectRemoveFn(spell_hallow_end_candy_pirate::HandleRemove, EFFECT_0, SPELL_AURA_MOD_INCREASE_SWIM_SPEED, AURA_EFFECT_HANDLE_REAL);
    }
};

enum TrickInitial
{
    SPELL_TRICK_INITIAL     = 24750
};

// 24714 - Trick
class spell_hallow_end_trick_initial : public SpellScript
{
    PrepareSpellScript(spell_hallow_end_trick_initial);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_TRICK_INITIAL });
    }

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        GetHitUnit()->CastSpell(GetHitUnit(), SPELL_TRICK_INITIAL);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_hallow_end_trick_initial::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

enum TrickSpells
{
    SPELL_PIRATE_COSTUME_MALE           = 24708,
    SPELL_PIRATE_COSTUME_FEMALE         = 24709,
    SPELL_NINJA_COSTUME_MALE            = 24710,
    SPELL_NINJA_COSTUME_FEMALE          = 24711,
    SPELL_LEPER_GNOME_COSTUME_MALE      = 24712,
    SPELL_LEPER_GNOME_COSTUME_FEMALE    = 24713,
    SPELL_SKELETON_COSTUME              = 24723,
    SPELL_GHOST_COSTUME_MALE            = 24735,
    SPELL_GHOST_COSTUME_FEMALE          = 24736,
    SPELL_TRICK_BUFF                    = 24753,
};

// 24750 - Trick
class spell_hallow_end_trick : public SpellScript
{
    PrepareSpellScript(spell_hallow_end_trick);

    bool Validate(SpellInfo const* /*spell*/) override
    {
        return ValidateSpellInfo(
        {
            SPELL_PIRATE_COSTUME_MALE,
            SPELL_PIRATE_COSTUME_FEMALE,
            SPELL_NINJA_COSTUME_MALE,
            SPELL_NINJA_COSTUME_FEMALE,
            SPELL_LEPER_GNOME_COSTUME_MALE,
            SPELL_LEPER_GNOME_COSTUME_FEMALE,
            SPELL_SKELETON_COSTUME,
            SPELL_GHOST_COSTUME_MALE,
            SPELL_GHOST_COSTUME_FEMALE,
            SPELL_TRICK_BUFF
        });
    }

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        if (Player* target = GetHitPlayer())
        {
            uint8 gender = target->GetNativeGender();
            uint32 spellId = SPELL_TRICK_BUFF;
            switch (urand(0, 5))
            {
                case 1:
                    spellId = gender == GENDER_FEMALE ? SPELL_LEPER_GNOME_COSTUME_FEMALE : SPELL_LEPER_GNOME_COSTUME_MALE;
                    break;
                case 2:
                    spellId = gender == GENDER_FEMALE ? SPELL_PIRATE_COSTUME_FEMALE : SPELL_PIRATE_COSTUME_MALE;
                    break;
                case 3:
                    spellId = gender == GENDER_FEMALE ? SPELL_GHOST_COSTUME_FEMALE : SPELL_GHOST_COSTUME_MALE;
                    break;
                case 4:
                    spellId = gender == GENDER_FEMALE ? SPELL_NINJA_COSTUME_FEMALE : SPELL_NINJA_COSTUME_MALE;
                    break;
                case 5:
                    spellId = SPELL_SKELETON_COSTUME;
                    break;
                default:
                    break;
            }

            caster->CastSpell(target, spellId, true);
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_hallow_end_trick::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

enum TrickOrTreatSpells
{
    SPELL_TRICK                 = 24714,
    SPELL_TREAT                 = 24715,
    SPELL_TRICKED_OR_TREATED    = 24755,
    SPELL_TRICKY_TREAT_SPEED    = 42919,
    SPELL_TRICKY_TREAT_TRIGGER  = 42965,
    SPELL_UPSET_TUMMY           = 42966
};

// 24751 - Trick or Treat
class spell_hallow_end_trick_or_treat : public SpellScript
{
    PrepareSpellScript(spell_hallow_end_trick_or_treat);

    bool Validate(SpellInfo const* /*spell*/) override
    {
        return ValidateSpellInfo({ SPELL_TRICK, SPELL_TREAT, SPELL_TRICKED_OR_TREATED });
    }

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        if (Player* target = GetHitPlayer())
        {
            caster->CastSpell(target, roll_chance_i(50) ? SPELL_TRICK : SPELL_TREAT, true);
            caster->CastSpell(target, SPELL_TRICKED_OR_TREATED, true);
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_hallow_end_trick_or_treat::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

// 44436 - Tricky Treat
class spell_hallow_end_tricky_treat : public SpellScript
{
    PrepareSpellScript(spell_hallow_end_tricky_treat);

    bool Validate(SpellInfo const* /*spell*/) override
    {
        return ValidateSpellInfo(
        {
            SPELL_TRICKY_TREAT_SPEED,
            SPELL_TRICKY_TREAT_TRIGGER,
            SPELL_UPSET_TUMMY
        });
    }

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        if (caster->HasAura(SPELL_TRICKY_TREAT_TRIGGER) && caster->GetAuraCount(SPELL_TRICKY_TREAT_SPEED) > 3 && roll_chance_i(33))
            caster->CastSpell(caster, SPELL_UPSET_TUMMY, true);
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_hallow_end_tricky_treat::HandleScript, EFFECT_0, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

enum HallowendData
{
    SPELL_HALLOWED_WAND_PIRATE             = 24717,
    SPELL_HALLOWED_WAND_NINJA              = 24718,
    SPELL_HALLOWED_WAND_LEPER_GNOME        = 24719,
    SPELL_HALLOWED_WAND_RANDOM             = 24720,
    SPELL_HALLOWED_WAND_SKELETON           = 24724,
    SPELL_HALLOWED_WAND_WISP               = 24733,
    SPELL_HALLOWED_WAND_GHOST              = 24737,
    SPELL_HALLOWED_WAND_BAT                = 24741
};

// 24717, 24718, 24719, 24720, 24724, 24733, 24737, 24741
class spell_hallow_end_wand : public SpellScript
{
    PrepareSpellScript(spell_hallow_end_wand);

    bool Validate(SpellInfo const* /*spellEntry*/) override
    {
        return ValidateSpellInfo(
        {
            SPELL_PIRATE_COSTUME_MALE,
            SPELL_PIRATE_COSTUME_FEMALE,
            SPELL_NINJA_COSTUME_MALE,
            SPELL_NINJA_COSTUME_FEMALE,
            SPELL_LEPER_GNOME_COSTUME_MALE,
            SPELL_LEPER_GNOME_COSTUME_FEMALE,
            SPELL_GHOST_COSTUME_MALE,
            SPELL_GHOST_COSTUME_FEMALE
        });
    }

    void HandleScriptEffect()
    {
        Unit* caster = GetCaster();
        Unit* target = GetHitUnit();

        uint32 spellId = 0;
        uint8 gender = target->GetNativeGender();

        switch (GetSpellInfo()->Id)
        {
            case SPELL_HALLOWED_WAND_LEPER_GNOME:
                spellId = gender ? SPELL_LEPER_GNOME_COSTUME_FEMALE : SPELL_LEPER_GNOME_COSTUME_MALE;
                break;
            case SPELL_HALLOWED_WAND_PIRATE:
                spellId = gender ? SPELL_PIRATE_COSTUME_FEMALE : SPELL_PIRATE_COSTUME_MALE;
                break;
            case SPELL_HALLOWED_WAND_GHOST:
                spellId = gender ? SPELL_GHOST_COSTUME_FEMALE : SPELL_GHOST_COSTUME_MALE;
                break;
            case SPELL_HALLOWED_WAND_NINJA:
                spellId = gender ? SPELL_NINJA_COSTUME_FEMALE : SPELL_NINJA_COSTUME_MALE;
                break;
            case SPELL_HALLOWED_WAND_RANDOM:
                spellId = RAND(SPELL_HALLOWED_WAND_PIRATE, SPELL_HALLOWED_WAND_NINJA, SPELL_HALLOWED_WAND_LEPER_GNOME, SPELL_HALLOWED_WAND_SKELETON, SPELL_HALLOWED_WAND_WISP, SPELL_HALLOWED_WAND_GHOST, SPELL_HALLOWED_WAND_BAT);
                break;
            default:
                return;
        }
        caster->CastSpell(target, spellId, true);
    }

    void Register() override
    {
        AfterHit += SpellHitFn(spell_hallow_end_wand::HandleScriptEffect);
    }
};

enum CreateWaterBucket
{
    SPELL_CREATE_WATER_BUCKET_BARREL_SPLASH    = 43244,
    SPELL_JUST_LOOTED_WATER_BARREL             = 44410
};

// 42144 - Headless Horseman - Create Water Bucket
class spell_hallow_end_create_water_bucket : public SpellScript
{
    PrepareSpellScript(spell_hallow_end_create_water_bucket);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_CREATE_WATER_BUCKET_BARREL_SPLASH, SPELL_JUST_LOOTED_WATER_BARREL });
    }

    void HandleScript(SpellEffIndex /*effIndex*/)
    {
        Unit* target = GetHitUnit();
        if (!target)
            return;

        target->CastSpell(target, SPELL_CREATE_WATER_BUCKET_BARREL_SPLASH);
        target->CastSpell(target, SPELL_JUST_LOOTED_WATER_BARREL);

        if (Player* player = target->ToPlayer())
        {
            if (!player->HasItemCount(32971, 1))
                player->AddItem(32971, 1);
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_hallow_end_create_water_bucket::HandleScript, EFFECT_1, SPELL_EFFECT_SCRIPT_EFFECT);
    }
};

/* =========================================================================
 *   HALLOW'S END - GOLDSHIRE TOWN DEFENSE & SHADE OF THE HORSEMAN
 * ========================================================================= */

enum HallowEndEncounterSpells
{
    SPELL_HEADLESS_HORSEMAN_FIRE_MISSILE           = 42132, // Headless Horseman - Start Fire
    SPELL_HEADLESS_HORSEMAN_BURNING_COSMETIC_BASE  = 43184,
    SPELL_HEADLESS_HORSEMAN_FIRE_SIZE_BIG          = 43148,
    SPELL_HEADLESS_HORSEMAN_FIRE_EXTINGUISH_SPLASH = 42348,
    SPELL_HEADLESS_HORSEMAN_SMOKE                  = 42355,
    SPELL_HEADLESS_HORSEMAN_LAUGH                  = 43893,
    SPELL_HEADLESS_HORSEMAN_CLEAVE                 = 42587,
    SPELL_CONFLAGRATION                            = 42380,
    SPELL_HEADLESS_HORSEMAN_WHIRLWIND              = 43116,
    SPELL_SPROUTING                                = 42281, // Summon Pumpkin Fiends
    SPELL_RAIN_OF_TREATS                           = 43344,
    SPELL_QUEST_CREDIT_1                           = 42242,
    SPELL_QUEST_CREDIT_2                           = 47775,
    SPELL_VICTORY_EVENT                            = 42682,
    SPELL_THROW_BUCKET                             = 42340,
    SPELL_BUCKET_LANDS                             = 42339,
    SPELL_CREATE_WATER_BUCKET                      = 42144
};

enum HallowEndCreatures
{
    NPC_SHADE_OF_THE_HORSEMAN    = 23543,
    NPC_FLAME_BUNNY_GOLDSHIRE    = 23537,
    NPC_FLAME_BUNNY_EXTRA        = 23686,
    NPC_PUMPKIN_FIEND            = 23545,
    NPC_COSTUMED_ORPHAN_MATRON   = 24519
};

enum HallowEndGameObjects
{
    GO_LARGE_JACK_O_LANTERN      = 186887,
    GO_WATER_BARREL              = 186234,
    GO_WATER_BUCKETS             = 186615
};

enum HallowEndQuests
{
    QUEST_FIRE_BRIGADE_PRACTICE  = 11360,
    QUEST_STOP_THE_FIRES_A       = 11131,
    QUEST_LET_THE_FIRES_COME_A   = 12135,
    QUEST_SMASH_THE_PUMPKIN      = 12133
};

enum HallowEndSounds
{
    SOUND_HORSEMAN_ENTRANCE      = 11966, // "Prepare yourselves, the bells have tolled!..."
    SOUND_HORSEMAN_FIRE_BURNS    = 11967, // "The sky is dark.  The fire burns..."
    SOUND_HORSEMAN_FIRES_OUT     = 11968, // "My flames have died, left not a spark..."
    SOUND_HORSEMAN_FAIL          = 12570, // "Fire consumes! You've tried and failed..."
    SOUND_HORSEMAN_CLEANSING     = 12571, // "The town still burns, a cleansing fire!..."
    SOUND_HORSEMAN_CONFLAGRATION = 12573, // "Harken, cur!  Tis you I spurn!..."
    SOUND_HORSEMAN_SPROUTING     = 11963, // "Soldiers arise, stand and fight!..."
    SOUND_HORSEMAN_DEATH         = 11964  // "This end have I reached before..."
};

enum ShadeSays
{
    SAY_ENTRANCE        = 0,
    SAY_FIRE_BURNS      = 1,
    SAY_CLEANSING_FIRE  = 2,
    SAY_FAILURE         = 3,
    SAY_LANDING         = 4,
    SAY_CONFLAGRATION   = 5,
    SAY_SPROUTING       = 6,
    SAY_DEATH           = 7
};

enum MatronSays
{
    SAY_MATRON_ALERT    = 0,
    SAY_MATRON_VICTORY  = 1
};

enum MovementPoints
{
    POINT_WP_0     = 0,
    POINT_WP_1     = 1,
    POINT_WP_2     = 2,
    POINT_WP_3     = 3,
    POINT_WP_4     = 4,
    POINT_LAND     = 100,
    POINT_FLY_AWAY = 101
};

Position const FlightWaypoints[5] =
{
    { -9465.0f, 22.0f, 75.0f, 0.0f }, // WP 0: Over Lion's Pride Inn
    { -9460.0f, 48.0f, 75.0f, 0.0f }, // WP 1: Over Crossroads
    { -9470.0f, 85.0f, 75.0f, 0.0f }, // WP 2: Over Blacksmith
    { -9495.0f, 75.0f, 75.0f, 0.0f }, // WP 3: Over Town Hall
    { -9435.0f, 55.0f, 75.0f, 0.0f }  // WP 4: Over Matron / East side
};

Position const GoldshireCrossroads(-9460.0f, 45.0f, 56.5f, 4.7f);
Position const FlightAwayPoint(-9460.0f, 45.0f, 150.0f, 0.0f);

Position const FirePositions[10] =
{
    // Lion's Pride Inn (Roof & Porch)
    { -9468.0f, 25.0f, 66.5f, 0.0f },
    { -9460.0f, 15.0f, 65.5f, 0.0f },
    { -9456.0f, 32.0f, 65.5f, 0.0f },
    { -9456.0f, 40.0f, 57.0f, 0.0f }, // Porch

    // Blacksmith (Roof & Yard)
    { -9470.0f, 82.0f, 64.0f, 0.0f },
    { -9465.0f, 75.0f, 57.0f, 0.0f },
    { -9475.0f, 90.0f, 58.0f, 0.0f },

    // Town Hall / General Store (Roof & Steps)
    { -9495.0f, 75.0f, 64.0f, 0.0f },
    { -9488.0f, 68.0f, 56.5f, 0.0f },
    { -9498.0f, 85.0f, 57.0f, 0.0f }
};

// Flame Bunny / Burning Fire AI
struct npc_flame_bunny_goldshire : public ScriptedAI
{
    npc_flame_bunny_goldshire(Creature* creature) : ScriptedAI(creature), _extinguished(false)
    {
        creature->SetDisplayId(11686);
        creature->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE);
        creature->SetFaction(14);
    }

    void Reset() override
    {
        _extinguished = false;
        _scheduler.CancelAll();
        _scheduler.Schedule(500ms, [this](TaskContext /*context*/)
        {
            DoCastSelf(SPELL_HEADLESS_HORSEMAN_BURNING_COSMETIC_BASE, true);
        }).Schedule(2s, [this](TaskContext /*context*/)
        {
            DoCastSelf(SPELL_HEADLESS_HORSEMAN_FIRE_SIZE_BIG, true);
        });
    }

    void Extinguish(Player* player)
    {
        if (_extinguished)
            return;

        _extinguished = true;
        me->RemoveAllAuras();
        DoCastSelf(SPELL_HEADLESS_HORSEMAN_FIRE_EXTINGUISH_SPLASH, true);
        DoCastSelf(SPELL_HEADLESS_HORSEMAN_SMOKE, true);

        if (player)
        {
            player->KilledMonsterCredit(NPC_FLAME_BUNNY_GOLDSHIRE);
        }

        me->DespawnOrUnsummon(1500ms);
    }

    bool IsExtinguished() const { return _extinguished; }

    void UpdateAI(uint32 diff) override
    {
        _scheduler.Update(diff);
    }

private:
    TaskScheduler _scheduler;
    bool _extinguished;
};

// Shade of the Horseman AI
struct npc_shade_of_the_horseman : public ScriptedAI
{
    npc_shade_of_the_horseman(Creature* creature) : ScriptedAI(creature),
        _phase(PHASE_INTRO), _currentWP(0), _bombTimer(0), _failTimer(0),
        _firesSpawned(false), _sprouted(false), _cleaveTimer(6000),
        _conflagTimer(15000), _whirlwindTimer(22000), _checkFiresTimer(1000)
    {
    }

    enum Phases
    {
        PHASE_INTRO,
        PHASE_FLIGHT_BOMBING,
        PHASE_LANDING,
        PHASE_COMBAT,
        PHASE_FLY_AWAY
    };

    void InitializeAI() override
    {
        me->SetDisableGravity(true);
        me->SetCanFly(true);
        me->SetHover(true);
        me->AddUnitMovementFlag(MOVEMENTFLAG_FLYING | MOVEMENTFLAG_DISABLE_GRAVITY | MOVEMENTFLAG_HOVER);
        ScriptedAI::InitializeAI();
    }

    void Reset() override
    {
        _phase = PHASE_INTRO;
        _currentWP = 0;
        _bombTimer = 0;
        _failTimer = 0;
        _firesSpawned = false;
        _sprouted = false;
        _cleaveTimer = 6000;
        _conflagTimer = 15000;
        _whirlwindTimer = 22000;
        _checkFiresTimer = 1000;
        _fireGuids.clear();

        me->SetCanFly(true);
        me->SetDisableGravity(true);
        me->SetHover(true);
        me->AddUnitMovementFlag(MOVEMENTFLAG_FLYING | MOVEMENTFLAG_DISABLE_GRAVITY | MOVEMENTFLAG_HOVER);
        me->SetSpeed(MOVE_FLIGHT, 1.8f);
        me->SetSpeed(MOVE_RUN, 1.8f);
        me->Mount(22653); // Headless Horseman flying fiery steed (Model 2647)
        me->SetUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_UNINTERACTIBLE);
        me->SetReactState(REACT_PASSIVE);

        me->SetMaxHealth(350000);
        me->SetHealth(350000);

        // Entrance rhyme & sound
        Talk(SAY_ENTRANCE);
        me->PlayDirectSound(SOUND_HORSEMAN_ENTRANCE);

        // Begin bombing run to WP 0
        _phase = PHASE_FLIGHT_BOMBING;
        me->GetMotionMaster()->MovePoint(POINT_WP_0, FlightWaypoints[0], false);
    }

    void MovementInform(uint32 type, uint32 id) override
    {
        if (type != POINT_MOTION_TYPE)
            return;

        switch (id)
        {
            case POINT_WP_0: // Over Lion's Pride Inn
            {
                SpawnBuildingFires(0, 4); // First 4 fires (Inn)
                Talk(SAY_FIRE_BURNS);
                me->PlayDirectSound(SOUND_HORSEMAN_FIRE_BURNS);
                _currentWP = 1;
                me->GetMotionMaster()->MovePoint(POINT_WP_1, FlightWaypoints[1], false);
                break;
            }
            case POINT_WP_1: // Over Crossroads
            {
                _currentWP = 2;
                me->GetMotionMaster()->MovePoint(POINT_WP_2, FlightWaypoints[2], false);
                break;
            }
            case POINT_WP_2: // Over Blacksmith
            {
                SpawnBuildingFires(4, 3); // Fires 4-6 (Blacksmith)
                DoCastSelf(SPELL_HEADLESS_HORSEMAN_LAUGH, true);
                _currentWP = 3;
                me->GetMotionMaster()->MovePoint(POINT_WP_3, FlightWaypoints[3], false);
                break;
            }
            case POINT_WP_3: // Over Town Hall
            {
                SpawnBuildingFires(7, 3); // Fires 7-9 (Town Hall)
                _firesSpawned = true;
                Talk(SAY_CLEANSING_FIRE);
                me->PlayDirectSound(SOUND_HORSEMAN_CLEANSING);
                _currentWP = 4;
                me->GetMotionMaster()->MovePoint(POINT_WP_4, FlightWaypoints[4], false);
                break;
            }
            case POINT_WP_4: // Over East side
            {
                _currentWP = 0;
                me->GetMotionMaster()->MovePoint(POINT_WP_0, FlightWaypoints[0], false);
                break;
            }
            case POINT_LAND:
            {
                _phase = PHASE_COMBAT;
                me->Dismount();
                me->SetHover(false);
                me->SetCanFly(false);
                me->SetDisableGravity(false);
                me->RemoveUnitMovementFlag(MOVEMENTFLAG_FLYING | MOVEMENTFLAG_DISABLE_GRAVITY | MOVEMENTFLAG_HOVER);
                me->RemoveUnitFlag(UNIT_FLAG_NON_ATTACKABLE | UNIT_FLAG_UNINTERACTIBLE);
                me->SetReactState(REACT_AGGRESSIVE);
                DoZoneInCombat();
                break;
            }
            case POINT_FLY_AWAY:
            {
                DespawnRemainingFires();
                me->DespawnOrUnsummon();
                break;
            }
            default:
                break;
        }
    }

    void SpawnBuildingFires(uint32 startIdx, uint32 count)
    {
        for (uint32 i = startIdx; i < startIdx + count && i < 10; ++i)
        {
            if (Creature* fire = me->SummonCreature(NPC_FLAME_BUNNY_GOLDSHIRE, FirePositions[i], TEMPSUMMON_MANUAL_DESPAWN))
            {
                me->CastSpell(fire, SPELL_HEADLESS_HORSEMAN_FIRE_MISSILE, true);
                _fireGuids.push_back(fire->GetGUID());
            }
        }
    }

    uint32 GetActiveFiresCount()
    {
        uint32 count = 0;
        for (ObjectGuid const& guid : _fireGuids)
        {
            if (Creature* fire = ObjectAccessor::GetCreature(*me, guid))
            {
                if (fire->IsAlive())
                {
                    if (auto* ai = CAST_AI(npc_flame_bunny_goldshire, fire->AI()))
                    {
                        if (!ai->IsExtinguished())
                            count++;
                    }
                    else
                        count++;
                }
            }
        }
        return count;
    }

    void DespawnRemainingFires()
    {
        for (ObjectGuid const& guid : _fireGuids)
        {
            if (Creature* fire = ObjectAccessor::GetCreature(*me, guid))
                fire->DespawnOrUnsummon();
        }
        _fireGuids.clear();
    }

    void JustEngagedWith(Unit* /*who*/) override
    {
        DoCastSelf(SPELL_HEADLESS_HORSEMAN_LAUGH, true);
    }

    void JustDied(Unit* /*killer*/) override
    {
        Talk(SAY_DEATH);
        me->PlayDirectSound(SOUND_HORSEMAN_DEATH);

        DoCastSelf(SPELL_RAIN_OF_TREATS, true);
        DoCastSelf(SPELL_QUEST_CREDIT_1, true);
        DoCastSelf(SPELL_QUEST_CREDIT_2, true);
        DoCastSelf(SPELL_VICTORY_EVENT, true);

        // Spawn Large Jack-o'-Lantern at death location for 5 minutes
        me->SummonGameObject(GO_LARGE_JACK_O_LANTERN, me->GetPosition(), QuaternionData(), 5min);

        // Reward and complete quests for all players in 100 yard radius
        std::vector<Player*> players;
        me->GetPlayerListInGrid(players, 100.0f);
        for (Player* player : players)
        {
            if (player->GetQuestStatus(QUEST_STOP_THE_FIRES_A) == QUEST_STATUS_INCOMPLETE)
                player->CompleteQuest(QUEST_STOP_THE_FIRES_A);
            if (player->GetQuestStatus(QUEST_LET_THE_FIRES_COME_A) == QUEST_STATUS_INCOMPLETE)
                player->CompleteQuest(QUEST_LET_THE_FIRES_COME_A);
            if (player->GetQuestStatus(QUEST_FIRE_BRIGADE_PRACTICE) == QUEST_STATUS_INCOMPLETE)
                player->CompleteQuest(QUEST_FIRE_BRIGADE_PRACTICE);

            if (AchievementEntry const* ach = sAchievementStore.LookupEntry(289)) // The Savior of Hallow's End
            {
                if (!player->HasAchieved(289))
                    player->CompletedAchievement(ach);
            }
        }

        // Matron announcement
        if (Creature* matron = me->FindNearestCreature(NPC_COSTUMED_ORPHAN_MATRON, 150.0f, true))
        {
            matron->AI()->Talk(SAY_MATRON_VICTORY);
        }

        DespawnRemainingFires();
    }

    void UpdateAI(uint32 diff) override
    {
        if (_phase == PHASE_FLIGHT_BOMBING)
        {
            _failTimer += diff;
            if (_failTimer >= 210000) // 3.5 minutes timeout
            {
                _phase = PHASE_FLY_AWAY;
                Talk(SAY_FAILURE);
                me->PlayDirectSound(SOUND_HORSEMAN_FAIL);
                me->GetMotionMaster()->MovePoint(POINT_FLY_AWAY, FlightAwayPoint, false);
                return;
            }

            if (_checkFiresTimer <= diff)
            {
                _checkFiresTimer = 1000;
                if (_firesSpawned && GetActiveFiresCount() == 0)
                {
                    // All fires extinguished! Horseman descends!
                    _phase = PHASE_LANDING;
                    Talk(SAY_LANDING);
                    me->PlayDirectSound(SOUND_HORSEMAN_FIRES_OUT);
                    me->GetMotionMaster()->MovePoint(POINT_LAND, GoldshireCrossroads, false);
                    return;
                }
            }
            else
                _checkFiresTimer -= diff;

            return;
        }

        if (_phase != PHASE_COMBAT)
            return;

        if (!UpdateVictim())
            return;

        // Cleave
        if (_cleaveTimer <= diff)
        {
            DoCastVictim(SPELL_HEADLESS_HORSEMAN_CLEAVE);
            _cleaveTimer = urand(6000, 8000);
        }
        else
            _cleaveTimer -= diff;

        // Conflagration
        if (_conflagTimer <= diff)
        {
            if (Unit* target = SelectTarget(SelectTargetMethod::Random, 0, 30.0f, true, false))
            {
                Talk(SAY_CONFLAGRATION);
                me->PlayDirectSound(SOUND_HORSEMAN_CONFLAGRATION);
                DoCast(target, SPELL_CONFLAGRATION);
            }
            _conflagTimer = urand(16000, 20000);
        }
        else
            _conflagTimer -= diff;

        // Whirlwind
        if (_whirlwindTimer <= diff)
        {
            DoCastSelf(SPELL_HEADLESS_HORSEMAN_WHIRLWIND);
            _whirlwindTimer = urand(22000, 26000);
        }
        else
            _whirlwindTimer -= diff;

        // Sprouting at <= 50% HP
        if (!_sprouted && HealthBelowPct(50))
        {
            _sprouted = true;
            Talk(SAY_SPROUTING);
            me->PlayDirectSound(SOUND_HORSEMAN_SPROUTING);
            DoCastSelf(SPELL_SPROUTING, true);
            for (uint8 i = 0; i < 4; ++i)
            {
                Position pos = me->GetRandomPoint(me->GetPosition(), 5.0f);
                me->SummonCreature(NPC_PUMPKIN_FIEND, pos, TEMPSUMMON_TIMED_DESPAWN_OUT_OF_COMBAT, 60s);
            }
        }

        DoMeleeAttackIfReady();
    }

private:
    Phases _phase;
    uint8 _currentWP;
    uint32 _bombTimer;
    uint32 _failTimer;
    bool _firesSpawned;
    bool _sprouted;
    uint32 _cleaveTimer;
    uint32 _conflagTimer;
    uint32 _whirlwindTimer;
    uint32 _checkFiresTimer;
    std::vector<ObjectGuid> _fireGuids;
};

// Costumed Orphan Matron AI
struct npc_costumed_orphan_matron : public ScriptedAI
{
    npc_costumed_orphan_matron(Creature* creature) : ScriptedAI(creature),
        _autoEventTimer(1800000) // 30 minutes
    {
    }

    bool OnGossipHello(Player* player) override
    {
        if (me->IsQuestGiver())
            player->PrepareQuestMenu(me->GetGUID());

        Creature* horseman = me->FindNearestCreature(NPC_SHADE_OF_THE_HORSEMAN, 300.0f, true);
        if (!horseman)
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Sound the alarm! The Headless Horseman approaches!", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 1);
        }
        else
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "What can I do to help defend Goldshire?", GOSSIP_SENDER_MAIN, GOSSIP_ACTION_INFO_DEF + 2);
        }

        SendGossipMenuFor(player, player->GetGossipTextId(me), me->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, uint32 /*menuId*/, uint32 gossipListId) override
    {
        uint32 const action = player->PlayerTalkClass->GetGossipOptionAction(gossipListId);
        ClearGossipMenuFor(player);
        CloseGossipMenuFor(player);

        if (action == GOSSIP_ACTION_INFO_DEF + 1)
        {
            StartHorsemanAttack();
        }
        else if (action == GOSSIP_ACTION_INFO_DEF + 2)
        {
            me->Whisper("Grab a water bucket from the barrel next to me and throw water on the burning buildings!", LANG_UNIVERSAL, player);
        }
        return true;
    }

    void StartHorsemanAttack()
    {
        Creature* existing = me->FindNearestCreature(NPC_SHADE_OF_THE_HORSEMAN, 300.0f, true);
        if (existing)
            return;

        Talk(SAY_MATRON_ALERT);
        Position const spawnPos(-9460.0f, 45.0f, 75.0f, 4.7f);
        me->SummonCreature(NPC_SHADE_OF_THE_HORSEMAN, spawnPos, TEMPSUMMON_MANUAL_DESPAWN);
    }

    void UpdateAI(uint32 diff) override
    {
        if (_autoEventTimer <= diff)
        {
            _autoEventTimer = 1800000; // Reset 30 min
            StartHorsemanAttack();
        }
        else
            _autoEventTimer -= diff;
    }

private:
    uint32 _autoEventTimer;
};

// 42340 - Throw Bucket
class spell_hallow_end_throw_bucket : public SpellScript
{
    PrepareSpellScript(spell_hallow_end_throw_bucket);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_BUCKET_LANDS, SPELL_HEADLESS_HORSEMAN_FIRE_EXTINGUISH_SPLASH, SPELL_HEADLESS_HORSEMAN_SMOKE, SPELL_CREATE_WATER_BUCKET });
    }

    void HandleBucket(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        Player* player = caster->ToPlayer();

        // Bucket Brigade: If thrown at another player, pass bucket!
        if (Player* hitPlayer = GetHitPlayer())
        {
            if (hitPlayer != player)
            {
                hitPlayer->CastSpell(hitPlayer, SPELL_CREATE_WATER_BUCKET, true);
                return;
            }
        }

        Position targetPos;
        if (WorldObject* obj = GetExplTargetWorldObject())
            targetPos = obj->GetPosition();
        else if (GetExplTargetDest())
            targetPos = *GetExplTargetDest();
        else if (GetHitUnit())
            targetPos = GetHitUnit()->GetPosition();
        else
            targetPos = caster->GetPosition();

        std::list<Creature*> fires;
        GetCreatureListWithEntryInGrid(fires, caster, NPC_FLAME_BUNNY_GOLDSHIRE, 60.0f);
        std::list<Creature*> fires2;
        GetCreatureListWithEntryInGrid(fires2, caster, NPC_FLAME_BUNNY_EXTRA, 60.0f);
        fires.splice(fires.end(), fires2);

        for (Creature* fire : fires)
        {
            if (fire && fire->IsAlive() && fire->GetExactDist2d(targetPos.GetPositionX(), targetPos.GetPositionY()) <= 15.0f)
            {
                if (auto* ai = CAST_AI(npc_flame_bunny_goldshire, fire->AI()))
                    ai->Extinguish(player);
                else
                {
                    fire->CastSpell(fire, SPELL_HEADLESS_HORSEMAN_FIRE_EXTINGUISH_SPLASH, true);
                    fire->CastSpell(fire, SPELL_HEADLESS_HORSEMAN_SMOKE, true);
                    if (player)
                        player->KilledMonsterCredit(NPC_FLAME_BUNNY_GOLDSHIRE);
                    fire->DespawnOrUnsummon(1000ms);
                }
            }
        }
    }

    void Register() override
    {
        OnEffectHit += SpellEffectFn(spell_hallow_end_throw_bucket::HandleBucket, EFFECT_0, SPELL_EFFECT_TRIGGER_SPELL);
    }
};

// 42339 - Bucket Lands
class spell_hallow_end_bucket_lands : public SpellScript
{
    PrepareSpellScript(spell_hallow_end_bucket_lands);

    bool Validate(SpellInfo const* /*spellInfo*/) override
    {
        return ValidateSpellInfo({ SPELL_HEADLESS_HORSEMAN_FIRE_EXTINGUISH_SPLASH, SPELL_HEADLESS_HORSEMAN_SMOKE });
    }

    void HandleDummy(SpellEffIndex /*effIndex*/)
    {
        Unit* caster = GetCaster();
        if (!caster)
            return;

        Player* player = caster->ToPlayer();

        Position targetPos;
        if (WorldObject* obj = GetExplTargetWorldObject())
            targetPos = obj->GetPosition();
        else if (GetExplTargetDest())
            targetPos = *GetExplTargetDest();
        else if (GetHitUnit())
            targetPos = GetHitUnit()->GetPosition();
        else
            targetPos = caster->GetPosition();

        std::list<Creature*> fires;
        GetCreatureListWithEntryInGrid(fires, caster, NPC_FLAME_BUNNY_GOLDSHIRE, 60.0f);
        std::list<Creature*> fires2;
        GetCreatureListWithEntryInGrid(fires2, caster, NPC_FLAME_BUNNY_EXTRA, 60.0f);
        fires.splice(fires.end(), fires2);

        for (Creature* fire : fires)
        {
            if (fire && fire->IsAlive() && fire->GetExactDist2d(targetPos.GetPositionX(), targetPos.GetPositionY()) <= 15.0f)
            {
                if (auto* ai = CAST_AI(npc_flame_bunny_goldshire, fire->AI()))
                    ai->Extinguish(player);
                else
                {
                    fire->CastSpell(fire, SPELL_HEADLESS_HORSEMAN_FIRE_EXTINGUISH_SPLASH, true);
                    fire->CastSpell(fire, SPELL_HEADLESS_HORSEMAN_SMOKE, true);
                    if (player)
                        player->KilledMonsterCredit(NPC_FLAME_BUNNY_GOLDSHIRE);
                    fire->DespawnOrUnsummon(1000ms);
                }
            }
        }
    }

    void Register() override
    {
        OnEffectHitTarget += SpellEffectFn(spell_hallow_end_bucket_lands::HandleDummy, EFFECT_0, SPELL_EFFECT_DUMMY);
    }
};

void AddSC_event_hallows_end()
{
    RegisterSpellScript(spell_hallow_end_candy);
    RegisterSpellScript(spell_hallow_end_candy_pirate);
    RegisterSpellScript(spell_hallow_end_trick_initial);
    RegisterSpellScript(spell_hallow_end_trick);
    RegisterSpellScript(spell_hallow_end_trick_or_treat);
    RegisterSpellScript(spell_hallow_end_tricky_treat);
    RegisterSpellScript(spell_hallow_end_wand);
    RegisterSpellScript(spell_hallow_end_create_water_bucket);

    // Goldshire Hallow's End town defense
    RegisterSpellScript(spell_hallow_end_throw_bucket);
    RegisterSpellScript(spell_hallow_end_bucket_lands);
    RegisterCreatureAI(npc_flame_bunny_goldshire);
    RegisterCreatureAI(npc_shade_of_the_horseman);
    RegisterCreatureAI(npc_costumed_orphan_matron);
}
