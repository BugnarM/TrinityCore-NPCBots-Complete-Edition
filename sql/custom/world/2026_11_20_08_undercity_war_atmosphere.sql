-- ==============================================================================
-- 2026_11_20_08_undercity_war_atmosphere.sql
-- Battle for the Undercity: Siege Atmosphere, War Machinery, Troop Formations & Banners
-- Target: TrinityCore 3.3.5a (client build 12340)
-- Phase: 64 (Undercity Siege Phase) in Tirisfal Glades (area 85)
-- ==============================================================================

START TRANSACTION;

-- 1. Ensure creature templates have correct combat/visibility configuration
UPDATE `creature_template`
SET `faction` = 106, `minlevel` = 80, `maxlevel` = 80
WHERE `entry` IN (31832, 31833);

UPDATE `creature_template`
SET `faction` = 29, `minlevel` = 80, `maxlevel` = 80, `HealthModifier` = 15
WHERE `entry` = 31652;

UPDATE `creature_template`
SET `faction` = 35, `minlevel` = 80, `maxlevel` = 80
WHERE `entry` = 31566;

UPDATE `creature_template`
SET `faction` = 1802, `minlevel` = 75, `maxlevel` = 80
WHERE `entry` IN (31638, 31637);

UPDATE `creature_template`
SET `faction` = 1802, `minlevel` = 75, `maxlevel` = 80
WHERE `entry` = 27160;

-- 2. Spawn Horde War Formations, Demolishers, Banners, and Aerial Bombers in Phase 64
INSERT INTO `creature` (`id`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`,
    `position_x`, `position_y`, `position_z`, `orientation`, `curhealth`, `curmana`, `npcflag`)
SELECT event_spawn.`id`, 0, 85, event_spawn.`areaId`, 1, 64,
    event_spawn.`x`, event_spawn.`y`, event_spawn.`z`, event_spawn.`o`,
    event_spawn.`health`, event_spawn.`mana`, 0
FROM (
    -- Kor'kron Infantry Formation outside Ruins of Lordaeron (Rank 1)
    SELECT 31832 AS `id`, 153 AS `areaId`, 1950.0 AS `x`, 245.0 AS `y`, 44.25 AS `z`, 3.14 AS `o`, 45000 AS `health`, 0 AS `mana`
    UNION ALL SELECT 31832, 153, 1950.0, 242.0, 44.25, 3.14, 45000, 0
    UNION ALL SELECT 31832, 153, 1950.0, 239.0, 44.25, 3.14, 45000, 0
    UNION ALL SELECT 31832, 153, 1950.0, 236.0, 44.25, 3.14, 45000, 0
    -- Kor'kron Infantry Formation (Rank 2)
    UNION ALL SELECT 31832, 153, 1953.0, 245.0, 44.25, 3.14, 45000, 0
    UNION ALL SELECT 31832, 153, 1953.0, 242.0, 44.25, 3.14, 45000, 0
    UNION ALL SELECT 31832, 153, 1953.0, 239.0, 44.25, 3.14, 45000, 0
    UNION ALL SELECT 31832, 153, 1953.0, 236.0, 44.25, 3.14, 45000, 0
    -- Kor'kron Squad Leaders flanking formation
    UNION ALL SELECT 31833, 153, 1947.0, 248.0, 44.25, 3.14, 60000, 0
    UNION ALL SELECT 31833, 153, 1947.0, 232.0, 44.25, 3.14, 60000, 0
    -- Horde Demolishers positioned on ridges
    UNION ALL SELECT 31652, 153, 1936.0, 215.0, 45.5, 2.80, 150000, 0
    UNION ALL SELECT 31652, 153, 1934.0, 258.0, 45.5, 3.40, 150000, 0
    -- Blight Wagons stationed on perimeter
    UNION ALL SELECT 31566, 153, 1922.0, 218.0, 50.0, 2.80, 80000, 0
    UNION ALL SELECT 31566, 153, 1920.0, 255.0, 50.0, 3.40, 80000, 0
    -- Horde War Banners along the courtyard road
    UNION ALL SELECT 32214, 153, 1940.0, 248.0, 45.0, 3.14, 100, 0
    UNION ALL SELECT 32214, 153, 1940.0, 230.0, 45.0, 3.14, 100, 0
    UNION ALL SELECT 32215, 153, 1925.0, 248.0, 50.0, 3.14, 100, 0
    UNION ALL SELECT 32215, 153, 1925.0, 228.0, 50.0, 3.14, 100, 0
    UNION ALL SELECT 32214, 153, 1890.0, 248.0, 58.0, 3.14, 100, 0
    UNION ALL SELECT 32214, 153, 1890.0, 228.0, 58.0, 3.14, 100, 0
    -- Horde Infra-green Bomber hovering in the sky over the Lordaeron ruins
    UNION ALL SELECT 31838, 153, 1850.0, 238.0, 88.0, 3.14, 250000, 0
    -- ==============================================================================
    -- Alliance Sewer Breach Camp & Vanguard Spawns (Phase 64)
    -- ==============================================================================
    -- 7th Legion Infantry guarding sewer breach
    UNION ALL SELECT 27160, 153, 1816.0, 458.0, 15.5, 4.70, 45000, 0
    UNION ALL SELECT 27160, 153, 1816.0, 452.0, 15.5, 4.70, 45000, 0
    UNION ALL SELECT 27160, 153, 1819.0, 458.0, 15.5, 4.70, 45000, 0
    UNION ALL SELECT 27160, 153, 1819.0, 452.0, 15.5, 4.70, 45000, 0
    -- Alliance Siege Vehicles (Steam Tanks) near sewer threshold
    UNION ALL SELECT 31638, 153, 1810.0, 465.0, 15.5, 4.50, 120000, 0
    UNION ALL SELECT 31638, 153, 1815.0, 442.0, 15.0, 4.90, 120000, 0
    -- Alliance Flying Machine hovering above sewer entrance
    UNION ALL SELECT 31637, 153, 1815.0, 448.0, 28.0, 4.70, 60000, 0
    -- Alliance War Banners
    UNION ALL SELECT 32217, 153, 1808.0, 456.0, 15.0, 4.70, 100, 0
    UNION ALL SELECT 32221, 153, 1814.0, 460.0, 15.5, 4.70, 100, 0
) AS event_spawn
WHERE NOT EXISTS (
    SELECT 1 FROM `creature` AS existing
    WHERE existing.`id` = event_spawn.`id` AND existing.`map` = 0
      AND existing.`phaseMask` = 64
      AND ABS(existing.`position_x` - event_spawn.`x`) < 2
      AND ABS(existing.`position_y` - event_spawn.`y`) < 2
);

COMMIT;
