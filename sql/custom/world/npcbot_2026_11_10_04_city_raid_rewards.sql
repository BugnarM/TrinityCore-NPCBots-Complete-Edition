-- Temporary City Raid reward objects. Native GO loot is intercepted by the AI:
-- eligibility, bag space, money cap and one claim per character are checked.
-- Display 5744 is the installed Reinforced Fel Iron Chest model (template 185168).
START TRANSACTION;
INSERT INTO gameobject_template (entry, type, displayId, name, size, ScriptName)
VALUES (990086, 3, 5744, 'Spoils of the Stormwind Raid', 1, 'go_city_raid_celebration'),
       (990087, 3, 5744, 'Spoils of the Orgrimmar Raid', 1, 'go_city_raid_celebration')
ON DUPLICATE KEY UPDATE type=VALUES(type), displayId=VALUES(displayId), name=VALUES(name), size=VALUES(size), ScriptName=VALUES(ScriptName);
COMMIT;
