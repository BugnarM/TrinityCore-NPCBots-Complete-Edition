-- Use the in-game placements beside the Horde enchanting trainers.
START TRANSACTION;

-- Orgrimmar
UPDATE `creature`
SET `position_x` = 1912.595703125, `position_y` = -4433.9072265625,
    `position_z` = 24.800960540771484, `orientation` = 5.876339435577393
WHERE `guid` = 4001205 AND `id` = 70903 AND `map` = 1;

-- Thunder Bluff
UPDATE `creature`
SET `position_x` = -1106.623046875, `position_y` = 49.23877716064453,
    `position_z` = 140.5734100341797, `orientation` = 3.5979104042053223
WHERE `guid` = 4001206 AND `id` = 70903 AND `map` = 1;

-- Undercity
UPDATE `creature`
SET `position_x` = 1482.4638671875, `position_y` = 276.8973388671875,
    `position_z` = -62.15949249267578, `orientation` = 0.22696344554424286
WHERE `guid` = 4001207 AND `id` = 70903 AND `map` = 0;

COMMIT;
