-- Fix Quest 4292 "The Bait for Lar'korwi"
-- 1. Ensure Preserved Threshadon Meat (spell 15118 / event 3980) respawns carcass AND summons Lar'korwi
-- 2. Ensure Preserved Pheromone Mixture (spell 15119 / event 3981) summons Lar'korwi
-- 3. Add SmartAI to Lar'korwi (entry 9684) so he runs and attacks the player upon summon

DELETE FROM `event_scripts` WHERE `id` IN (3980, 3981);
INSERT INTO `event_scripts` (`id`, `delay`, `command`, `datalong`, `datalong2`, `dataint`, `x`, `y`, `z`, `o`, `Comment`) VALUES
(3980, 0, 9, 27142, 300, 0, 0, 0, 0, 0, 'Respawn Preserved Threshadon Carcass for 5 minutes'),
(3980, 2, 10, 9684, 300000, 0, -7196.45, -2405.63, -217.28, 4.8, 'Summon Lar\'korwi when meat placed'),
(3981, 0, 10, 9684, 300000, 0, -7196.45, -2405.63, -217.28, 4.8, 'Summon Lar\'korwi when pheromones applied');

-- SmartAI for Lar'korwi (entry 9684)
DELETE FROM `smart_scripts` WHERE `entryorguid` = 9684 AND `source_type` = 0 AND `id` IN (2, 3, 4);
INSERT INTO `smart_scripts` (`entryorguid`, `source_type`, `id`, `link`, `event_type`, `event_phase_mask`, `event_chance`, `event_flags`, `event_param1`, `event_param2`, `event_param3`, `event_param4`, `event_param5`, `action_type`, `action_param1`, `action_param2`, `action_param3`, `action_param4`, `action_param5`, `action_param6`, `target_type`, `target_param1`, `target_param2`, `target_param3`, `target_param4`, `target_x`, `target_y`, `target_z`, `target_o`, `comment`) VALUES
(9684, 0, 2, 3, 54, 0, 100, 0, 0, 0, 0, 0, 0, 59, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 'Lar\'korwi - On Just Summoned - Set Run On'),
(9684, 0, 3, 0, 61, 0, 100, 0, 0, 0, 0, 0, 0, 49, 0, 0, 0, 0, 0, 0, 21, 80, 0, 0, 0, 0, 0, 0, 0, 'Lar\'korwi - On Just Summoned - Attack Closest Player'),
(9684, 0, 4, 0, 11, 0, 100, 0, 0, 0, 0, 0, 0, 49, 0, 0, 0, 0, 0, 0, 21, 80, 0, 0, 0, 0, 0, 0, 0, 'Lar\'korwi - On Respawn - Attack Closest Player');
