-- Southern Durotar & Troll Starting Area NPCBot Wanderer Nodes & Spawns (Zone 14, Map 1)
-- Enable active bot spawning and Horde flags for existing key hubs
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 5 WHERE `id` = 31; -- Sen'jin Village (SPAWN | HORDE_ONLY)
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 5 WHERE `id` = 1431; -- Valley of Trials 3 (SPAWN | HORDE_ONLY)
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 5 WHERE `id` = 1446; -- Echo Isles 3 (SPAWN | HORDE_ONLY)

-- Update links on existing nodes to connect with the new southern points
UPDATE `creature_template_npcbot_wander_nodes` SET `links` = '1413:0 1419:0 1420:0 1421:0 1422:0 62005:0' WHERE `id` = 31;
UPDATE `creature_template_npcbot_wander_nodes` SET `links` = '31:0 1413:0 1420:0 1444:0 62004:0' WHERE `id` = 1419;
UPDATE `creature_template_npcbot_wander_nodes` SET `links` = '31:0 1413:0 1420:0 1422:0 62005:0' WHERE `id` = 1421;
UPDATE `creature_template_npcbot_wander_nodes` SET `links` = '31:0 1413:0 1420:0 1421:0 1423:0 62005:0' WHERE `id` = 1422;
UPDATE `creature_template_npcbot_wander_nodes` SET `links` = '1419:0 1445:0 1448:0 62003:0' WHERE `id` = 1444;
UPDATE `creature_template_npcbot_wander_nodes` SET `links` = '1420:0 1444:0 1446:0 62002:0' WHERE `id` = 1445;
UPDATE `creature_template_npcbot_wander_nodes` SET `links` = '1445:0 1447:0 1448:0 62001:0 62002:0' WHERE `id` = 1446;
UPDATE `creature_template_npcbot_wander_nodes` SET `links` = '1444:0 1446:0 1447:0 62003:0' WHERE `id` = 1448;

-- Insert new wanderer nodes for Southern Durotar (Zalazane's Fall, Darkspear Strand, Coast Camp, Kolkar Crag)
DELETE FROM `creature_template_npcbot_wander_nodes` WHERE `id` BETWEEN 62001 AND 62005;
INSERT INTO `creature_template_npcbot_wander_nodes` (`id`, `name`, `mapid`, `zoneid`, `areaid`, `minlevel`, `maxlevel`, `minwaittime`, `maxwaittime`, `proximity`, `flags`, `x`, `y`, `z`, `o`, `links`) VALUES
(62001, 'ZalazanesFall', 1, 14, 368, 5, 10, 3000, 7000, 0, 5, -1553.21, -5350.96, 4.72, 0, '1446:0 62002:0'),
(62002, 'EchoIslesWestCoast', 1, 14, 368, 1, 10, 3000, 7000, 0, 4, -1400.0, -5250.0, 2.0, 0, '1445:0 1446:0 62001:0'),
(62003, 'DarkspearStrand', 1, 14, 368, 1, 10, 3000, 7000, 0, 5, -720.0, -5460.0, 1.5, 0, '1444:0 1448:0 62004:0'),
(62004, 'DarkspearCoastCamp', 1, 14, 393, 1, 10, 3000, 7000, 0, 5, -528.63, -5215.8, 12.18, 0, '1419:0 62003:0'),
(62005, 'KolkarCragSouth', 1, 14, 366, 1, 10, 3000, 7000, 0, 4, -1060.0, -4750.0, 16.0, 0, '1421:0 1422:0 31:0');
