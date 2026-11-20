-- Package the local Undercity encounter setup for the bundled world database.
-- Keep established spawn GUIDs and any unrelated assignments.
START TRANSACTION;

UPDATE `creature_template`
SET `AIName` = '', `ScriptName` = 'npc_zidormi_undercity', `npcflag` = `npcflag` | 1
WHERE `entry` = 31848 AND `ScriptName` IN ('', 'npc_zidormi_undercity');

UPDATE `creature_template`
SET `AIName` = '', `ScriptName` = 'npc_thrall_battle_for_undercity', `npcflag` = `npcflag` | 3
WHERE `entry` = 31650 AND `ScriptName` IN ('', 'npc_thrall_battle_for_undercity');

UPDATE `creature_template`
SET `AIName` = '', `ScriptName` = 'npc_sylvanas_battle_for_undercity'
WHERE `entry` = 31651 AND `ScriptName` IN ('', 'npc_sylvanas_battle_for_undercity');

UPDATE `creature_template`
SET `AIName` = '', `ScriptName` = 'boss_varimathras_battle_for_undercity', `faction` = 16
WHERE `entry` = 31565 AND `ScriptName` IN ('', 'boss_varimathras_battle_for_undercity');

-- Putress ships with both player- and NPC-immunity flags in the bundled snapshot.
UPDATE `creature_template`
SET `AIName` = '', `ScriptName` = 'boss_putress_battle_for_undercity', `faction` = 14,
    `minlevel` = 82, `maxlevel` = 82, `unit_flags` = `unit_flags` & ~768
WHERE `entry` = 31530 AND `ScriptName` IN ('', 'boss_putress_battle_for_undercity');

INSERT INTO `creature` (`id`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`,
    `position_x`, `position_y`, `position_z`, `orientation`, `curhealth`, `curmana`, `npcflag`)
SELECT event_spawn.`id`, 0, 85, event_spawn.`areaId`, 1, event_spawn.`phaseMask`,
    event_spawn.`x`, event_spawn.`y`, event_spawn.`z`, event_spawn.`o`,
    event_spawn.`health`, event_spawn.`mana`, event_spawn.`npcflag`
FROM (
    SELECT 31848 AS `id`, 153 AS `areaId`, 65 AS `phaseMask`,
        1947.0 AS `x`, 230.0 AS `y`, 44.25 AS `z`, 3.14 AS `o`, 8000 AS `health`, 0 AS `mana`, 1 AS `npcflag`
    UNION ALL SELECT 31848, 153, 65, 1805.0, 450.0, 15.0, 4.7, 8000, 0, 1
    UNION ALL SELECT 31565, 1497, 64, 1275.0, 316.0, -57.24, 0.0, 3500000, 100000, 0
    UNION ALL SELECT 31530, 1497, 64, 1543.31, 362.002, -62.1596, 0.0, 2500000, 100000, 0
    UNION ALL SELECT 31650, 1497, 64, 1335.0, 316.0, -63.5, 3.14, 7925544, 0, 3
    UNION ALL SELECT 31651, 1497, 64, 1337.0, 319.0, -63.5, 3.14, 6972500, 0, 0
) AS event_spawn
WHERE NOT EXISTS (
    SELECT 1 FROM `creature` AS existing
    WHERE existing.`id` = event_spawn.`id` AND existing.`map` = 0
      AND (existing.`phaseMask` & event_spawn.`phaseMask`) = event_spawn.`phaseMask`
      AND ABS(existing.`position_x` - event_spawn.`x`) < 5
      AND ABS(existing.`position_y` - event_spawn.`y`) < 5
      AND ABS(existing.`position_z` - event_spawn.`z`) < 5
);

COMMIT;
