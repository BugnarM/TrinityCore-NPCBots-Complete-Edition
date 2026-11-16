-- Connect southern workshops (Westpark #60032, Eastpark #60033) across bridges to northern workshop points (#60024, #60026)
START TRANSACTION;
UPDATE creature_template_npcbot_wander_nodes SET links = "60025:0 60011:0 60012:0 60032:0" WHERE id = 60024;
UPDATE creature_template_npcbot_wander_nodes SET links = "60016:0 60031:0 60027:0 60033:0" WHERE id = 60026;
UPDATE creature_template_npcbot_wander_nodes SET links = "60024:0 60025:0 60023:0 60011:0" WHERE id = 60032;
UPDATE creature_template_npcbot_wander_nodes SET links = "60026:0 60027:0 60031:0 60016:0" WHERE id = 60033;
COMMIT;
