-- ==============================================================================
-- 2026_11_20_08_undercity_war_atmosphere.sql
-- Battle for the Undercity: Authentic Siege, Persistent Canal Gas, Interior Mobs & Mini-Bosses
-- Target: TrinityCore 3.3.5a (client build 12340)
-- Phase: 64 (Undercity Siege Phase)
-- ==============================================================================

START TRANSACTION;

-- 1. Clean up any misplaced bombers, flying machines, or visible bunny triggers
DELETE FROM `creature` WHERE `phaseMask` = 64 AND `id` IN (31838, 31637, 32214, 32215, 32217, 32221);

-- 2. Configure Factions and Health for Interior Mobs and Mini-Bosses
UPDATE `creature_template`
SET `faction` = 16, `minlevel` = 80, `maxlevel` = 80, `HealthModifier` = 12, `unit_flags` = 0
WHERE `entry` = 32271; -- Legion Overlord (Horde Mini-Boss)

UPDATE `creature_template`
SET `faction` = 14, `minlevel` = 80, `maxlevel` = 80, `HealthModifier` = 10, `unit_flags` = 0
WHERE `entry` = 32735; -- Alchemist Burroughs (Alliance Mini-Boss)

UPDATE `creature_template`
SET `faction` = 16, `minlevel` = 78, `maxlevel` = 80, `HealthModifier` = 4, `unit_flags` = 0
WHERE `entry` = 31692; -- Reanimated Abomination

UPDATE `creature_template`
SET `faction` = 14, `minlevel` = 75, `maxlevel` = 78, `HealthModifier` = 3, `unit_flags` = 0
WHERE `entry` = 31844; -- Blight Aberration

UPDATE `creature_template`
SET `faction` = 16, `minlevel` = 75, `maxlevel` = 76, `HealthModifier` = 2, `unit_flags` = 0
WHERE `entry` IN (32269, 32270); -- Legion Invader & Dreadwhisperer

UPDATE `creature_template`
SET `faction` = 14, `minlevel` = 75, `maxlevel` = 76, `HealthModifier` = 2, `unit_flags` = 0
WHERE `entry` IN (31482, 32396); -- Apothecary Chemist & Collaborator

-- 3. Exterior Siege Vehicles and Frontline Defenders (Tirisfal Glades, Area 85, Phase 64)
INSERT INTO `creature` (`id`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`,
    `position_x`, `position_y`, `position_z`, `orientation`, `curhealth`, `curmana`, `npcflag`)
SELECT event_spawn.`id`, 0, 85, event_spawn.`areaId`, 1, 64,
    event_spawn.`x`, event_spawn.`y`, event_spawn.`z`, event_spawn.`o`,
    event_spawn.`health`, event_spawn.`mana`, 0
FROM (
    -- Horde Demolishers & Blight Wagons on slopes bombarding the upper walls
    SELECT 31652 AS `id`, 153 AS `areaId`, 1936.0 AS `x`, 215.0 AS `y`, 45.5 AS `z`, 2.80 AS `o`, 150000 AS `health`, 0 AS `mana`
    UNION ALL SELECT 31652, 153, 1934.0, 258.0, 45.5, 3.40, 150000, 0
    UNION ALL SELECT 31566, 153, 1922.0, 218.0, 50.0, 2.80, 80000, 0
    UNION ALL SELECT 31566, 153, 1920.0, 255.0, 50.0, 3.40, 80000, 0
    -- Kor'kron Vanguard ready outside
    UNION ALL SELECT 31832, 153, 1950.0, 242.0, 44.25, 3.14, 45000, 0
    UNION ALL SELECT 31832, 153, 1950.0, 238.0, 44.25, 3.14, 45000, 0
    UNION ALL SELECT 31833, 153, 1948.0, 245.0, 44.25, 3.14, 60000, 0
    UNION ALL SELECT 31833, 153, 1948.0, 235.0, 44.25, 3.14, 60000, 0
    -- Alliance Steam Tanks outside sewer breach
    UNION ALL SELECT 31638, 153, 1810.0, 465.0, 15.5, 4.50, 120000, 0
    UNION ALL SELECT 31638, 153, 1815.0, 442.0, 15.0, 4.90, 120000, 0
    -- 7th Legion Vanguard
    UNION ALL SELECT 27160, 153, 1816.0, 456.0, 15.5, 4.70, 45000, 0
    UNION ALL SELECT 27160, 153, 1816.0, 450.0, 15.5, 4.70, 45000, 0
) AS event_spawn
WHERE NOT EXISTS (
    SELECT 1 FROM `creature` AS existing
    WHERE existing.`id` = event_spawn.`id` AND existing.`map` = 0
      AND existing.`phaseMask` = 64
      AND ABS(existing.`position_x` - event_spawn.`x`) < 3
      AND ABS(existing.`position_y` - event_spawn.`y`) < 3
);

-- 4. Undercity Interior (Area 1497, Phase 64): Canal Gas, Corridor Packs & Mini-Bosses
INSERT INTO `creature` (`id`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`,
    `position_x`, `position_y`, `position_z`, `orientation`, `curhealth`, `curmana`, `npcflag`)
