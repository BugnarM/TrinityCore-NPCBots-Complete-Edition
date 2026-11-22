-- Capital Enchanter, one per capital and one in each Shattrath faction quarter.
START TRANSACTION;

INSERT INTO `creature_template`
    (`entry`, `name`, `subname`, `modelid1`, `minlevel`, `maxlevel`, `faction`, `npcflag`,
     `unit_class`, `unit_flags`, `BaseAttackTime`, `RangeAttackTime`, `type`, `flags_extra`, `ScriptName`)
VALUES (70903, 'Aelrin Runethread', 'Enchanting Services', 16776, 75, 75, 35, 1, 1, 2, 2000, 2000, 7, 2, 'npc_capital_enchanter')
ON DUPLICATE KEY UPDATE `name` = VALUES(`name`), `subname` = VALUES(`subname`),
    `modelid1` = VALUES(`modelid1`), `faction` = VALUES(`faction`), `npcflag` = VALUES(`npcflag`),
    `ScriptName` = VALUES(`ScriptName`);

INSERT INTO `npc_text` (`ID`, `text0_0`, `text0_1`, `Probability0`)
VALUES (70903, 'I can enchant compatible pieces of your equipped gear. Choose a piece to see your options and their prices.',
    'I can enchant compatible pieces of your equipped gear. Choose a piece to see your options and their prices.', 1)
ON DUPLICATE KEY UPDATE `text0_0` = VALUES(`text0_0`), `text0_1` = VALUES(`text0_1`), `Probability0` = VALUES(`Probability0`);

-- The offsets are measured from the trainers and face into their rooms.
INSERT INTO `creature`
    (`guid`, `id`, `map`, `spawnMask`, `phaseMask`, `position_x`, `position_y`, `position_z`, `orientation`, `spawntimesecs`)
VALUES
    (4001201, 70903,   0, 1, 1, -8855.92653,    802.40039,   96.5175, 5.35816, 120), -- Stormwind, Lucan Cordell
    (4001202, 70903,   0, 1, 1, -4811.37709,  -1187.52649,  512.564,  3.26377, 120), -- Ironforge, Gimble Thistlefuzz
    (4001203, 70903,   1, 1, 1, 10146.13925,   2322.97581, 1333.08,   0.99484, 120), -- Darnassus, Taladan
    (4001204, 70903, 530, 1, 1, -3884.20540, -11494.32888, -136.057,  1.67552, 120), -- The Exodar, Nahogg
    (4001205, 70903,   1, 1, 1,  1909.47370,  -4436.97816,   24.8823, 2.86234, 120), -- Orgrimmar, Godan
    (4001206, 70903,   1, 1, 1, -1108.59587,     46.39753,  140.521,  5.28835, 120), -- Thunder Bluff, Teg Dawnstrider
    (4001207, 70903,   0, 1, 1,  1479.94100,    272.63869,  -62.0769, 3.19395, 120), -- Undercity, Lavinia Crowe
    (4001208, 70903, 530, 1, 1,  9959.90132,  -7246.62021,   32.1599, 2.42601, 120), -- Silvermoon, Sedana
    (4001209, 70903, 530, 1, 1, -2276.32931,   5568.90943,   79.9269, 4.31096, 120), -- Shattrath, Enchantress Volali (Scryers)
    (4001210, 70903, 571, 1, 1,  5844.80486,    726.19136,  642.08,   3.66519, 120), -- Dalaran, Enchanter Nalthanis
    (4001211, 70903, 530, 1, 1, -1861.33531,   5657.92162,  127.459,  2.79598, 120)  -- Shattrath, Zurii (Aldor)
ON DUPLICATE KEY UPDATE `id` = VALUES(`id`), `map` = VALUES(`map`), `spawnMask` = VALUES(`spawnMask`),
    `phaseMask` = VALUES(`phaseMask`), `position_x` = VALUES(`position_x`), `position_y` = VALUES(`position_y`),
    `position_z` = VALUES(`position_z`), `orientation` = VALUES(`orientation`);

COMMIT;
