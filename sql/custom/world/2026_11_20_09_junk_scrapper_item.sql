-- ==============================================================================
-- 2026_11_20_09_junk_scrapper_item.sql
-- Portable Junk Scrapper & Auto-Sell Item for TrinityCore 3.3.5a
-- ==============================================================================

START TRANSACTION;

DELETE FROM `item_template` WHERE `entry` = 70902;

INSERT INTO `item_template` (
    `entry`, `class`, `subclass`, `SoundOverrideSubclass`, `name`, `displayid`, `Quality`, `Flags`, `FlagsExtra`,
    `BuyCount`, `BuyPrice`, `SellPrice`, `InventoryType`, `AllowableClass`, `AllowableRace`, `ItemLevel`, `RequiredLevel`,
    `RequiredSkill`, `RequiredSkillRank`, `requiredspell`, `requiredhonorrank`, `RequiredCityRank`, `RequiredReputationFaction`,
    `RequiredReputationRank`, `maxcount`, `stackable`, `ContainerSlots`, `StatsCount`,
    `delay`, `ammo_type`, `RangedModRange`,
    `spellid_1`, `spelltrigger_1`, `spellcharges_1`, `spellppmRate_1`, `spellcooldown_1`, `spellcategory_1`, `spellcategorycooldown_1`,
    `bonding`, `description`, `PageText`, `LanguageID`, `PageMaterial`, `startquest`, `lockid`, `Material`, `sheath`,
    `RandomProperty`, `RandomSuffix`, `block`, `itemset`, `MaxDurability`, `area`, `Map`, `BagFamily`, `TotemCategory`,
    `socketColor_1`, `socketContent_1`, `socketColor_2`, `socketContent_2`, `socketColor_3`, `socketContent_3`, `socketBonus`, `GemProperties`,
    `RequiredDisenchantSkill`, `ArmorDamageModifier`, `duration`, `ItemLimitCategory`, `HolidayId`, `ScriptName`, `DisenchantID`, `FoodType`,
    `minMoneyLoot`, `maxMoneyLoot`, `flagsCustom`
) VALUES (
    70902, 15, 0, -1, 'Junk Scrapper', 1183, 3, 64, 0,
    1, 0, 0, 0, -1, -1, 1, 0,
    0, 0, 0, 0, 0, 0,
    0, 1, 1, 0, 0,
    1000, 0, 0,
    55884, 0, -1, 0, 1000, 0, -1,
    1, 'Right Click to instantly sell all grey junk items in your bags for gold.', 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0,
    -1, 0, 0, 0, 0, 'item_junk_scrapper', 0, 0,
    0, 0, 0
);

COMMIT;