SELECT int_spawn.`id`, 0, 85, 1497, 1, 64,
    int_spawn.`x`, int_spawn.`y`, int_spawn.`z`, int_spawn.`o`,
    int_spawn.`health`, int_spawn.`mana`, 0
FROM (
    -- --- Horde Corridor Packs ---
    -- Checkpoint 1 Pack: Abomination + Demons
    SELECT 31692 AS `id`, 1596.0 AS `x`, 242.0 AS `y`, -52.14 AS `z`, 4.0 AS `o`, 45000 AS `health`, 0 AS `mana`
    UNION ALL SELECT 32269, 1592.0, 246.0, -52.14, 4.0, 28000, 0
    UNION ALL SELECT 32269, 1590.0, 242.0, -52.14, 4.0, 28000, 0
    -- Checkpoint 2 Pack: Dreadwhisperers + Abomination
    UNION ALL SELECT 31692, 1632.0, 206.0, -54.76, 3.5, 45000, 0
    UNION ALL SELECT 32270, 1636.0, 202.0, -54.76, 3.5, 28000, 0
    UNION ALL SELECT 32270, 1630.0, 200.0, -54.76, 3.5, 28000, 0
    -- Checkpoint 3 Pack: Infiltrators & Demons
    UNION ALL SELECT 32269, 1515.0, 183.0, -61.85, 2.8, 28000, 0
    UNION ALL SELECT 32270, 1511.0, 187.0, -61.85, 2.8, 28000, 0
    UNION ALL SELECT 31692, 1510.0, 182.0, -61.85, 2.8, 45000, 0
    -- Checkpoint 4 (Royal Quarter Gate): MINI-BOSS Legion Overlord + 2 Abominations
    UNION ALL SELECT 32271, 1395.0, 275.0, -62.10, 2.5, 120000, 0 -- Legion Overlord (Mini-Boss)
    UNION ALL SELECT 31692, 1398.0, 278.0, -62.10, 2.5, 45000, 0
    UNION ALL SELECT 31692, 1392.0, 272.0, -62.10, 2.5, 45000, 0

    -- --- Alliance Canal Packs ---
    -- Sewer Checkpoint 1 Pack: Chemist + Blight Aberration
    UNION ALL SELECT 31482, 1773.0, 416.0, -57.14, 1.2, 28000, 0
    UNION ALL SELECT 31844, 1777.0, 420.0, -57.14, 1.2, 35000, 0
    -- Sewer Checkpoint 2 Pack: Collaborator + Aberration
    UNION ALL SELECT 32396, 1691.0, 420.0, -62.21, 1.0, 28000, 0
    UNION ALL SELECT 31844, 1695.0, 424.0, -62.21, 1.0, 35000, 0
    -- Sewer Checkpoint 3 (Before Apothecarium): MINI-BOSS Alchemist Burroughs + 2 Aberrations
    UNION ALL SELECT 32735, 1585.0, 365.0, -62.08, 0.8, 100000, 0 -- Alchemist Burroughs (Mini-Boss)
    UNION ALL SELECT 31844, 1588.0, 368.0, -62.08, 0.8, 35000, 0
    UNION ALL SELECT 31844, 1582.0, 362.0, -62.08, 0.8, 35000, 0

    -- --- Persistent Undercity Canal Gas Emitters (Model 169 invisible triggers) ---
    UNION ALL SELECT 22515, 1600.0, 210.0, -54.00, 0.0, 100, 0
    UNION ALL SELECT 22515, 1550.0, 175.0, -60.00, 0.0, 100, 0
    UNION ALL SELECT 22515, 1480.0, 200.0, -62.00, 0.0, 100, 0
    UNION ALL SELECT 22515, 1420.0, 250.0, -62.00, 0.0, 100, 0
    UNION ALL SELECT 22515, 1360.0, 290.0, -62.50, 0.0, 100, 0
    UNION ALL SELECT 22515, 1750.0, 415.0, -58.00, 0.0, 100, 0
    UNION ALL SELECT 22515, 1700.0, 420.0, -62.00, 0.0, 100, 0
    UNION ALL SELECT 22515, 1650.0, 395.0, -62.00, 0.0, 100, 0
    UNION ALL SELECT 22515, 1600.0, 370.0, -62.00, 0.0, 100, 0
) AS int_spawn
WHERE NOT EXISTS (
    SELECT 1 FROM `creature` AS existing
    WHERE existing.`id` = int_spawn.`id` AND existing.`map` = 0
      AND existing.`phaseMask` = 64
      AND ABS(existing.`position_x` - int_spawn.`x`) < 3
      AND ABS(existing.`position_y` - int_spawn.`y`) < 3
);

-- Set aura 23861 (persistent poison gas cloud visual) on all interior gas triggers
UPDATE `creature_template_addon`
SET `auras` = '23861'
WHERE `entry` = 22515;

COMMIT;
