-- Southern Durotar & Troll Starting Area NPCBot Wanderer Nodes & Spawns (Zone 14, Map 1)
-- Enable active bot spawning and Horde flags for all troll starting hubs, villages, and isles
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 5 WHERE `id` = 29;   -- The Den (Exact Troll/Orc spawn cavern!)
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 5 WHERE `id` = 1431; -- Valley of Trials 3
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 5 WHERE `id` = 1434; -- Valley of Trials 6
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 5 WHERE `id` = 1436; -- Valley of Trials 8
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 5 WHERE `id` = 31;   -- Sen'jin Village
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 5 WHERE `id` = 1419; -- Durotar South Hub 3 (Approach to beach)
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 5 WHERE `id` = 1420; -- Durotar South Hub 4 (Approach to Echo Isles)
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 5 WHERE `id` = 1421; -- Durotar South Hub 5 (Approach to Kolkar)
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 5 WHERE `id` = 1444; -- Echo Isles 1
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 5 WHERE `id` = 1445; -- Echo Isles 2
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 5 WHERE `id` = 1446; -- Echo Isles 3
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 5 WHERE `id` = 1447; -- Echo Isles 4
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 5 WHERE `id` = 1448; -- Echo Isles 5

-- Connect links properly
UPDATE `creature_template_npcbot_wander_nodes` SET `links` = '1431:0 1436:0' WHERE `id` = 29;
UPDATE `creature_template_npcbot_wander_nodes` SET `links` = '1413:0 1419:0 1420:0 1421:0 1422:0 62005:0' WHERE `id` = 31;
UPDATE `creature_template_npcbot_wander_nodes` SET `links` = '31:0 1413:0 1420:0 1444:0 62004:0' WHERE `id` = 1419;
UPDATE `creature_template_npcbot_wander_nodes` SET `links` = '31:0 1419:0 1421:0 1422:0 1445:0' WHERE `id` = 1420;
UPDATE `creature_template_npcbot_wander_nodes` SET `links` = '31:0 1413:0 1420:0 1422:0 62005:0' WHERE `id` = 1421;
UPDATE `creature_template_npcbot_wander_nodes` SET `links` = '31:0 1413:0 1420:0 1421:0 1423:0 62005:0' WHERE `id` = 1422;
UPDATE `creature_template_npcbot_wander_nodes` SET `links` = '1419:0 1445:0 1448:0 62003:0' WHERE `id` = 1444;
UPDATE `creature_template_npcbot_wander_nodes` SET `links` = '1420:0 1444:0 1446:0 62002:0' WHERE `id` = 1445;
UPDATE `creature_template_npcbot_wander_nodes` SET `links` = '1445:0 1447:0 1448:0 62001:0 62002:0' WHERE `id` = 1446;
UPDATE `creature_template_npcbot_wander_nodes` SET `links` = '1446:0 1448:0' WHERE `id` = 1447;
UPDATE `creature_template_npcbot_wander_nodes` SET `links` = '1444:0 1446:0 1447:0 62003:0' WHERE `id` = 1448;

-- Insert new wanderer nodes for Southern Durotar (Zalazane's Fall, Darkspear Strand, Coast Camp, Kolkar Crag)
DELETE FROM `creature_template_npcbot_wander_nodes` WHERE `id` BETWEEN 62001 AND 62005;
INSERT INTO `creature_template_npcbot_wander_nodes` (`id`, `name`, `mapid`, `zoneid`, `areaid`, `minlevel`, `maxlevel`, `minwaittime`, `maxwaittime`, `proximity`, `flags`, `x`, `y`, `z`, `o`, `links`) VALUES
(62001, 'ZalazanesFall', 1, 14, 368, 5, 10, 3000, 7000, 0, 5, -1553.21, -5350.96, 4.72, 0, '1446:0 62002:0'),
(62002, 'EchoIslesWestCoast', 1, 14, 368, 1, 10, 3000, 7000, 0, 4, -1400.0, -5250.0, 2.0, 0, '1445:0 1446:0 62001:0'),
(62003, 'DarkspearStrand', 1, 14, 368, 1, 10, 3000, 7000, 0, 5, -720.0, -5460.0, 1.5, 0, '1444:0 1448:0 62004:0'),
(62004, 'DarkspearCoastCamp', 1, 14, 393, 1, 10, 3000, 7000, 0, 5, -528.63, -5215.8, 12.18, 0, '1419:0 62003:0'),
(62005, 'KolkarCragSouth', 1, 14, 366, 1, 10, 3000, 7000, 0, 4, -1060.0, -4750.0, 16.0, 0, '1421:0 1422:0 31:0');
