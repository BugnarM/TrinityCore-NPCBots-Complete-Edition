-- Preserve legacy completion markers; track only unfinished/new package deliveries.
CREATE TABLE IF NOT EXISTS `character_heirlooms_received` (
  `guid` INT UNSIGNED NOT NULL,
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
CREATE TABLE IF NOT EXISTS `character_heirloom_delivery` (
  `guid` INT UNSIGNED NOT NULL,
  `item` INT UNSIGNED NOT NULL,
  `count` INT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`guid`, `item`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
CREATE TABLE IF NOT EXISTS `character_companion_stone` (
  `guid` INT UNSIGNED NOT NULL,
  `first_use_done` TINYINT UNSIGNED NOT NULL DEFAULT 0,
  `last_use_time` BIGINT UNSIGNED NOT NULL DEFAULT 0,
  PRIMARY KEY (`guid`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
