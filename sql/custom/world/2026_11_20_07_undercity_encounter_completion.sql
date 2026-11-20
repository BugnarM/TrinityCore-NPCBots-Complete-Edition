-- Encounter Completion and Parity for Battle for the Undercity (Horde 13267 & Alliance 13377)
-- References:
--   AzerothCore WotLK PR #1479 (commit 5379ed193b6008d40168fe6ce807b268f2d2d525 by Killyana)
--   Dalaran-WoW zone_undercity.cpp
--   TrinityCore 3.3.5a NPCBots Complete Edition

START TRANSACTION;

-- 1. Ensure Broll Bearmantle (32376) and King Varian Wrynn (32303) are quest starters for 13377
UPDATE `creature_template`
SET `minlevel` = 80, `maxlevel` = 80, `faction` = 1733, `npcflag` = `npcflag` | 3, `unit_flags` = 0
WHERE `entry` = 32376;

UPDATE `creature_template`
SET `AIName` = '', `ScriptName` = 'npc_varian_battle_for_undercity', `npcflag` = `npcflag` | 3
WHERE `entry` = 32303 AND `ScriptName` IN ('', 'npc_varian_battle_for_undercity');

INSERT IGNORE INTO `creature_queststarter` (`id`, `quest`) VALUES
(32376, 13377),
(32303, 13377);

-- 2. Ensure King Varian Wrynn (32303) in Undercity can end quest 13377 (along with Stormwind Keep 29611)
INSERT IGNORE INTO `creature_questender` (`id`, `quest`) VALUES
(32303, 13377);

-- Ensure Thrall (31650) in Undercity can end quest 13267
INSERT IGNORE INTO `creature_questender` (`id`, `quest`) VALUES
(31650, 13267);

-- 3. Spawn Broll Bearmantle outside the Undercity sewer entrance (phaseMask 65 = visible in peace and siege)
INSERT INTO `creature` (`id`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`,
    `position_x`, `position_y`, `position_z`, `orientation`, `curhealth`, `curmana`, `npcflag`)
SELECT 32376, 0, 85, 153, 1, 65, 1812.0, 454.0, 15.0, 4.7, 45000, 20000, 3
WHERE NOT EXISTS (
    SELECT 1 FROM `creature`
    WHERE `id` = 32376 AND `map` = 0
      AND `position_x` BETWEEN 1795.0 AND 1830.0
      AND `position_y` BETWEEN 440.0 AND 470.0
);

-- 4. Quest event-completion flags in quest_template_addon (SpecialFlags bit 2 = QUEST_SPECIAL_FLAGS_EXPLORATION_OR_EVENT)
UPDATE `quest_template_addon`
SET `SpecialFlags` = `SpecialFlags` | 2
WHERE `ID` IN (13267, 13377);

-- 5. Spell Area: Phase 64 autocast for Alliance upon completing prerequisite 13371 (The Killing Time)
INSERT INTO `spell_area` (`spell`, `area`, `quest_start`, `quest_end`, `aura_spell`, `racemask`, `gender`,
    `autocast`, `quest_start_status`, `quest_end_status`) VALUES
(59062, 85,   13371, 13377, 0, 1101, 2, 1, 64, 11),
(59062, 1497, 13371, 13377, 0, 1101, 2, 1, 64, 11)
ON DUPLICATE KEY UPDATE `autocast` = 1;

-- Ensure incomplete active quests for both factions maintain autocast in Tirisfal (85) and Undercity (1497)
UPDATE `spell_area`
SET `autocast` = 1
WHERE `spell` = 59062 AND `quest_start` IN (13267, 13377) AND `quest_start_status` = 8;

-- 6. Alliance Sewer & Canal Interior Corridor Spawns (phaseMask 64)
UPDATE `creature_template`
SET `minlevel` = 75, `maxlevel` = 76, `faction` = 16, `unit_flags` = 0
WHERE `entry` IN (32394, 32395, 32396, 32397);

UPDATE `creature_template`
SET `minlevel` = 75, `maxlevel` = 76, `faction` = 16, `unit_flags` = 0
WHERE `entry` IN (32269, 32519);

UPDATE `creature_template`
SET `AIName` = '', `ScriptName` = 'boss_putress_battle_for_undercity', `faction` = 14,
    `minlevel` = 82, `maxlevel` = 82, `unit_flags` = 0
WHERE `entry` = 31530;

INSERT INTO `creature` (`id`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`,
    `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `StringId`)
SELECT pack.`entry`, 0, 85, 1497, 1, 64, pack.`x`, pack.`y`, pack.`z`, pack.`o`, 180, pack.`spawn_key`
FROM (
    SELECT 32394 AS `entry`, 1760.0 AS `x`, 412.0 AS `y`, -57.14 AS `z`, 3.94 AS `o`,
        'undercity_assault_alliance_sewer_01' AS `spawn_key`
    UNION ALL SELECT 32395, 1758.0, 415.0, -57.14, 3.94, 'undercity_assault_alliance_sewer_02'
    UNION ALL SELECT 32394, 1762.0, 410.0, -57.14, 3.94, 'undercity_assault_alliance_sewer_03'
    UNION ALL SELECT 32396, 1695.0, 424.0, -62.21, 2.50, 'undercity_assault_alliance_canal_01'
    UNION ALL SELECT 32394, 1692.0, 420.0, -62.21, 2.50, 'undercity_assault_alliance_canal_02'
    UNION ALL SELECT 32395, 1690.0, 422.0, -62.21, 2.50, 'undercity_assault_alliance_canal_03'
    UNION ALL SELECT 32397, 1585.0, 368.0, -62.08, 3.80, 'undercity_assault_alliance_apoth_01'
    UNION ALL SELECT 32394, 1588.0, 365.0, -62.08, 3.80, 'undercity_assault_alliance_apoth_02'
    UNION ALL SELECT 32396, 1583.0, 370.0, -62.08, 3.80, 'undercity_assault_alliance_apoth_03'
) AS pack
WHERE NOT EXISTS (
    SELECT 1 FROM `creature` AS existing
    WHERE existing.`id` = pack.`entry` AND existing.`map` = 0
      AND existing.`StringId` = pack.`spawn_key`
);

-- 7. English Creature Text for Varian, Putress, and Varimathras
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `comment`) VALUES
(32303, 241, 0, 'Justice has been served for the fallen at the Wrathgate! The Alliance has triumphed!', 14, 0, 100, 'Undercity custom event: Varian - victory'),
(32303, 242, 0, 'Hold here and destroy the apothecary guards before advancing on Putress!', 14, 0, 100, 'Undercity custom event: Varian - apoth gate')
ON DUPLICATE KEY UPDATE `Text` = VALUES(`Text`);

COMMIT;
