-- Move the blue inner-gate checkpoint onto the ground component connected to
-- the defender graveyard. The old closest polygon was an isolated sliver.
-- In-memory waypoints take this update on the next worldserver start.
UPDATE `creature_template_npcbot_wander_nodes`
SET `x` = 1408.92, `y` = -220.981, `z` = 32.1082
WHERE `mapid` = 607 AND `id` = 5222;
