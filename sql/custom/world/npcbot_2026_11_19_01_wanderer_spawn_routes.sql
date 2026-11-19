START TRANSACTION;

-- Crystalvein Mine and the existing trail to Gurubashi: the mine mobs are level 41–44.
-- Keep the level 39–45 anchor connected to both the mine loop and the southern trails.
UPDATE creature_template_npcbot_wander_nodes
SET minlevel = CASE WHEN id IN (926, 930) THEN 39 ELSE 37 END, maxlevel = 45
WHERE mapid = 0 AND zoneid = 33 AND id IN (926, 927, 928, 929, 930);

-- Move CrystalveinMine3 onto the adjacent ground polygon, about 2.5 yards away.
UPDATE creature_template_npcbot_wander_nodes
SET x = -13171.5, y = -469.707, z = 4.58118
WHERE id = 928 AND mapid = 0 AND zoneid = 33 AND name = 'CrystalveinMine3';

-- Dream Bough is an isolated elite pocket. Both existing ground routes terminate early.
-- Retire the waypoint and its two links. The Ravenwind / Twin Colossals route remains connected.
UPDATE creature_template_npcbot_wander_nodes
SET links = TRIM(REGEXP_REPLACE(CONCAT(' ', TRIM(links), ' '), ' 1921:[0-9]+ ', ' '))
WHERE mapid = 1 AND zoneid = 357 AND id IN (1914, 1920);

DELETE FROM creature_template_npcbot_wander_nodes
WHERE id = 1921 AND mapid = 1 AND zoneid = 357 AND areaid = 1111 AND name = 'FeralasN9';

-- Lost Rigger Cove: add a second, bidirectional ground route into the level 44–50 network.
UPDATE creature_template_npcbot_wander_nodes
SET links = CONCAT_WS(' ', NULLIF(TRIM(links), ''), '2215:0')
WHERE id = 2212 AND mapid = 1 AND zoneid = 440 AND links NOT REGEXP '(^|[[:space:]])2215:';

UPDATE creature_template_npcbot_wander_nodes
SET links = CONCAT_WS(' ', NULLIF(TRIM(links), ''), '2212:0')
WHERE id = 2215 AND mapid = 1 AND zoneid = 440 AND links NOT REGEXP '(^|[[:space:]])2212:';

COMMIT;
