-- Companion Hearthstone item configuration
-- Icon: displayid 20984 (INV_Misc_Rune_06 - Crimson Fiery Rune Hearthstone)
-- Cooldown: 5 hours (18,000,000 ms)
UPDATE `item_template` 
SET `displayid` = 20984, 
    `description` = 'Right Click to summon or dismiss your leveling companion bot (Max 1). 5 hour cooldown.', 
    `spellcooldown_1` = 18000000, 
    `quality` = 3 
WHERE `entry` = 70901;

-- Ensure all starting race and class combinations receive the Companion Hearthstone at level 1
INSERT IGNORE INTO `playercreateinfo_item` (`race`, `class`, `itemid`, `amount`)
SELECT `race`, `class`, 70901, 1 FROM `playercreateinfo`;
