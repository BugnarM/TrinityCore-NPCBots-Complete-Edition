-- Populate the Horde assault's route from the lower lift toward the Royal Quarter.
-- Coordinates were checked against the realm's vmap floors and walking mesh.
INSERT INTO `creature` (`id`, `map`, `zoneId`, `areaId`, `spawnMask`, `phaseMask`,
    `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`, `StringId`)
SELECT pack.`entry`, 0, 85, 1497, 1, 64, pack.`x`, pack.`y`, pack.`z`, pack.`o`, 180, pack.`spawn_key`
FROM (
    SELECT 31278 AS `entry`, 1592.0 AS `x`, 243.0 AS `y`, -52.14 AS `z`, 3.55 AS `o`,
        'undercity_assault_trade_quarter_01' AS `spawn_key`
    UNION ALL SELECT 31278, 1594.0, 244.0, -52.14, 3.55, 'undercity_assault_trade_quarter_02'
    UNION ALL SELECT 32772, 1596.0, 244.0, -52.14, 3.55, 'undercity_assault_trade_quarter_03'
    UNION ALL SELECT 31278, 1536.0, 277.0, -62.17, 0.15, 'undercity_assault_lower_corridor_01'
    UNION ALL SELECT 32772, 1538.0, 277.0, -62.17, 0.15, 'undercity_assault_lower_corridor_02'
    UNION ALL SELECT 31278, 1536.0, 279.0, -62.17, 0.15, 'undercity_assault_lower_corridor_03'
    UNION ALL SELECT 31278, 1466.0, 265.0, -62.17, 4.80, 'undercity_assault_inner_corridor_01'
    UNION ALL SELECT 32772, 1468.0, 265.0, -62.17, 4.80, 'undercity_assault_inner_corridor_02'
    UNION ALL SELECT 31278, 1470.0, 264.0, -62.17, 4.80, 'undercity_assault_inner_corridor_03'
) AS pack
WHERE NOT EXISTS (
    SELECT 1 FROM `creature` AS existing
    WHERE existing.`id` = pack.`entry` AND existing.`map` = 0
      AND existing.`StringId` = pack.`spawn_key`
);
