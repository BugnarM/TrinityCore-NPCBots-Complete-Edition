-- Existing installations: road checkpoints projected onto the server's v15 navmesh.
-- The previous approximate heights were up to 42 yards below the walkable surface.
START TRANSACTION;
UPDATE creature_template_npcbot_wander_nodes SET z=8.22452 WHERE id=5315 AND mapid=628;
UPDATE creature_template_npcbot_wander_nodes SET z=15.8709 WHERE id=5316 AND mapid=628;
UPDATE creature_template_npcbot_wander_nodes SET z=27.9307 WHERE id=5317 AND mapid=628;
-- This point was inside an isolated cliff patch; move it onto the verified coastal road.
UPDATE creature_template_npcbot_wander_nodes SET x=979.2,y=-495.733,z=23.6282 WHERE id=5318 AND mapid=628;
UPDATE creature_template_npcbot_wander_nodes SET z=49.8497 WHERE id=5319 AND mapid=628;
UPDATE creature_template_npcbot_wander_nodes SET z=20.6601 WHERE id=5320 AND mapid=628;
COMMIT;
