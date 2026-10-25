-- Run against the active world database (world2 on this installation).
-- Authorization is defined in C++: RBAC 70036, like .npcbot createnew.
INSERT INTO `command` (`name`, `help`) VALUES
('npcbot autoequip', 'Syntax: .npcbot autoequip [blue|t7|t8|t9|t10|wrathful|ilvl]\nSelect your active, permanent NPCBot outside combat. Named presets require level 80. T7-T10 and Wrathful equip five matching class/spec set pieces plus compatible weapons/accessories. Numeric ilvl is a strict upper cap (1-284). Previous items return to the owner through bags/mail. Requires NPCBot creation permission (RBAC 70036).')
ON DUPLICATE KEY UPDATE `help` = VALUES(`help`);
