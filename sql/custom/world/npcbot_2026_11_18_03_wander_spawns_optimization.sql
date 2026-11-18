-- NPCBot Wanderer Spawns Optimization & Fixes
-- Enables active bot spawning across previously dormant zones and low-density areas.

-- 1. Teldrassil (Zone 141, Map 1) - Fix Night Elf Starter Hub & quest POIs
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 3 WHERE `id` = 33;   -- Aldrassil (Night Elf starter spawn!)
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 3 WHERE `id` = 972;  -- Shadowglen Center
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 3 WHERE `id` = 976;  -- Starbreeze Village
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 3 WHERE `id` = 988;  -- Pools of Arlithrien
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 3 WHERE `id` = 991;  -- Oracle Glade
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 3 WHERE `id` = 64;   -- Banethil Barrow Den

-- 2. Dun Morogh (Zone 1, Map 0) - Fix Dwarf & Gnome Starter Hub
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 3 WHERE `id` = 1;    -- Anvilmar

-- 3. Westfall (Zone 40, Map 0) - Expand Alliance spawns across the zone
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 3 WHERE `id` = 12;   -- Moonbrook
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 3 WHERE `id` = 745;  -- Saldean's Farm
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 3 WHERE `id` = 738;  -- Alexston Farmstead
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 3 WHERE `id` = 724;  -- Longshore Coast / Lighthouse approach
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 3 WHERE `id` = 739;  -- Gold Coast Quarry

-- 4. Darkshore (Zone 148, Map 1) - Expand Alliance spawns along the coast
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 3 WHERE `id` = 59;   -- Auberdine Town Center
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 3 WHERE `id` = 1009; -- Ameth'Aran
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 3 WHERE `id` = 1013; -- Bashal'Aran
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 3 WHERE `id` = 1023; -- Tower of Althalaxx
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 3 WHERE `id` = 1007; -- Master's Glaive
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 3 WHERE `id` = 999;  -- Remtravel's Excavation
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 3 WHERE `id` = 1024; -- Ruins of Mathystra

-- 5. Thousand Needles (Zone 400, Map 1) - Major hubs & racetrack
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 5 WHERE `id` = 41;   -- Freewind Post (Horde)
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 5 WHERE `id` = 50;   -- Whitereach Post (Horde)
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 1 WHERE `id` = 117;  -- Mirage Raceway (Neutral Goblin/Gnome)
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 5 WHERE `id` = 115;  -- Darkcloud Pinnacle

-- 6. Un'Goro Crater (Zone 490, Map 1) - Quadrants and POIs
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 1 WHERE `id` = 112;  -- Fungal Rock
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 1 WHERE `id` = 2235; -- South Ungoro / Slithering Scar
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 1 WHERE `id` = 2245; -- West Ungoro / Golakka Hot Springs
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 1 WHERE `id` = 2255; -- Central Ungoro / Fire Plume Ridge
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 1 WHERE `id` = 2265; -- East Ungoro

-- 7. Silverpine Forest (Zone 130, Map 0) - Key points of interest
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 5 WHERE `id` = 311;  -- Fenris Keep
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 5 WHERE `id` = 320;  -- Amberhill Hall
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 5 WHERE `id` = 324;  -- Pyrewood Village
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 5 WHERE `id` = 327;  -- Greymane Wall

-- 8. Badlands (Zone 3, Map 0) - Outposts & Excavations
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 1 WHERE `id` = 25;   -- Angor Fortress
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 1 WHERE `id` = 86;   -- Uldaman Entrance
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 1 WHERE `id` = 607;  -- Camp Boff
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 1 WHERE `id` = 619;  -- Hammertoe's Digsite

-- 9. Swamp of Sorrows (Zone 8, Map 0) - Faction and neutral camps
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 5 WHERE `id` = 186;  -- Misty Reed Post (Horde)
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 3 WHERE `id` = 819;  -- Harborage (Alliance)
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 1 WHERE `id` = 826;  -- Pool of Tears

-- 10. Searing Gorge (Zone 51, Map 0) - Thorium Brotherhood & Camps
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 1 WHERE `id` = 180;  -- Thorium Point Center
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 1 WHERE `id` = 564;  -- Grimesilt Digsite
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 1 WHERE `id` = 565;  -- Tanner Camp
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 1 WHERE `id` = 588;  -- Firewatch Ridge South

-- 11. Alterac Mountains (Zone 36, Map 0) - Activate spawns in towns & ruins
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 1 WHERE `id` = 331;  -- Strahnbrad
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 1 WHERE `id` = 338;  -- Ruins of Alterac
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 1 WHERE `id` = 328;  -- Dandred's Fold
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 5 WHERE `id` = 353;  -- Lordamere Internment Camp
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 1 WHERE `id` = 380;  -- Gallows Corner
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 1 WHERE `id` = 345;  -- Corrahn's Dagger

-- 12. Deadwind Pass (Zone 41, Map 0) - Karazhan path & ravines
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 1 WHERE `id` = 170;  -- Ariden's Camp
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 1 WHERE `id` = 802;  -- Deadman's Crossing
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 1 WHERE `id` = 801;  -- Karazhan Approach / East Exit

-- 13. Blackrock Mountain (Zone 25, Map 0) - Main Gates & Bridge
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 1 WHERE `id` = 570;  -- Searing Gorge Gates
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 1 WHERE `id` = 628;  -- Burning Steppes Gates
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 1 WHERE `id` = 623;  -- Blackrock Suspended Chains

-- 14. Wintergrasp (Zone 4197, Map 571) - Fortress Keep & Siege Workshops
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 9 WHERE `id` = 60000; -- Alliance Landing
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 9 WHERE `id` = 60010; -- Wintergrasp Keep Interior
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 9 WHERE `id` = 60024; -- East Workshop
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 9 WHERE `id` = 60026; -- West Workshop
UPDATE `creature_template_npcbot_wander_nodes` SET `flags` = 9 WHERE `id` = 60030; -- South Workshop
