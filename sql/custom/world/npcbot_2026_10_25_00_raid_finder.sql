-- Run against the active world database (world2 on this installation).
INSERT INTO `command` (`name`, `help`) VALUES
("raid queue", "Syntax: .raid queue <key> [10|25] [tank|heal|dps]\nQueue your solo character or premade group for Normal raid matchmaking. Bot fill after the configured wait; native access and item-level requirements apply. Raids: naxx, os, eoe, voa, ulduar, totc, ony, icc, rs."),
("raid find", "Syntax: .raid find <key> [10|25] [tank|heal|dps]\nAlias for .raid queue."),
("raid leave", "Syntax: .raid leave\nLeave the current raid finder queue."),
("raid status", "Syntax: .raid status\nCheck your current status and wait time in the raid finder queue."),
("raid list", "Syntax: .raid list\nList all available raids supported by the automated Raid Finder."),
("rf queue", "Syntax: .rf queue <key> [10|25] [tank|heal|dps]\nShortcut for .raid queue."),
("rf find", "Syntax: .rf find <key> [10|25] [tank|heal|dps]\nShortcut for .raid queue."),
("rf leave", "Syntax: .rf leave\nShortcut for .raid leave."),
("rf status", "Syntax: .rf status\nShortcut for .raid status."),
("rf list", "Syntax: .rf list\nShortcut for .raid list.")
ON DUPLICATE KEY UPDATE `help` = VALUES(`help`);

INSERT INTO `command` (`name`, `help`) VALUES
('npcbot autoequip', 'Syntax: .npcbot autoequip [blue|t7|t8|t9|t10|wrathful|ilvl|organic [blue|t7|t8|t9|t10]]\nSelect your active permanent bot outside combat. Standard presets match class/spec/role; organic presets vary set bonuses and secondary PvP pieces. Previous items return to bags/mail. Requires RBAC 70036.')
ON DUPLICATE KEY UPDATE `help` = VALUES(`help`);
