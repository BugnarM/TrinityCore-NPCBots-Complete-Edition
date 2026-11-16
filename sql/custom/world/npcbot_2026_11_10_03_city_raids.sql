-- Game Events for City Raids (Stormwind & Orgrimmar)
DELETE FROM `game_event` WHERE `eventEntry` IN (86, 87);
INSERT INTO `game_event` (`eventEntry`, `start_time`, `end_time`, `occurence`, `length`, `holiday`, `holidayStage`, `description`, `world_event`, `announce`) VALUES
(86, '2010-01-01 00:00:00', '2030-12-31 00:00:00', 5184000, 2592000, 0, 0, 'City Raid: Stormwind (Horde Invasion)', 5, 2),
(87, '2010-01-01 00:00:00', '2030-12-31 00:00:00', 5184000, 2592000, 0, 0, 'City Raid: Orgrimmar (Alliance Invasion)', 5, 2);
