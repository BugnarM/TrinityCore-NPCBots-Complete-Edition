-- Supply exterior escorts where the local Undercity setup has only an interior Thrall
-- or where its exterior Varian spawn has not been installed.
INSERT INTO `creature` (`id`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`,
    `position_x`, `position_y`, `position_z`, `orientation`, `curhealth`, `npcflag`, `ScriptName`)
SELECT 31650, 0, 85, 153, 1, 64, 1949.0, 230.0, 44.25, 3.14, 7925544, 3, 'npc_thrall_battle_for_undercity'
WHERE NOT EXISTS (
    SELECT 1 FROM `creature`
    WHERE `id` = 31650 AND `map` = 0 AND (`phaseMask` & 64) <> 0
      AND `position_x` BETWEEN 1929.0 AND 1969.0
      AND `position_y` BETWEEN 210.0 AND 250.0 AND `position_z` BETWEEN 35.0 AND 55.0
);

INSERT INTO `creature` (`id`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`,
    `position_x`, `position_y`, `position_z`, `orientation`, `curhealth`, `npcflag`, `ScriptName`)
SELECT 32303, 0, 85, 153, 1, 64, 1808.0, 455.0, 15.0, 4.7, 8500000, 3, 'npc_varian_battle_for_undercity'
WHERE NOT EXISTS (
    SELECT 1 FROM `creature`
    WHERE `id` = 32303 AND `map` = 0 AND (`phaseMask` & 64) <> 0
      AND `position_x` BETWEEN 1788.0 AND 1828.0
      AND `position_y` BETWEEN 435.0 AND 475.0 AND `position_z` BETWEEN 5.0 AND 25.0
);

-- The base DB already has an exterior Thrall without a spawn-level script.
-- Bind existing exterior spawns without replacing other custom script assignments.
UPDATE `creature`
SET `npcflag` = `npcflag` | 3,
    `ScriptName` = CASE `id` WHEN 31650 THEN 'npc_thrall_battle_for_undercity'
                              WHEN 32303 THEN 'npc_varian_battle_for_undercity' END
WHERE `map` = 0 AND (`phaseMask` & 64) <> 0 AND (
    (`id` = 31650 AND `ScriptName` IN ('', 'npc_thrall_battle_for_undercity')
      AND `position_x` BETWEEN 1929.0 AND 1969.0
      AND `position_y` BETWEEN 210.0 AND 250.0 AND `position_z` BETWEEN 35.0 AND 55.0)
    OR
    (`id` = 32303 AND `ScriptName` IN ('', 'npc_varian_battle_for_undercity')
      AND `position_x` BETWEEN 1788.0 AND 1828.0
      AND `position_y` BETWEEN 435.0 AND 475.0 AND `position_z` BETWEEN 5.0 AND 25.0)
);
