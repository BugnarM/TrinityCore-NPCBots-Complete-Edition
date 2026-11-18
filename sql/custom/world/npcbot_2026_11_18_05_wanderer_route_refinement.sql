-- Bidirectional routes validated on TrinityCore 3.3.5a ground mmaps.
-- Add safe approaches, bypasses and ramp turns while preserving existing links.
CREATE TEMPORARY TABLE npcbot_wander_refinement_guard (valid INT NOT NULL);
START TRANSACTION;
INSERT INTO npcbot_wander_refinement_guard
SELECT CASE WHEN COUNT(*) = 30 THEN 1 ELSE NULL END
FROM creature_template_npcbot_wander_nodes
WHERE (id = 804 AND mapid = 0 AND zoneid = 41) OR
       (id = 805 AND mapid = 0 AND zoneid = 41) OR
       (id = 1436 AND mapid = 1 AND zoneid = 14) OR
       (id = 3263 AND mapid = 530 AND zoneid = 3523) OR
       (id = 3265 AND mapid = 530 AND zoneid = 3523) OR
       (id = 3831 AND mapid = 571 AND zoneid = 65) OR
       (id = 4063 AND mapid = 571 AND zoneid = 65) OR
       (id = 4064 AND mapid = 571 AND zoneid = 65) OR
       (id = 4162 AND mapid = 571 AND zoneid = 65) OR
       (id = 4163 AND mapid = 571 AND zoneid = 65) OR
       (id = 4228 AND mapid = 571 AND zoneid = 65) OR
       (id = 4489 AND mapid = 571 AND zoneid = 66) OR
       (id = 4490 AND mapid = 571 AND zoneid = 66) OR
       (id = 4558 AND mapid = 571 AND zoneid = 66) OR
       (id = 4560 AND mapid = 571 AND zoneid = 66) OR
       (id = 4624 AND mapid = 571 AND zoneid = 3711) OR
       (id = 4625 AND mapid = 571 AND zoneid = 3711) OR
       (id = 4665 AND mapid = 571 AND zoneid = 3711) OR
       (id = 4666 AND mapid = 571 AND zoneid = 3711) OR
       (id = 4667 AND mapid = 571 AND zoneid = 3711) OR
       (id = 4707 AND mapid = 571 AND zoneid = 67) OR
       (id = 4708 AND mapid = 571 AND zoneid = 67) OR
       (id = 4744 AND mapid = 571 AND zoneid = 67) OR
       (id = 4746 AND mapid = 571 AND zoneid = 67) OR
       (id = 4796 AND mapid = 571 AND zoneid = 67) OR
       (id = 4797 AND mapid = 571 AND zoneid = 67) OR
       (id = 4970 AND mapid = 571 AND zoneid = 210) OR
       (id = 4972 AND mapid = 571 AND zoneid = 210) OR
       (id = 5608 AND mapid = 530 AND zoneid = 3433) OR
       (id = 5612 AND mapid = 530 AND zoneid = 3433);

INSERT INTO creature_template_npcbot_wander_nodes
    (id, name, mapid, zoneid, areaid, minlevel, maxlevel, minwaittime, maxwaittime, proximity, flags, x, y, z, o, links)
