-- Share the north elevator and its doors with the Horde assault phase.
-- The escort uses the verified landing and never waits on platform collision.
UPDATE `gameobject`
SET `phaseMask` = `phaseMask` | 64
WHERE `map` = 0 AND `id` IN (20649, 20650, 20651)
  AND `position_x` BETWEEN 1593 AND 1600 AND `position_y` BETWEEN 279 AND 306;
