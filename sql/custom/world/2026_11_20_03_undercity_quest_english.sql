-- Keep the Horde quest incomplete until the scripted event awards credit.
UPDATE `quest_template_addon` SET `SpecialFlags` = `SpecialFlags` | 2 WHERE `ID` = 13267;

-- Translate the seeded lines while preserving any locally edited creature text.
UPDATE `creature_text` SET `Text` = 'Varimathras has fallen! Undercity is ours again. For the Forsaken!'
WHERE `CreatureID` = 31650 AND `GroupID` = 240 AND `ID` = 0
  AND `Text` = 'Varimathras a fost infrant! Orasul Undercity apartine din nou Hoardei! Pentru Forsaken!';
UPDATE `creature_text` SET `Text` = 'Thrall! We have come for answers about Bolvar Fordragon and everyone who died at the Wrathgate!'
WHERE `CreatureID` = 32401 AND `GroupID` = 240 AND `ID` = 0
  AND `Text` = 'Thrall! Astazi cerem socoteala pentru Bolvar Fordragon si cadavrele de la Wrathgate!';
UPDATE `creature_text` SET `Text` = 'Varian, pull your soldiers back! The traitor has fallen and the plague is contained. There is no need to start a war here.'
WHERE `CreatureID` = 31650 AND `GroupID` = 241 AND `ID` = 0
  AND `Text` = 'Varian, retrage-ti oamenii! Tradatorul a fost pedepsit, iar ciuma a fost oprita. Nu incepe un razboi aici!';
UPDATE `creature_text` SET `Text` = 'Stand down, both of you! Enough blood has been spilled today.'
WHERE `CreatureID` = 31418 AND `GroupID` = 240 AND `ID` = 0
  AND `Text` = 'Opriti-va! Nu va mai curge sange astazi! Acest conflict nu va aduce pace nimanui!';
UPDATE `creature_text` SET `Text` = 'For the Horde! Soldiers, push into the heart of the city. Sweep aside anyone who stands in our way!'
WHERE `CreatureID` = 31650 AND `GroupID` = 242 AND `ID` = 0
  AND `Text` = 'Pentru Hoarda! Soldati, inaintam spre inima orasului! Nimic nu va ramane in calea noastra!';
UPDATE `creature_text` SET `Text` = 'You are too late, worms. This city belongs to the Legion now!'
WHERE `CreatureID` = 31565 AND `GroupID` = 240 AND `ID` = 0
  AND `Text` = 'Ati intarziat, viermi! Puterea Legiunii este infinita! Acest oras ne apartine!';
UPDATE `creature_text` SET `Text` = 'Damn you... Master, forgive me...'
WHERE `CreatureID` = 31565 AND `GroupID` = 241 AND `ID` = 0
  AND `Text` = 'Blestemati sa fiti... Stapane, iarta-ma...';
UPDATE `creature_text` SET `Text` = 'For Bolvar and the Alliance! Sweep through the sewers and take down the traitors!'
WHERE `CreatureID` = 32303 AND `GroupID` = 240 AND `ID` = 0
  AND `Text` = 'Pentru Bolvar! Pentru Alianta! Asaltati canalele si ucideti tradatorii!';
UPDATE `creature_text` SET `Text` = 'You thought we were helpless? Our plague will scour this world!'
WHERE `CreatureID` = 31530 AND `GroupID` = 240 AND `ID` = 0
  AND `Text` = 'Ati crezut ca suntem neputinciosi? Ciuma noastra va curata aceasta lume!';
UPDATE `creature_text` SET `Text` = 'My work... will outlive me...'
WHERE `CreatureID` = 31530 AND `GroupID` = 241 AND `ID` = 0
  AND `Text` = 'Munca mea... nu se va opri... niciodata...';

-- Seed the English lines on realms that do not have the older dialogue migration.
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `comment`) VALUES
(31650, 240, 0, 'Varimathras has fallen! Undercity is ours again. For the Forsaken!', 14, 0, 100, 'Undercity custom event: Thrall - victory'),
(32401, 240, 0, 'Thrall! We have come for answers about Bolvar Fordragon and everyone who died at the Wrathgate!', 14, 0, 100, 'Undercity custom event: Varian - confrontation'),
(31650, 241, 0, 'Varian, pull your soldiers back! The traitor has fallen and the plague is contained. There is no need to start a war here.', 14, 0, 100, 'Undercity custom event: Thrall - response'),
(31418, 240, 0, 'Stand down, both of you! Enough blood has been spilled today.', 14, 0, 100, 'Undercity custom event: Jaina - intervention'),
(31650, 242, 0, 'For the Horde! Soldiers, push into the heart of the city. Sweep aside anyone who stands in our way!', 14, 0, 100, 'Undercity custom event: Thrall - assault'),
(31565, 240, 0, 'You are too late, worms. This city belongs to the Legion now!', 14, 0, 100, 'Undercity custom event: Varimathras - aggro'),
(31565, 241, 0, 'Damn you... Master, forgive me...', 14, 0, 100, 'Undercity custom event: Varimathras - death'),
(32303, 240, 0, 'For Bolvar and the Alliance! Sweep through the sewers and take down the traitors!', 14, 0, 100, 'Undercity custom event: Varian - assault'),
(31530, 240, 0, 'You thought we were helpless? Our plague will scour this world!', 14, 0, 100, 'Undercity custom event: Putress - aggro'),
(31530, 241, 0, 'My work... will outlive me...', 14, 0, 100, 'Undercity custom event: Putress - death')
ON DUPLICATE KEY UPDATE `ID` = `creature_text`.`ID`;
