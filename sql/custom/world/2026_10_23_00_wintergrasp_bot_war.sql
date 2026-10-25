-- Optional scripted Wintergrasp armies. Apply to the world database before enabling BotWar.
-- These are ScriptedAI soldiers, not hireable NPCBots; wander-node tables are not used.
START TRANSACTION;

INSERT INTO `creature_template`
(`entry`, `modelid1`, `modelid2`, `modelid3`, `modelid4`, `name`, `subname`,
 `minlevel`, `maxlevel`, `exp`, `faction`, `speed_walk`, `speed_run`, `scale`, `rank`,
 `BaseAttackTime`, `RangeAttackTime`, `unit_class`, `type`, `HealthModifier`,
 `ManaModifier`, `ArmorModifier`, `DamageModifier`, `ExperienceModifier`,
 `RegenHealth`, `AIName`, `MovementType`, `ScriptName`)
SELECT bots.entry, source.modelid1, source.modelid2, source.modelid3, source.modelid4,
 bots.name, 'Wintergrasp combatant', 80, 80, 2, bots.faction, 1, 1, 1, 0,
 2000, 2000, bots.stat_class, 7, 2.5, 3, 1, 1, 0, 1, '', 0, 'npc_wg_bot_soldier'
FROM (
 SELECT 80501 AS entry, 30740 AS source_entry, 1732 AS faction, 1 AS stat_class, 'Wintergrasp Vanguard' AS name
 UNION ALL SELECT 80502, 30740, 1732, 8, 'Wintergrasp Frost Mage'
 UNION ALL SELECT 80503, 30740, 1732, 2, 'Wintergrasp Battle Priest'
 UNION ALL SELECT 80504, 30740, 1732, 2, 'Wintergrasp Marksman'
 UNION ALL SELECT 80505, 30739, 1735, 1, 'Wintergrasp Vanguard'
 UNION ALL SELECT 80506, 30739, 1735, 8, 'Wintergrasp Frost Mage'
 UNION ALL SELECT 80507, 30739, 1735, 2, 'Wintergrasp Battle Priest'
 UNION ALL SELECT 80508, 30739, 1735, 2, 'Wintergrasp Marksman'
) AS bots
JOIN `creature_template` AS source ON source.entry = bots.source_entry
ON DUPLICATE KEY UPDATE
 `name` = VALUES(`name`), `subname` = VALUES(`subname`),
 `minlevel` = 80, `maxlevel` = 80, `exp` = 2, `faction` = VALUES(`faction`),
 `unit_class` = VALUES(`unit_class`), `npcflag` = 0, `unit_flags` = 0,
 `unit_flags2` = 0, `dynamicflags` = 0, `rank` = 0, `type` = 7,
 `HealthModifier` = 2.5, `ManaModifier` = 3, `ArmorModifier` = 1,
 `DamageModifier` = 1, `ExperienceModifier` = 0, `mechanic_immune_mask` = 0,
 `spell_school_immune_mask` = 0, `flags_extra` = 0, `speed_run` = 1,
 `AIName` = '', `MovementType` = 0, `ScriptName` = 'npc_wg_bot_soldier';

-- Creature stat classes support only 1/2/4/8. Hunters need a mana stat class (2),
-- not rogue class 4. Their combat profile is selected by the entry in C++.
INSERT INTO `creature_equip_template` (`CreatureID`, `ID`, `ItemID1`, `ItemID2`, `ItemID3`)
VALUES (80501, 1, 40384, 0, 0), (80502, 1, 35, 0, 0),
       (80503, 1, 35, 0, 0), (80504, 1, 0, 0, 40385),
       (80505, 1, 40384, 0, 0), (80506, 1, 35, 0, 0),
       (80507, 1, 35, 0, 0), (80508, 1, 0, 0, 40385)
ON DUPLICATE KEY UPDATE `ItemID1` = VALUES(`ItemID1`),
 `ItemID2` = VALUES(`ItemID2`), `ItemID3` = VALUES(`ItemID3`);

INSERT INTO `creature_template_movement` (`CreatureId`, `Ground`, `Swim`, `Flight`, `Rooted`, `Chase`, `Random`)
VALUES (80501, 1, 1, 0, 0, 0, 0), (80502, 1, 1, 0, 0, 0, 0),
       (80503, 1, 1, 0, 0, 0, 0), (80504, 1, 1, 0, 0, 0, 0),
       (80505, 1, 1, 0, 0, 0, 0), (80506, 1, 1, 0, 0, 0, 0),
       (80507, 1, 1, 0, 0, 0, 0), (80508, 1, 1, 0, 0, 0, 0)
ON DUPLICATE KEY UPDATE `Ground` = 1, `Swim` = 1, `Flight` = 0,
 `Rooted` = 0, `Chase` = 0, `Random` = 0;

UPDATE `creature_template` SET `AIName` = '', `ScriptName` = 'npc_wg_bot_vehicle'
WHERE `entry` IN (27881, 28094, 28312, 32627);
UPDATE `creature_template` SET `AIName` = '', `ScriptName` = 'npc_wg_tower_cannon'
WHERE `entry` = 28366;

-- Chase=0 means Run in CreatureChaseMovementType.
UPDATE `creature_template_movement` SET `Ground` = 1, `Flight` = 0, `Rooted` = 0, `Chase` = 0
WHERE `CreatureId` IN (27881, 28094, 28312, 32627);

UPDATE `creature_template` SET `AIName` = '', `ScriptName` = 'npc_wg_bot_siege_turret'
WHERE `entry` IN (28319, 32629);

COMMIT;
