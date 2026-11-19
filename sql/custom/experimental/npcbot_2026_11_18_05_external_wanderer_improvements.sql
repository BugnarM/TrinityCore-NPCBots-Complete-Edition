-- Experimental waypoint data. On the installed ground mmaps, the links
-- 4743–62423, 62423–62424, and 3664–62420 fail the bidirectional path check.
-- NPCBot Wanderer External Improvements & Tactical Navigation Optimization
-- Adds high-level patrols, mountain ramps, and dungeon/raid exterior hubs (Ulduar, Black Temple, Karazhan, Manaforge, Frenzyheart, Wintergarde).
DELETE FROM `creature_template_npcbot_wander_nodes` WHERE `id` BETWEEN 62400 AND 62424;

INSERT INTO `creature_template_npcbot_wander_nodes` (`id`, `name`, `mapid`, `zoneid`, `areaid`, `minlevel`, `maxlevel`, `minwaittime`, `maxwaittime`, `proximity`, `flags`, `x`, `y`, `z`, `o`, `links`) VALUES
(62400, 'Foot Steppes lower path 1', 571, 67, 4432, 76, 80, 500, 1600, 0, 524288, 7161.12, -1451.91, 923.633, 0, '4796:0 62401:0'),
(62401, 'Foot Steppes lower path 2', 571, 67, 4432, 76, 80, 500, 1600, 0, 524288, 7208.06, -1421.91, 914.407, 0, '4797:0 62400:0'),
(62402, 'Narvir ramp 1', 571, 67, 4435, 76, 80, 300, 1000, 0, 524288, 8242.13, -220.533, 906.856, 0, '4707:0 62403:0'),
(62403, 'Narvir ramp 2', 571, 67, 4435, 76, 80, 300, 1000, 0, 524288, 8288.27, -237.6, 922.856, 0, '4708:0 62402:0'),
(62404, 'Tunka ramp 1', 571, 67, 67, 76, 80, 300, 1000, 0, 524292, 7791.2, -2745.6, 1164.86, 0, '4744:0 4746:0'),
(62405, 'Kolramas ramp 1', 571, 66, 4299, 73, 77, 300, 1000, 0, 524288, 4988, -3625.07, 332.533, 0, '4489:0 62406:0'),
(62406, 'Kolramas ramp 2', 571, 66, 4299, 73, 77, 300, 1000, 0, 524288, 5063.47, -3607.73, 360.8, 0, '4490:0 62405:0'),
(62407, 'Frenzyheart patrol 1', 571, 3711, 4292, 74, 78, 1100, 3800, 0, 524288, 5183.33, 4483.33, -95.1338, 0, '4624:0 62408:0'),
(62408, 'Frenzyheart patrol 2', 571, 3711, 4292, 74, 78, 1100, 3800, 0, 524288, 5183.33, 4550, -96.4305, 0, '4625:0 62407:0'),
(62409, 'Rainspeaker patrol 1', 571, 3711, 4291, 74, 78, 1100, 3800, 0, 524288, 5616.67, 4616.67, -137.977, 0, '4666:0 62410:0'),
(62410, 'Rainspeaker patrol 2', 571, 3711, 4291, 74, 78, 1100, 3800, 0, 524288, 5683.33, 4616.67, -129.693, 0, '4665:0 62409:0'),
(62411, 'Karazhan exterior 1', 0, 41, 2562, 50, 60, 1100, 3800, 0, 524288, -11165, -2080, 48.3355, 0, '804:0 62412:0'),
(62412, 'Karazhan exterior 2', 0, 41, 2562, 50, 60, 1100, 3800, 0, 524288, -11105, -2060, 44.8612, 0, '805:0 62411:0'),
(62413, 'Wintergarde ground patrol 1', 571, 65, 4188, 70, 74, 1100, 3800, 0, 524288, 3710, -1090, 121.094, 0, '4162:0 62414:0'),
(62414, 'Wintergarde ground patrol 2', 571, 65, 4188, 70, 74, 1100, 3800, 0, 524288, 3780, -1055, 119.903, 0, '4163:0 62413:0'),
(62415, 'Manaforge Bnaar patrol 1', 530, 3523, 3726, 67, 70, 1100, 3800, 0, 524288, 2970, 4080, 151.347, 0, '3265:0 62416:0'),
(62416, 'Manaforge Bnaar patrol 2', 530, 3523, 3726, 67, 70, 1100, 3800, 0, 524288, 3030, 4080, 150.861, 0, '3263:0 62415:0'),
(62417, 'Ulduar exterior 1', 571, 67, 4445, 76, 80, 2200, 6800, 0, 1, 9040, -1290, 1060.29, 0, '62418:0 62419:0 62424:0'),
(62418, 'Ulduar exterior 2', 571, 67, 4445, 76, 80, 800, 2600, 0, 524288, 9040, -1205, 1058.59, 0, '62417:0 62419:0'),
(62419, 'Ulduar exterior 3', 571, 67, 4445, 76, 80, 1000, 3000, 0, 1, 9125, -1290, 1065.21, 0, '62417:0 62418:0'),
(62420, 'Black Temple exterior 1', 530, 3520, 3840, 67, 70, 2200, 6800, 0, 1, -3650, 116.67, 70.3253, 0, '62421:0 62422:0 3664:0'),
(62421, 'Black Temple exterior 2', 530, 3520, 3840, 67, 70, 800, 2600, 0, 524288, -3583.33, 183.33, 49.2033, 0, '62420:0 62422:0'),
(62422, 'Black Temple exterior 3', 530, 3520, 3840, 67, 70, 1000, 3000, 0, 1, -3583.33, 116.67, 72.9774, 0, '62420:0 62421:0'),
(62423, 'Ulduar Great Terrace Road', 571, 67, 4445, 76, 80, 1000, 3000, 0, 524288, 8550, -1250, 980, 0.5, '4743:0 62424:0'),
(62424, 'Ulduar Gate Approach Camp', 571, 67, 4445, 76, 80, 1000, 3000, 0, 524288, 8861.33, -1322.39, 1033.4, 0.7, '62423:0 62417:0');