VALUES
    (62400, 'Foot Steppes lower path 1', 571, 67, 4432, 76, 80, 500, 1600, 0, 524288, 7161.12, -1451.91, 923.633, 0, '4796:0 62401:0'),
    (62401, 'Foot Steppes lower path 2', 571, 67, 4432, 76, 80, 500, 1600, 0, 524288, 7208.06, -1421.91, 914.407, 0, '4797:0 62400:0'),
    (62402, 'Narvir ramp 1', 571, 67, 4435, 76, 80, 300, 1000, 0, 524288, 8242.13, -220.533, 906.856, 0, '4707:0 62403:0'),
    (62403, 'Narvir ramp 2', 571, 67, 4435, 76, 80, 300, 1000, 0, 524288, 8288.27, -237.6, 922.856, 0, '4708:0 62402:0'),
    (62404, 'Tunka ramp 1', 571, 67, 67, 76, 80, 300, 1000, 0, 524292, 7791.2, -2745.6, 1164.86, 0, '4744:0 4746:0'),
    (62405, 'Kolramas ramp 1', 571, 66, 4299, 73, 77, 300, 1000, 0, 524288, 4988, -3625.07, 332.533, 0, '4489:0 62406:0'),
    (62406, 'Kolramas ramp 2', 571, 66, 4299, 73, 77, 300, 1000, 0, 524288, 5063.47, -3607.73, 360.8, 0, '4490:0 62405:0'),
    (62407, 'Frenzyheart patrol 1', 571, 3711, 4292, 74, 78, 1100, 3800, 0, 524288, 5183.33, 4483.33, -95.1338, 0, '4624:0 62408:0'),
    (62408, 'Frenzyheart patrol 2', 571, 3711, 4292, 74, 78, 1100, 3800, 0, 524288, 5183.33, 4550.0, -96.4305, 0, '4625:0 62407:0'),
    (62409, 'Rainspeaker patrol 1', 571, 3711, 4291, 74, 78, 1100, 3800, 0, 524288, 5616.67, 4616.67, -137.977, 0, '4666:0 62410:0'),
    (62410, 'Rainspeaker patrol 2', 571, 3711, 4291, 74, 78, 1100, 3800, 0, 524288, 5683.33, 4616.67, -129.693, 0, '4665:0 62409:0'),
    (62411, 'Karazhan exterior 1', 0, 41, 2562, 50, 60, 1100, 3800, 0, 524288, -11165.0, -2080.0, 48.3355, 0, '804:0 62412:0'),
    (62412, 'Karazhan exterior 2', 0, 41, 2562, 50, 60, 1100, 3800, 0, 524288, -11105.0, -2060.0, 44.8612, 0, '805:0 62411:0'),
    (62413, 'Wintergarde ground patrol 1', 571, 65, 4188, 70, 74, 1100, 3800, 0, 524288, 3710.0, -1090.0, 121.094, 0, '4162:0 62414:0'),
    (62414, 'Wintergarde ground patrol 2', 571, 65, 4188, 70, 74, 1100, 3800, 0, 524288, 3780.0, -1055.0, 119.903, 0, '4163:0 62413:0'),
    (62415, 'Manaforge Bnaar patrol 1', 530, 3523, 3726, 67, 70, 1100, 3800, 0, 524288, 2970.0, 4080.0, 151.347, 0, '3265:0 62416:0'),
    (62416, 'Manaforge Bnaar patrol 2', 530, 3523, 3726, 67, 70, 1100, 3800, 0, 524288, 3030.0, 4080.0, 150.861, 0, '3263:0 62415:0'),
    (62417, 'Ulduar exterior 1', 571, 67, 4445, 76, 80, 2200, 6800, 0, 1, 9040.0, -1290.0, 1060.29, 0, '62418:0 62419:0'),
    (62418, 'Ulduar exterior 2', 571, 67, 4445, 76, 80, 800, 2600, 0, 524288, 9040.0, -1205.0, 1058.59, 0, '62417:0 62419:0'),
    (62419, 'Ulduar exterior 3', 571, 67, 4445, 76, 80, 1000, 3000, 0, 1, 9125.0, -1290.0, 1065.21, 0, '62417:0 62418:0'),
    (62420, 'Black Temple exterior 1', 530, 3520, 3840, 67, 70, 2200, 6800, 0, 1, -3650.0, 116.67, 70.3253, 0, '62421:0 62422:0'),
    (62421, 'Black Temple exterior 2', 530, 3520, 3840, 67, 70, 800, 2600, 0, 524288, -3583.33, 183.33, 49.2033, 0, '62420:0 62422:0'),
    (62422, 'Black Temple exterior 3', 530, 3520, 3840, 67, 70, 1000, 3000, 0, 1, -3583.33, 116.67, 72.9774, 0, '62420:0 62421:0');

