-- Apply a two-minute respawn to ordinary outdoor combat spawns on the five-minute timer.
-- Leave other timers, scripted encounters, events, pools, and linked respawns intact.
START TRANSACTION;

UPDATE creature AS c
JOIN creature_template AS t ON t.entry = c.id
SET c.spawntimesecs = 120
WHERE c.map IN (0, 1, 530, 571) AND c.spawntimesecs = 300
  AND t.rank = 0 AND t.type IN (1, 2, 3, 4, 5, 6, 7, 9, 10)
  AND t.faction NOT IN (0, 35, 114) AND t.VehicleId = 0
  AND c.npcflag = 0 AND t.npcflag = 0
  -- Exclude non-attackable, player-immune and non-interactable spawns or templates.
  AND ((c.unit_flags | t.unit_flags) & 0x02010182) = 0
  -- Exclude civilian, guard, trigger, no-XP, event and other special combat flags.
  AND (t.flags_extra & 0x0000E4C3) = 0
  AND COALESCE(c.ScriptName, '') = '' AND t.ScriptName = ''
  AND NOT EXISTS (SELECT 1 FROM game_event_creature AS e WHERE e.guid = c.guid)
  AND NOT EXISTS (SELECT 1 FROM pool_members AS p WHERE p.type = 0 AND p.spawnId = c.guid)
  AND NOT EXISTS (SELECT 1 FROM linked_respawn AS r WHERE r.linkType IN (0, 1) AND r.guid = c.guid)
  AND NOT EXISTS (SELECT 1 FROM linked_respawn AS r WHERE r.linkType IN (0, 3) AND r.linkedGuid = c.guid);

COMMIT;