-- Anchor Updates connecting existing continent nodes to new patrols:
UPDATE `creature_template_npcbot_wander_nodes` SET `links` = CONCAT(`links`, ' 62400:0') WHERE `id` = 4796 AND `links` NOT LIKE '%62400%';
UPDATE `creature_template_npcbot_wander_nodes` SET `links` = CONCAT(`links`, ' 62401:0') WHERE `id` = 4797 AND `links` NOT LIKE '%62401%';
UPDATE `creature_template_npcbot_wander_nodes` SET `links` = CONCAT(`links`, ' 62402:0') WHERE `id` = 4707 AND `links` NOT LIKE '%62402%';
UPDATE `creature_template_npcbot_wander_nodes` SET `links` = CONCAT(`links`, ' 62403:0') WHERE `id` = 4708 AND `links` NOT LIKE '%62403%';
UPDATE `creature_template_npcbot_wander_nodes` SET `links` = CONCAT(`links`, ' 62404:0') WHERE `id` = 4744 AND `links` NOT LIKE '%62404%';
UPDATE `creature_template_npcbot_wander_nodes` SET `links` = CONCAT(`links`, ' 62404:0') WHERE `id` = 4746 AND `links` NOT LIKE '%62404%';
UPDATE `creature_template_npcbot_wander_nodes` SET `links` = CONCAT(`links`, ' 62405:0') WHERE `id` = 4489 AND `links` NOT LIKE '%62405%';
UPDATE `creature_template_npcbot_wander_nodes` SET `links` = CONCAT(`links`, ' 62406:0') WHERE `id` = 4490 AND `links` NOT LIKE '%62406%';
UPDATE `creature_template_npcbot_wander_nodes` SET `links` = CONCAT(`links`, ' 62407:0') WHERE `id` = 4624 AND `links` NOT LIKE '%62407%';
UPDATE `creature_template_npcbot_wander_nodes` SET `links` = CONCAT(`links`, ' 62408:0') WHERE `id` = 4625 AND `links` NOT LIKE '%62408%';
UPDATE `creature_template_npcbot_wander_nodes` SET `links` = CONCAT(`links`, ' 62409:0') WHERE `id` = 4666 AND `links` NOT LIKE '%62409%';
UPDATE `creature_template_npcbot_wander_nodes` SET `links` = CONCAT(`links`, ' 62410:0') WHERE `id` = 4665 AND `links` NOT LIKE '%62410%';
UPDATE `creature_template_npcbot_wander_nodes` SET `links` = CONCAT(`links`, ' 62411:0') WHERE `id` = 804 AND `links` NOT LIKE '%62411%';
UPDATE `creature_template_npcbot_wander_nodes` SET `links` = CONCAT(`links`, ' 62412:0') WHERE `id` = 805 AND `links` NOT LIKE '%62412%';
UPDATE `creature_template_npcbot_wander_nodes` SET `links` = CONCAT(`links`, ' 62413:0') WHERE `id` = 4162 AND `links` NOT LIKE '%62413%';
UPDATE `creature_template_npcbot_wander_nodes` SET `links` = CONCAT(`links`, ' 62414:0') WHERE `id` = 4163 AND `links` NOT LIKE '%62414%';
UPDATE `creature_template_npcbot_wander_nodes` SET `links` = CONCAT(`links`, ' 62415:0') WHERE `id` = 3265 AND `links` NOT LIKE '%62415%';
UPDATE `creature_template_npcbot_wander_nodes` SET `links` = CONCAT(`links`, ' 62416:0') WHERE `id` = 3263 AND `links` NOT LIKE '%62416%';
UPDATE `creature_template_npcbot_wander_nodes` SET `links` = CONCAT(`links`, ' 62423:0') WHERE `id` = 4743 AND `links` NOT LIKE '%62423%';
UPDATE `creature_template_npcbot_wander_nodes` SET `links` = CONCAT(`links`, ' 62420:0') WHERE `id` = 3664 AND `links` NOT LIKE '%62420%';