UPDATE creature_template_npcbot_wander_nodes SET links = TRIM(REPLACE(CONCAT(' ', TRIM(links), ' '), ' 4490:0 ', ' ')) WHERE id = 4489 AND mapid = 571 AND zoneid = 66;
UPDATE creature_template_npcbot_wander_nodes SET links = TRIM(REPLACE(CONCAT(' ', TRIM(links), ' '), ' 4489:0 ', ' ')) WHERE id = 4490 AND mapid = 571 AND zoneid = 66;
UPDATE creature_template_npcbot_wander_nodes SET links = TRIM(REPLACE(CONCAT(' ', TRIM(links), ' '), ' 4708:0 ', ' ')) WHERE id = 4707 AND mapid = 571 AND zoneid = 67;
UPDATE creature_template_npcbot_wander_nodes SET links = TRIM(REPLACE(CONCAT(' ', TRIM(links), ' '), ' 4707:0 ', ' ')) WHERE id = 4708 AND mapid = 571 AND zoneid = 67;
UPDATE creature_template_npcbot_wander_nodes SET links = TRIM(REPLACE(CONCAT(' ', TRIM(links), ' '), ' 4746:0 ', ' ')) WHERE id = 4744 AND mapid = 571 AND zoneid = 67;
UPDATE creature_template_npcbot_wander_nodes SET links = TRIM(REPLACE(CONCAT(' ', TRIM(links), ' '), ' 4744:0 ', ' ')) WHERE id = 4746 AND mapid = 571 AND zoneid = 67;
UPDATE creature_template_npcbot_wander_nodes SET links = TRIM(REPLACE(CONCAT(' ', TRIM(links), ' '), ' 5611:0 ', ' ')) WHERE id = 5612 AND mapid = 530 AND zoneid = 3433;
UPDATE creature_template_npcbot_wander_nodes SET links = CONCAT_WS(' ', NULLIF(TRIM(links), ''), '62411:0') WHERE id = 804 AND mapid = 0 AND zoneid = 41 AND COALESCE(links, '') NOT REGEXP '(^|[[:space:]])62411:';
UPDATE creature_template_npcbot_wander_nodes SET links = CONCAT_WS(' ', NULLIF(TRIM(links), ''), '62412:0') WHERE id = 805 AND mapid = 0 AND zoneid = 41 AND COALESCE(links, '') NOT REGEXP '(^|[[:space:]])62412:';
UPDATE creature_template_npcbot_wander_nodes SET links = CONCAT_WS(' ', NULLIF(TRIM(links), ''), '29:0') WHERE id = 1436 AND mapid = 1 AND zoneid = 14 AND COALESCE(links, '') NOT REGEXP '(^|[[:space:]])29:';
UPDATE creature_template_npcbot_wander_nodes SET links = CONCAT_WS(' ', NULLIF(TRIM(links), ''), '62416:0') WHERE id = 3263 AND mapid = 530 AND zoneid = 3523 AND COALESCE(links, '') NOT REGEXP '(^|[[:space:]])62416:';
UPDATE creature_template_npcbot_wander_nodes SET links = CONCAT_WS(' ', NULLIF(TRIM(links), ''), '62415:0') WHERE id = 3265 AND mapid = 530 AND zoneid = 3523 AND COALESCE(links, '') NOT REGEXP '(^|[[:space:]])62415:';
UPDATE creature_template_npcbot_wander_nodes SET links = CONCAT_WS(' ', NULLIF(TRIM(links), ''), '4228:0') WHERE id = 3831 AND mapid = 571 AND zoneid = 65 AND COALESCE(links, '') NOT REGEXP '(^|[[:space:]])4228:';
UPDATE creature_template_npcbot_wander_nodes SET links = CONCAT_WS(' ', NULLIF(TRIM(links), ''), '4064:0') WHERE id = 4063 AND mapid = 571 AND zoneid = 65 AND COALESCE(links, '') NOT REGEXP '(^|[[:space:]])4064:';
UPDATE creature_template_npcbot_wander_nodes SET links = CONCAT_WS(' ', NULLIF(TRIM(links), ''), '4063:0') WHERE id = 4064 AND mapid = 571 AND zoneid = 65 AND COALESCE(links, '') NOT REGEXP '(^|[[:space:]])4063:';
UPDATE creature_template_npcbot_wander_nodes SET links = CONCAT_WS(' ', NULLIF(TRIM(links), ''), '62413:0') WHERE id = 4162 AND mapid = 571 AND zoneid = 65 AND COALESCE(links, '') NOT REGEXP '(^|[[:space:]])62413:';
UPDATE creature_template_npcbot_wander_nodes SET links = CONCAT_WS(' ', NULLIF(TRIM(links), ''), '62414:0') WHERE id = 4163 AND mapid = 571 AND zoneid = 65 AND COALESCE(links, '') NOT REGEXP '(^|[[:space:]])62414:';
UPDATE creature_template_npcbot_wander_nodes SET links = CONCAT_WS(' ', NULLIF(TRIM(links), ''), '3831:0') WHERE id = 4228 AND mapid = 571 AND zoneid = 65 AND COALESCE(links, '') NOT REGEXP '(^|[[:space:]])3831:';
UPDATE creature_template_npcbot_wander_nodes SET links = CONCAT_WS(' ', NULLIF(TRIM(links), ''), '62405:0') WHERE id = 4489 AND mapid = 571 AND zoneid = 66 AND COALESCE(links, '') NOT REGEXP '(^|[[:space:]])62405:';
UPDATE creature_template_npcbot_wander_nodes SET links = CONCAT_WS(' ', NULLIF(TRIM(links), ''), '62406:0') WHERE id = 4490 AND mapid = 571 AND zoneid = 66 AND COALESCE(links, '') NOT REGEXP '(^|[[:space:]])62406:';
UPDATE creature_template_npcbot_wander_nodes SET links = CONCAT_WS(' ', NULLIF(TRIM(links), ''), '4560:0') WHERE id = 4558 AND mapid = 571 AND zoneid = 66 AND COALESCE(links, '') NOT REGEXP '(^|[[:space:]])4560:';
UPDATE creature_template_npcbot_wander_nodes SET links = CONCAT_WS(' ', NULLIF(TRIM(links), ''), '4558:0') WHERE id = 4560 AND mapid = 571 AND zoneid = 66 AND COALESCE(links, '') NOT REGEXP '(^|[[:space:]])4558:';
UPDATE creature_template_npcbot_wander_nodes SET links = CONCAT_WS(' ', NULLIF(TRIM(links), ''), '62407:0') WHERE id = 4624 AND mapid = 571 AND zoneid = 3711 AND COALESCE(links, '') NOT REGEXP '(^|[[:space:]])62407:';
UPDATE creature_template_npcbot_wander_nodes SET links = CONCAT_WS(' ', NULLIF(TRIM(links), ''), '62408:0') WHERE id = 4625 AND mapid = 571 AND zoneid = 3711 AND COALESCE(links, '') NOT REGEXP '(^|[[:space:]])62408:';
UPDATE creature_template_npcbot_wander_nodes SET links = CONCAT_WS(' ', NULLIF(TRIM(links), ''), '4667:0') WHERE id = 4665 AND mapid = 571 AND zoneid = 3711 AND COALESCE(links, '') NOT REGEXP '(^|[[:space:]])4667:';
UPDATE creature_template_npcbot_wander_nodes SET links = CONCAT_WS(' ', NULLIF(TRIM(links), ''), '62410:0') WHERE id = 4665 AND mapid = 571 AND zoneid = 3711 AND COALESCE(links, '') NOT REGEXP '(^|[[:space:]])62410:';
UPDATE creature_template_npcbot_wander_nodes SET links = CONCAT_WS(' ', NULLIF(TRIM(links), ''), '62409:0') WHERE id = 4666 AND mapid = 571 AND zoneid = 3711 AND COALESCE(links, '') NOT REGEXP '(^|[[:space:]])62409:';
UPDATE creature_template_npcbot_wander_nodes SET links = CONCAT_WS(' ', NULLIF(TRIM(links), ''), '4665:0') WHERE id = 4667 AND mapid = 571 AND zoneid = 3711 AND COALESCE(links, '') NOT REGEXP '(^|[[:space:]])4665:';
UPDATE creature_template_npcbot_wander_nodes SET links = CONCAT_WS(' ', NULLIF(TRIM(links), ''), '62402:0') WHERE id = 4707 AND mapid = 571 AND zoneid = 67 AND COALESCE(links, '') NOT REGEXP '(^|[[:space:]])62402:';
UPDATE creature_template_npcbot_wander_nodes SET links = CONCAT_WS(' ', NULLIF(TRIM(links), ''), '62403:0') WHERE id = 4708 AND mapid = 571 AND zoneid = 67 AND COALESCE(links, '') NOT REGEXP '(^|[[:space:]])62403:';
UPDATE creature_template_npcbot_wander_nodes SET links = CONCAT_WS(' ', NULLIF(TRIM(links), ''), '62404:0') WHERE id = 4744 AND mapid = 571 AND zoneid = 67 AND COALESCE(links, '') NOT REGEXP '(^|[[:space:]])62404:';
UPDATE creature_template_npcbot_wander_nodes SET links = CONCAT_WS(' ', NULLIF(TRIM(links), ''), '62404:0') WHERE id = 4746 AND mapid = 571 AND zoneid = 67 AND COALESCE(links, '') NOT REGEXP '(^|[[:space:]])62404:';
UPDATE creature_template_npcbot_wander_nodes SET links = CONCAT_WS(' ', NULLIF(TRIM(links), ''), '62400:0') WHERE id = 4796 AND mapid = 571 AND zoneid = 67 AND COALESCE(links, '') NOT REGEXP '(^|[[:space:]])62400:';
UPDATE creature_template_npcbot_wander_nodes SET links = CONCAT_WS(' ', NULLIF(TRIM(links), ''), '62401:0') WHERE id = 4797 AND mapid = 571 AND zoneid = 67 AND COALESCE(links, '') NOT REGEXP '(^|[[:space:]])62401:';
UPDATE creature_template_npcbot_wander_nodes SET links = CONCAT_WS(' ', NULLIF(TRIM(links), ''), '4972:0') WHERE id = 4970 AND mapid = 571 AND zoneid = 210 AND COALESCE(links, '') NOT REGEXP '(^|[[:space:]])4972:';
UPDATE creature_template_npcbot_wander_nodes SET links = CONCAT_WS(' ', NULLIF(TRIM(links), ''), '4970:0') WHERE id = 4972 AND mapid = 571 AND zoneid = 210 AND COALESCE(links, '') NOT REGEXP '(^|[[:space:]])4970:';
UPDATE creature_template_npcbot_wander_nodes SET links = CONCAT_WS(' ', NULLIF(TRIM(links), ''), '5612:0') WHERE id = 5608 AND mapid = 530 AND zoneid = 3433 AND COALESCE(links, '') NOT REGEXP '(^|[[:space:]])5612:';
UPDATE creature_template_npcbot_wander_nodes SET links = CONCAT_WS(' ', NULLIF(TRIM(links), ''), '5608:0') WHERE id = 5612 AND mapid = 530 AND zoneid = 3433 AND COALESCE(links, '') NOT REGEXP '(^|[[:space:]])5608:';

-- Keep traffic moving on corners, reserve longer stops for protected hubs.
UPDATE creature_template_npcbot_wander_nodes SET minwaittime = 350, maxwaittime = 1400
WHERE mapid = 571 AND id IN (4489, 4490, 4707, 4708, 4744, 4746);
UPDATE creature_template_npcbot_wander_nodes SET minwaittime = 2500, maxwaittime = 7500
WHERE mapid = 571 AND id IN (4667, 4796, 4972);
UPDATE creature_template_npcbot_wander_nodes SET minwaittime = 6000, maxwaittime = 15000
WHERE mapid = 571 AND id = 4700;
-- Move Camp Tunka'lo starts to the Horde junction. Retain the camp as a visit.
UPDATE creature_template_npcbot_wander_nodes SET flags = flags | 1
WHERE mapid = 571 AND zoneid = 67 AND id = 4744;
UPDATE creature_template_npcbot_wander_nodes SET flags = (flags & ~1) | 524288, minwaittime = 2500, maxwaittime = 7500
WHERE mapid = 571 AND zoneid = 67 AND id = 4745;
COMMIT;
DROP TEMPORARY TABLE npcbot_wander_refinement_guard;
