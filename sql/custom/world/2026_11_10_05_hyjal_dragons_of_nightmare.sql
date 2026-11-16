-- Relocate the 4 Dragons of Nightmare to the new Hyjal Arena
-- Spaced side by side facing south towards the arena center

-- Ysondre (Entry 14887)
UPDATE `creature` SET 
    `map` = 1, `zoneId` = 616, `areaId` = 616, 
    `position_x` = 4675.0, `position_y` = -3700.0, `position_z` = 952.57, 
    `orientation` = 4.712, `spawntimesecs` = 300, `curhealth` = 1895000, 
    `MovementType` = 0 
WHERE `guid` = 32343;

-- Lethon (Entry 14888)
UPDATE `creature` SET 
    `map` = 1, `zoneId` = 616, `areaId` = 616, 
    `position_x` = 4688.0, `position_y` = -3700.0, `position_z` = 953.38, 
    `orientation` = 4.712, `spawntimesecs` = 300, `curhealth` = 1895000, 
    `MovementType` = 0 
WHERE `guid` = 52350;

-- Emeriss (Entry 14889)
UPDATE `creature` SET 
    `map` = 1, `zoneId` = 616, `areaId` = 616, 
    `position_x` = 4702.0, `position_y` = -3700.0, `position_z` = 954.69, 
    `orientation` = 4.712, `spawntimesecs` = 300, `curhealth` = 1895000, 
    `MovementType` = 0 
WHERE `guid` = 50012;

-- Taerar (Entry 14890)
UPDATE `creature` SET 
    `map` = 1, `zoneId` = 616, `areaId` = 616, 
    `position_x` = 4715.0, `position_y` = -3700.0, `position_z` = 956.24, 
    `orientation` = 4.712, `spawntimesecs` = 300, `curhealth` = 1895000, 
    `MovementType` = 0 
WHERE `guid` = 4256;

-- Clear old Duskwood path from Taerar's addon
UPDATE `creature_addon` SET `path_id` = 0 WHERE `guid` = 4256;
