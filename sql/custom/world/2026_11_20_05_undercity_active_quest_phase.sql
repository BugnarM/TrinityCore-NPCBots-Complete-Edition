-- Keep Zidormi's manually applied siege aura across the Undercity zone change
-- for players with an active battle quest. The normal prerequisite rules keep
-- their autocast behavior. These rows do not cast the aura after opting out.
INSERT INTO `spell_area` (`spell`, `area`, `quest_start`, `quest_end`, `aura_spell`, `racemask`, `gender`,
    `autocast`, `quest_start_status`, `quest_end_status`) VALUES
(59062, 85,   13267, 0, 0, 0, 2, 0, 8, 0),
(59062, 1497, 13267, 0, 0, 0, 2, 0, 8, 0),
(59062, 85,   13377, 0, 0, 0, 2, 0, 8, 0),
(59062, 1497, 13377, 0, 0, 0, 2, 0, 8, 0)
ON DUPLICATE KEY UPDATE `spell` = `spell_area`.`spell`;
