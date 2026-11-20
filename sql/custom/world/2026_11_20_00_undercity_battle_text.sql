-- Use the AzerothCore dialogue pattern (grouped creature_text) with the local scenario's text.
-- Groups 240-242 are reserved here for the custom script. Preserve existing configured lines on reruns.
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `comment`) VALUES
(31650, 240, 0, 'Varimathras a fost infrant! Orasul Undercity apartine din nou Hoardei! Pentru Forsaken!', 14, 0, 100, 'Undercity custom event: Thrall - victory'),
(32401, 240, 0, 'Thrall! Astazi cerem socoteala pentru Bolvar Fordragon si cadavrele de la Wrathgate!', 14, 0, 100, 'Undercity custom event: Varian - confrontation'),
(31650, 241, 0, 'Varian, retrage-ti oamenii! Tradatorul a fost pedepsit, iar ciuma a fost oprita. Nu incepe un razboi aici!', 14, 0, 100, 'Undercity custom event: Thrall - response'),
(31418, 240, 0, 'Opriti-va! Nu va mai curge sange astazi! Acest conflict nu va aduce pace nimanui!', 14, 0, 100, 'Undercity custom event: Jaina - intervention'),
(31650, 242, 0, 'Pentru Hoarda! Soldati, inaintam spre inima orasului! Nimic nu va ramane in calea noastra!', 14, 0, 100, 'Undercity custom event: Thrall - assault'),
(31565, 240, 0, 'Ati intarziat, viermi! Puterea Legiunii este infinita! Acest oras ne apartine!', 14, 0, 100, 'Undercity custom event: Varimathras - aggro'),
(31565, 241, 0, 'Blestemati sa fiti... Stapane, iarta-ma...', 14, 0, 100, 'Undercity custom event: Varimathras - death'),
(32303, 240, 0, 'Pentru Bolvar! Pentru Alianta! Asaltati canalele si ucideti tradatorii!', 14, 0, 100, 'Undercity custom event: Varian - assault'),
(31530, 240, 0, 'Ati crezut ca suntem neputinciosi? Ciuma noastra va curata aceasta lume!', 14, 0, 100, 'Undercity custom event: Putress - aggro'),
(31530, 241, 0, 'Munca mea... nu se va opri... niciodata...', 14, 0, 100, 'Undercity custom event: Putress - death')
ON DUPLICATE KEY UPDATE `ID` = `creature_text`.`ID`;
