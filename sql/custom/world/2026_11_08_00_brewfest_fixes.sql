-- Fix Brewfest Barker Bunny spell targets for Ironforge areatriggers
-- 4772 is Mystic Ward (Barker Bunny 4, Spell 43262)
-- 4774 is Military Ward (Barker Bunny 2, Spell 43260)
UPDATE `smart_scripts` SET `action_param1` = 43262, `comment` = "Areatrigger - On Trigger - Cast 'Brewfest  - Barker Bunny 4'" WHERE `entryorguid` = 4772 AND `source_type` = 2;
UPDATE `smart_scripts` SET `action_param1` = 43260, `comment` = "Areatrigger - On Trigger - Cast 'Brewfest  - Barker Bunny 2'" WHERE `entryorguid` = 4774 AND `source_type` = 2;
