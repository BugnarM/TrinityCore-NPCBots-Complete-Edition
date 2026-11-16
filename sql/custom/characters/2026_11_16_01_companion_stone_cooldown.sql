-- Companion Hearthstone persistent cooldown & first use tracking
CREATE TABLE IF NOT EXISTS `character_companion_stone` (
    `guid` INT UNSIGNED NOT NULL PRIMARY KEY,
    `first_use_done` TINYINT UNSIGNED NOT NULL DEFAULT 0,
    `last_use_time` BIGINT UNSIGNED NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
