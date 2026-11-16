-- Holiday Events Fixes: Ahune (Midsummer Fire Festival) and Hallow's End
-- 1. Midsummer Fire Festival: Ahune missing spell script bindings and bonfire bunnies
DELETE FROM `spell_script_names` WHERE `spell_id` IN (45954, 45964, 46333, 46735, 46809);
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(45954, 'spell_ahune_ahunes_shield'),
(45964, 'spell_ahune_find_opening_visual'),
(46333, 'spell_ahune_find_opening_beam_end'),
(46735, 'spell_ahune_spank_force_bunny_to_knock_you_to'),
(46809, 'spell_ahune_make_ahunes_ghost_burst');

UPDATE `creature_template` SET `ScriptName` = 'npc_shaman_bonfire_bunny' WHERE `entry` IN (25971, 25972, 25973);

-- 2. Hallow's End: Missing bucket brigade & extinguishing spells
DELETE FROM `spell_script_names` WHERE `spell_id` IN (42340, 42339);
INSERT INTO `spell_script_names` (`spell_id`, `ScriptName`) VALUES
(42340, 'spell_hallow_end_throw_bucket'),
(42339, 'spell_hallow_end_bucket_lands');
