# TrinityCore 3.3.5a – NPCBots Complete Edition

[![TrinityCore 3.3.5a](https://img.shields.io/badge/TrinityCore-3.3.5a%20(WotLK)-blue.svg)](https://github.com/TrinityCore/TrinityCore)
[![NPCBots Integrated](https://img.shields.io/badge/NPCBots-Trickerer%20Integrated-green.svg)](https://github.com/trickerer/Trinity-Bots)
[![C++17/20](https://img.shields.io/badge/C%2B%2B-17%2F20-orange.svg)](https://en.wikipedia.org/wiki/C%2B%2B17)
[![Tests Passing](https://img.shields.io/badge/Tests-53%2F53%20Passing-brightgreen.svg)](contrib/tools/run_custom_audit.py)
[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-lightgrey.svg)](COPYING)

---

### 📢 Live Test Server Available Upon Request!
> **Interested in trying out this living world before building or installing it yourself?**  
> We can temporarily spin up a live public test realm for players, testers, and server developers to experience the autonomous bots, 40v40 Wintergrasp, Arena queues, RDF, Raid Finder, and AoE looting firsthand!  
> 💬 **Open an Issue or start a Discussion on this repository to request temporary test server access!**

---

An advanced, production-grade distribution of **TrinityCore 3.3.5a (Wrath of the Lich King)** equipped with **NPCBots** and an extensive suite of custom autonomous AI systems. Designed for both solo-play enthusiasts and populated private servers seeking a truly living, breathing Azeroth without manual bot micromanagement.

---

## 📊 Quick Comparison: What Makes This Edition Different?

| Feature | Vanilla TrinityCore 3.3.5a | Base NPCBots (Trickerer) | **NPCBots Complete Edition (This Fork)** |
|:---|:---:|:---:|:---:|
| **Area-of-Effect (AoE) Looting** | ❌ 1 corpse at a time | ❌ 1 corpse at a time | ✅ **MoP-Style 120-yard AoE Loot (up to 60 corpses, consolidated gold & smart stacking)** |
| **Zone Wanderers Spawning** | ❌ None | ⚠️ Static across continents (High CPU & RAM) | ✅ **Dynamic Player-Centric (50 bots per occupied zone, auto-despawn on departure)** |
| **Server Memory & Grid Safety** | ✅ Standard unload | ❌ Long-lived skeletons block grid unloads (OOM risk) | ✅ **Anti-OOM: 5-min skeleton decay (`BonesLifetimeSeconds = 300`) & safe grid unloading** |
| **Healer Bot AI Engine** | ❌ None | ⚠️ Rigid 65% direct heal cutoff, no OOC top-off | ✅ **Proactive 95% OOC top-off, emergency priority brackets, incoming heal tracking** |
| **Open-World Player Aid** | ❌ None | ❌ Wanderers ignore players | ✅ **Wandering bots actively spot, heal, and resurrect dead or dying players in the wild** |
| **Personal Companion Bot** | ❌ None | ⚠️ Manual NPC spawn / command | ✅ **Instant Companion Hearthstone (Item 70901) with full interactive gossip menus** |
| **Starter Heirlooms & Inventory** | ❌ Standard gray gear | ❌ Standard gray gear | ✅ **Full Class Heirlooms (+30% XP), complete weapon arsenal, 4x 22-slot Glacial Bags** |
| **Cold Weather Flight** | ❌ Level 77 manual purchase | ❌ Level 77 manual purchase | ✅ **Tome of Cold Weather Flight in bags, ready for level 68** |
| **Spell Progression** | ❌ Visit trainer every 2 levels | ❌ Visit trainer every 2 levels | ✅ **Auto-Learn Spells on Level Up (immediate abilities, ranks, and talents)** |
| **Ghost / Spirit Run Speed** | ⚠️ 1.0x (Slow corpse runs) | ⚠️ 1.0x (Slow corpse runs) | ✅ **3.0x Ghost Speed (300% faster recovery runs after death)** |
| **Random Dungeon Finder (RDF)** | ⚠️ Requires real players | ⚠️ Basic bot fill | ✅ **Instant RDF queue: auto-fills Tank, Healer, 3 DPS bots with mechanic handling** |
| **Raid Finder with Bots (10 & 25)** | ❌ None | ❌ None | ✅ **Custom Raid Finder (`.raid queue`) for ICC, ToC, Ulduar, Naxx with bot roles** |
| **Arena Matchmaking (2v2/3v3/5v5)** | ⚠️ Players only | ⚠️ Basic arena | ✅ **Instant bot arena teams with LoS tactics, CC chains & burst synchronization** |
| **Lake Wintergrasp 40v40 Bot War** | ⚠️ Human-only battles | ❌ No Wintergrasp bot AI | ✅ **Epic 40v40 siege battles, autonomous Catapults/Demolishers/Engines, player boarding** |
| **Strand of the Ancients & Isle of Conquest** | ⚠️ Human-only | ❌ No vehicle/objective AI | ✅ **Autonomous Demolishers, seaforium charges, Gunship drops & keep teleporters** |
| **Autonomous City Raids** | ❌ Scripted boss only | ❌ None | ✅ **40-man raiding armies, 25-man throne defense, reinforcements & anti-exploit leash** |
| **World Bosses & Roamers** | ⚠️ Static world bosses | ❌ None | ✅ **Roaming Dark Knight Nemesis 5-man bosses in populated zones** |
| **Social `/who` List** | ⚠️ Real players only | ❌ Bots hidden | ✅ **Bots appear seamlessly in `/who` with realistic guild names and classes** |
| **Data Persistence & Integrity** | ⚠️ Standard tables | ⚠️ Basic persistence | ✅ **ACID-safe InnoDB ledgers for Heirlooms and Companion ownership** |
| **Regression Test Coverage** | ⚠️ Core unit tests | ⚠️ Limited | ✅ **53 automated custom regression tests (`run_custom_audit.py`)** |

---

## 🌟 Detailed Innovations & Systems

### 1. ⚡ Modern Area-of-Effect (AoE) Looting (MoP-Style)
* **One-Click Multi-Corpse Gathering:** Right-clicking any dead NPC corpse automatically loots **all eligible dead corpses within a massive 120-yard radius** (up to 60 corpses in a single action).
* **Consolidated Money:** All copper, silver, and gold from every corpse in the area is merged into a single sum and distributed properly among players and bot companions.
* **Smart Stacking:** Identical stackable items (cloth, leather, meat, reagents) from different mobs are merged into clean stacks in the loot window.
* **Instant Quest Progression:** Quest items needed for your active quests are collected directly into your quest bags, updating objectives immediately with conservative solo/group rights protection.
* **Zero-Loss Guarantee:** If your bags are full or total items exceed client window limits (16 slots), remaining items safely stay on their source corpses (which continue to sparkle) so nothing is ever lost.

### 2. 🌍 Hybrid Dynamic Zone Wanderers (`botdynamiczone`) & Anti-OOM Memory Safety
Standard wanderer bots spawn across entire continents, wasting massive CPU and memory in empty zones.
* **Player-Centric Spawning:** Autonomous wanderer bots only spawn in the specific zone where human players are currently active.
* **50-Bot High-Density Population:** Populates up to 50 bots per occupied zone for lively skirmishes and vibrant open-world questing.
* **Player Proximity Detection:** Spawns dynamically within 350 yards of active players and despawns bots cleanly beyond 700 yards.
* **Level-Appropriate Scaling:** Bots dynamically match the level range of the player's zone (e.g. Level 1–10 in Elwynn Forest, Level 40–50 in Tanaris, Level 70–80 in Northrend).
* **Anti-OOM Grid Cleanup (5-Min Skeletons):** Fallen bot bones decay after **5 minutes** (`NpcBot.Wanderer.BonesLifetimeSeconds = 300`) instead of 30 minutes. This allows inactive map grids to unload cleanly (`GridUnload = 1`), preventing Linux kernel Out Of Memory (OOM) killer crashes.

### 3. 💖 Intelligent Bot Healer AI & Out-of-Combat Top-Off (`bothealingpolicy`)
Base bot healing historically suffered from a rigid 65% HP direct heal cutoff, leaving players and companions perpetually wounded out of combat:
* **Out-of-Combat Top-Off (95% Ceiling):** Healers (Priests, Paladins, Druids, Shamans) actively top off players, group members, and friendly bots to 95% HP when out of combat (with mana conservation above 25%).
* **Emergency Healing Priority Brackets:** Emergency heals (<60% owner, <50% group) take immediate precedence over normal spell rotations.
* **Smart Overheal Prevention & Cast Cancellation:** If an incoming heal tops off the target or the target dies, healers cancel their cast immediately to conserve mana and avoid wasted GCDs.
* **Incoming Heal Awareness:** Healers track projected incoming heals and HoTs to prevent multiple bots from overhealing the same target.
* **Open-World Wanderer Assistance:** Friendly wanderer bots spot damaged or dead friendly adventurers within 40 yards, actively providing healing, buffs, and resurrection in the wild!

### 4. 🤝 Player Companion Bot & Companion Hearthstone
* **Companion Hearthstone (Item 70901):** Every adventurer receives a Companion Hearthstone to summon or toggle a personal leveling companion bot (instant first use, balanced 5-hour cooldown on subsequent uses; GMs have no cooldown and can command unlimited companions).
* **Full Interactive Gossip Dialog:** Right-clicking your companion bot opens standard NPCBot gossip menus directly in the world: **Manage Roles, Manage Equipment, Spells, and Add to Group**.
* **Transactional State Persistence:** Companion ownership and attachment are committed atomically with rollback protection to prevent orphan creatures or state desync.

### 5. 🎁 Auto-Granted Full Class Heirlooms & Starter Quality-of-Life
* **Every Starting Player Fully Equipped:** Upon character creation and first login, players are automatically equipped with the entire canonical WotLK Heirloom collection tailored to their class and specs.
* **+20% to +30% Experience Gain:** Includes 2x **Dread Pirate Rings** (`50255`, +5% XP each), plus class-matching **Shoulders** (`42949..42985`, +10% XP) and **Chests** (`48677..48691`, +10% XP).
* **Multi-Spec Weapon Arsenal:** Players receive a complete weapon arsenal (2H Axes, 2H Swords, 2H Maces, Staves, 1H Maces, 1H Swords, Daggers, Ranged Bows/Guns) covering all leveling specializations (e.g. Retribution/Holy for Paladins, Feral/Balance for Druids, Enhancement/Elemental for Shamans).
* **Instant Weapon Proficiencies:** Corresponding weapon skills are taught automatically so players never encounter "Weapon proficiency needed" roadblocks.
* **88+ Slots of Starter Inventory:** Automatically equips **4x Glacial Bags** (22 slots each, entry `41600`) into empty bag slots so players never run out of inventory room.
* **Northrend Cold Weather Flight Ready:** Every character receives a **Tome of Cold Weather Flight** (`49177`) in their bags ready for level 68.
* **InnoDB Ledger Security:** Heirlooms are tracked in an ACID transaction ledger (`characters.custom_heirloom_ledger`) preventing item duplication or loss on crash/reconnect.

### 6. 📜 Auto-Learn Spells on Level Up (`AutoLearnSpells`)
* Eliminates the tedious need to stop questing and travel to class trainers every 2 levels.
* New class spells, ability ranks, and weapon skills are learned automatically upon reaching the required level.
* Fully configurable via `AutoLearnSpells.Enable` and `AutoLearnSpells.Announce` in `worldserver.conf`.

### 7. 👻 3x Spirit / Ghost Run Speed
* Reduces tedious corpse-run downtime: while dead in spirit form, player movement speed is boosted to **300%** (`Ghost.RunSpeedMultiplier = 3.0`).
* Rejoining party encounters and open-world battles happens in seconds rather than minutes.

### 8. 🛡️ Instant Random Dungeon Finder (RDF) with Bot Roles
* **Instant Group Formation:** Queue solo or with friends in the standard Dungeon Finder (Normal or Heroic).
* **Automatic Role Fill:** If human players aren't available, the queue instantly fills within seconds with an intelligent **Tank**, **Healer**, and **3 DPS** NPCBots.
* **Mechanic-Aware Combat:** Dungeon bots handle dispels, interrupts, aggro management, and boss encounter mechanics.

### 9. 👑 Raid Finder with NPCBots (10 & 25-man Raids)
* **Custom Raid Queue (`.raid queue`):** Queue for classic WotLK raids (Icecrown Citadel, Trial of the Crusader, Ulduar, Naxxramas, Onyxia, etc.) in 10-man or 25-man format.
* **Balanced Role Composition:** Automatically assembles balanced rosters (e.g. 2 Tanks, 5 Healers, 18 DPS for 25-man; 2 Tanks, 2-3 Healers, 5-6 DPS for 10-man) with item level scaling.

### 10. ⚔️ Full Arena Matchmaking System (2v2, 3v3, 5v5)
* **Instant PvP Queues:** Queue solo or in teams across all classic WotLK arenas (Nagrand, Blade's Edge, Dalaran Sewers, Ruins of Lordaeron, Ring of Valor).
* **Autonomous Opponents & Teammates:** AI teams execute real PvP tactics: Line-of-Sight (LOS) positioning, Crowd Control chaining (stuns, silences, polymorphs), and synchronized burst phases.

### 11. 🏰 Lake Wintergrasp 40v40 Epic Siege Warfare
* **Full-Scale 40v40 Warfare:** Lake Wintergrasp is transformed into an autonomous 40 Alliance vs. 40 Horde clash.
* **Autonomous Siege Vehicles:** Bots construct, drive, and aim **Catapults, Demolishers, and Siege Engines**.
* **Multi-Crew AI & Player Boarding:** Bots operate multi-seat vehicles (driver + passenger turret gunners). Human players can board bot-driven vehicles or drive while bots provide turret support or infantry escort.
* **Organized Wall Assaults:** Squad leaders and siege vehicles follow strategic lanes (Westpark, Eastpark, Fortress Gates) rather than wandering randomly.
* **WotLK Military Promotions:** Authentic rank progression (Recruit ➡️ Corporal at 5 kills ➡️ Lieutenant at 10 kills) required to build heavy war machines.

### 12. 🚢 Strand of the Ancients (15v15) & Isle of Conquest (40v40)
* **Strand of the Ancients:** Attackers pilot Demolishers from the beach, assault colored gates, and plant seaforium charges while defenders man turret cannons.
* **Isle of Conquest:** Autonomous battles across the Docks, Hangar (Gunship aerial drops), and Siege Workshop with fortress keep teleporter support.

### 13. 🛡️ Autonomous City Raids & Throne Room Defense
* **40-Man Armies:** Epic 40-attacker raids assemble with dedicated 5-man subgroups and healers to march on enemy capital cities.
* **Faction Leader Boss Tactics:** At King Varian Wrynn or Thrall, bots execute real raid tactics—main tank facing boss away, secondary tanks swapping aggro on high burst, and casters maintaining safe distance.
* **Hardened Defense & Leash Bounds:** Capital cities feature 25 dedicated defenders (15 Throne Room elite guards) that reinforce from graveyards. Defender (65y) and Boss (90y) leash limits prevent exploit pulling out of throne rooms.
* **Defense Alerts & Victory Chest:** Global city defense alerts broadcast to players, and defeating the city leader spawns a celebration reward chest with **Emblems of Triumph and Gold**.

### 14. 👹 Dark Knight Nemesis Roamers (5-Man World Bosses)
* Roaming elite bosses hostile to both Horde and Alliance patrol key zones, offering spontaneous, challenging open-world dungeon encounters with rare loot rewards (can be enabled/disabled via `worldserver.conf`).

### 15. 👥 Living Social `/who` List Integration
* Active NPCBots and wanderers seamlessly appear in the player `/who` social directory with realistic guild names, races, and classes, making the server feel like a vibrant, high-population realm.

---

## 💾 Custom Database Updates (SQL)

All custom database migrations are included in `sql/custom/world/`:
* **Wintergrasp nodes, links & vehicle spawns:** `2026_10_23_00_wintergrasp_bot_war.sql`, `npcbot_2026_11_09_01_wintergrasp_native_bots.sql`, `npcbot_2026_11_15_01_wintergrasp_wander_links.sql`
* **Battleground waypoint meshes (SA & IC):** `2026_10_23_01_battleground_bots_sa_ic.sql`, `npcbot_2026_10_25_01_ioc_navigation.sql`, `npcbot_2026_10_26_01_sa_navigation.sql`
* **Arena meshes & points:** `2026_10_24_00_arena_bot_mesh.sql`
* **City Raids & Reward Chests:** `npcbot_2026_11_10_03_city_raids.sql`, `npcbot_2026_11_10_04_city_raid_rewards.sql`, `npcbot_2026_11_11_01_city_raid_internal_events.sql`
* **World Bosses & Mall Vendors:** `2026_11_08_01_gmisland_mall_vendors.sql`, `2026_11_10_01_hellfire_kazzak_wanderers.sql`, `2026_11_10_05_hyjal_dragons_of_nightmare.sql`

### Automated SQL Installer
Custom migrations use separate manifests for world, characters and auth, explicit prerequisites, SHA-256 checksums and an InnoDB database execution ledger:

```bash
# Dry run:
python3 sql/custom/apply_custom_sql.py --scope world --database world --defaults-file /secure/mysql.cnf

# Apply with verified backup:
python3 sql/custom/apply_custom_sql.py --scope world --database world --defaults-file /secure/mysql.cnf --apply --backup-dir /secure/trinity-backups
```

---

## ⚙️ Configuration Reference (`worldserver.conf`)

Recommended settings for optimal performance and stability:

```ini
###############################################################################
# AREA OF EFFECT (AoE) LOOTING
###############################################################################
AOELoot.Enable = 1
AOELoot.Range = 120.0
AOELoot.MaxCorpses = 60

###############################################################################
# DYNAMIC ZONE WANDERERS & MEMORY SAFETY
###############################################################################
NpcBot.WanderingBots.DynamicZone.Enable = 1
NpcBot.WanderingBots.DynamicZone.BotsPerZone = 50
NpcBot.WanderingBots.DynamicZone.UpdateInterval = 10
NpcBot.WanderingBots.DynamicZone.MaxBots = 50
NpcBot.WanderingBots.DynamicZone.SpawnBudget = 10
NpcBot.WanderingBots.DynamicZone.ProximityRadius = 350.0
NpcBot.WanderingBots.DynamicZone.DespawnRadius = 700.0
# 5-minute skeleton decay allows inactive grids to unload cleanly and prevents OOM
NpcBot.Wanderer.BonesLifetimeSeconds = 300
NpcBot.Wanderer.ReviveDelayMs = 300000
NpcBot.WanderingBots.Continents.Count = 0

###############################################################################
# ADVANCED BOT HEALER AI & OUT-OF-COMBAT TOP-OFF
###############################################################################
NpcBot.Healing.TopOff.Enable = 1
NpcBot.Healing.TopOff.HealthPct = 95
NpcBot.Healing.TopOff.MinManaPct = 25
NpcBot.Healing.TopOff.ScanIntervalMs = 750
NpcBot.Healing.Combat.ScanIntervalMs = 250
NpcBot.Healing.Emergency.OwnerHealthPct = 60
NpcBot.Healing.Emergency.GroupHealthPct = 50
NpcBot.Healing.FailedTargetRetryMs = 750
NpcBot.Wanderer.PlayerAid.Enable = 1
NpcBot.Wanderer.PlayerAid.Range = 40.0

###############################################################################
# AUTO-LEARN SPELLS & QUALITY OF LIFE
###############################################################################
AutoLearnSpells.Enable = 1
AutoLearnSpells.Announce = 1
Ghost.RunSpeedMultiplier = 3.0

###############################################################################
# RANDOM DUNGEON FINDER (RDF) WITH NPCBOTS
###############################################################################
NpcBot.Enable = 1
NpcBot.Enable.Dungeon = 1
NpcBot.Enable.DungeonFinder = 1
NpcBot.DungeonBots.Enable = 1
NpcBot.DungeonBots.Queue.WaitTime = 1
NpcBot.DungeonBots.Queue.RandomOnly = 0

###############################################################################
# RAID FINDER (RF) WITH NPCBOTS
###############################################################################
NpcBot.RaidFinder.Enable = 1
NpcBot.RaidFinder.WaitTime = 30
NpcBot.RaidFinder.MinItemLevel.Ratio = 1.0

###############################################################################
# ARENA MATCHMAKING WITH NPCBOTS
###############################################################################
NpcBot.Enable.Arena = 1
NpcBot.WanderingBots.Arena.Enable = 1
NpcBot.WanderingBots.Arena.TargetTeamSize.2v2 = 2
NpcBot.WanderingBots.Arena.TargetTeamSize.3v3 = 3
NpcBot.WanderingBots.Arena.TargetTeamSize.5v5 = 5

###############################################################################
# LAKE WINTERGRASP 40v40
###############################################################################
Wintergrasp.Enable = 1
Wintergrasp.PlayerMax = 40
Wintergrasp.BotWar.Enable = 1
Wintergrasp.BotWar.TargetPerTeam = 40

###############################################################################
# CITY RAIDS & ROAMING NEMESIS BOSSES
###############################################################################
CityRaid.Enable = 1
CityRaid.Cooldown = 7200
CityRaid.Duration = 1800
CityRaid.AttackerCount = 40
CityRaid.DefenderCount = 25
CityRaid.DefenderLeashRange = 65.0
CityRaid.BossLeashRange = 90.0
CityRaid.AutoStart = 1

NpcBot.DarkKnight.Enable = 1
NpcBot.DarkKnight.CountPerZone = 1
NpcBot.DarkKnight.RespawnTime = 300

NpcBot.WhoList.Enable = 1
```

---

## 🎮 In-Game Commands

### Player Commands
| Command | Description |
|---|---|
| `.companion [class]` | Summon or toggle a personal leveling companion bot (e.g. `warrior`, `paladin`, `mage`, `priest`, `hunter`, `druid`, `dk`, `shaman`, `warlock`, `rogue`, `bm`) |
| `.companion dismiss` | Dismiss your active companion bot |
| `.heirlooms` / `.companion heirlooms` | Receive your class's complete matching Heirloom equipment set and 4x Glacial Bags |
| `.raid queue <raid> [10\|25] [role]` | Queue for 10-man or 25-man Raid Finder with NPCBots (roles: `tank`, `healer`, `dps`) |
| `.raid status` | Check active Raid Finder queue status |
| `.raid leave` | Leave the Raid Finder queue |
| `.raid list` | List all available Raid Finder encounters, required levels and item score thresholds |
| `.npcbot add` | Hire an NPCBot companion into your party |
| `.npcbot remove` | Dismiss an NPCBot companion |
| `.npcbot info` | Display companion bot stats and equipment |

### GM & Administrative Commands
| Command | Description |
|---|---|
| `.npcbot dynamic status` | View active zones populated with dynamic wanderers |
| `.npcbot dynamic update` | Force an immediate zone-player check and refresh |
| `.npcbot dynamic reset` | Despawn dynamic wanderers and repopulate active zones |
| `.bf start 1` | Force start the Battle for Wintergrasp |
| `.bf stop 1` | Force end the Battle for Wintergrasp |
| `.revive <player>` | Resurrect a player with full/half HP |

---

## 🧪 Automated Regression Test Suite (53/53 Passing)

This repository includes a standalone test suite in `contrib/tests` covering C++20 policies and Python scenario tests:

```bash
python3 contrib/tools/run_custom_audit.py --jobs 2
```

**Coverage includes:**
* Dynamic Zone population budgeting, boundary clamping, and zone level ranges (`dynamic_zone_test.cpp`)
* Healer decision policies, emergency tiers, top-off ceiling, and cast cancellation (`healer_policy.cpp`, `healer_targets.py`)
* AoE and solo quest loot transfer integrity (`aoe_quest_transfer.py`)
* Resumable, transactional Heirloom delivery (`heirloom_delivery.py`)
* Wintergrasp vehicle cleanup, combat coordination, siege routes, and rank promotions (18 dedicated test cases)
* City raid defense alerts, group formations, lifecycles, and throne room navigation (9 dedicated test cases)
* Strand of the Ancients gate collision and Isle of Conquest teleporter logic

---

## 📦 Complete Database Dumps (Fresh Install)

Pre-packaged, clean database archives ready for immediate deployment are located in [`sql/database_dumps/`](sql/database_dumps/):

* **`auth.sql.gz`**: Clean authentication schema, default RBAC roles/permissions, build 12340 support, and default realmlist (no personal accounts).
* **`characters.sql.gz`**: Clean characters schema, system worldstates, preconfigured NPCBot entries with party enabled, custom heirloom ledger table, zero personal characters.
* **`world.sql.gz`**: Full, production-ready world database (creature templates, NPCBots extras, custom Companion Hearthstone item 70901, world bosses, waypoint meshes).

### Quick Database Import:
```bash
# 1. Create databases
mysql -u root -p -e "
CREATE DATABASE IF NOT EXISTS auth DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE DATABASE IF NOT EXISTS characters DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE DATABASE IF NOT EXISTS world DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
"

# 2. Import compressed archives
zcat sql/database_dumps/auth.sql.gz | mysql -u root -p auth
zcat sql/database_dumps/characters.sql.gz | mysql -u root -p characters
zcat sql/database_dumps/world.sql.gz | mysql -u root -p world
```

---

## 🚀 Building & Installation

### Requirements
* **Compiler:** GCC 9+ or Clang 10+ (C++17/20 support)
* **CMake:** 3.16 or newer
* **Database:** MySQL 5.7+ or MariaDB 10.3+
* **Libraries:** OpenSSL 1.1+, Boost 1.70+

### Linux Build
```bash
git clone --recursive https://github.com/BugnarM/TrinityCore-NPCBots-Complete-Edition.git
cd TrinityCore-NPCBots-Complete-Edition
mkdir build && cd build
cmake ../ -DCMAKE_INSTALL_PREFIX=/home/trinity/server
make -j$(nproc)
make install
```

---

## 📜 Credits & Acknowledgments
* **[TrinityCore Team](https://github.com/TrinityCore/TrinityCore):** Outstanding MMORPG server framework.
* **[Trickerer](https://github.com/trickerer):** Original creator and maintainer of the NPCBots mod.
* **[BugnarM](https://github.com/BugnarM):** Complete Edition architecture, AoE Looting, Dynamic Zone Wanderers, Healer AI & OOC Top-Off, 40v40 Wintergrasp Bot War, City Raids, and advanced PvP AI systems.

---

## 📄 License
This project is licensed under the **GNU General Public License v2.0** - see the [COPYING](COPYING) file for details.
