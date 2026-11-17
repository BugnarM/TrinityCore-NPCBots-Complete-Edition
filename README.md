# TrinityCore 3.3.5a – NPCBots Complete Edition

[![TrinityCore 3.3.5a](https://img.shields.io/badge/TrinityCore-3.3.5a%20(WotLK)-blue.svg)](https://github.com/TrinityCore/TrinityCore)
[![NPCBots Integrated](https://img.shields.io/badge/NPCBots-Trickerer%20Integrated-green.svg)](https://github.com/trickerer/Trinity-Bots)
[![C++17](https://img.shields.io/badge/C++-17-orange.svg)](https://en.wikipedia.org/wiki/C%2B%2B17)
[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-lightgrey.svg)](COPYING)

---

### 📢 Live Test Server Available Upon Request!
> **Interested in trying out this living world before building or installing it yourself?**  
> We can temporarily spin up a live public test realm for players, testers, and server developers to experience the autonomous bots, 40v40 Wintergrasp, Arena queues, RDF, and AoE looting firsthand!  
> 💬 **Open an Issue or start a Discussion on this repository to request temporary test server access!**

---

An advanced, production-grade distribution of **TrinityCore 3.3.5a (Wrath of the Lich King)** equipped with **NPCBots** and an extensive suite of custom autonomous AI systems. Designed for both solo-play enthusiasts and populated private servers seeking a truly living, breathing Azeroth without manual bot micromanagement.

---

## 🌟 What Makes This Edition Unique? (Custom Innovations)

This repository includes major custom systems, gameplay mechanics, and quality-of-life innovations that do **NOT** exist in vanilla TrinityCore 3.3.5 or base NPCBots:

### 1. ⚡ Modern Area-of-Effect (AoE) Looting (MoP-Style)
* **One-Click Multi-Corpse Gathering:** Right-clicking any dead NPC corpse automatically loots **all eligible dead corpses within a massive 120-yard radius** (up to 60 corpses in a single action).
* **Consolidated Money:** All copper, silver, and gold from every corpse in the area is merged into a single sum and distributed properly among players and bot companions.
* **Smart Stacking:** Identical stackable items (cloth, leather, meat, reagents) from different mobs are merged into clean stacks in the loot window.
* **Instant Quest Progression:** Quest items needed for your active quests are collected directly into your quest bags, updating objectives immediately.
* **Zero-Loss Guarantee:** If your bags are full or total items exceed client window limits (16 slots), remaining items safely stay on their source corpses (which continue to sparkle) so nothing is ever lost.

### 2. 🌍 Hybrid Dynamic Zone Wanderers (`botdynamiczone`)
Standard wanderer bots spawn across entire continents, wasting massive CPU and memory in empty zones.
* **Player-Centric Spawning:** Autonomous wanderer bots only spawn in the specific zone where human players are currently active.
* **Level-Appropriate Scaling:** Bots dynamically match the level range of the player's zone (e.g. Level 1–10 in Elwynn Forest, Level 40–50 in Tanaris, Level 70–80 in Northrend).
* **Automatic Resource Cleanup:** When players leave a zone, the wanderer bots gracefully despawn, freeing server memory and CPU cycles.

### 3. 🤝 Player Companion Bot & Companion Hearthstone
* **Companion Hearthstone (Item 70901):** Every adventurer receives a Companion Hearthstone to summon or toggle a personal leveling companion bot (instant first use, balanced 5-hour cooldown on subsequent uses; GMs have no cooldown and can command unlimited companions).
* **Full Interactive Gossip Dialog:** Right-clicking your companion bot opens standard NPCBot gossip menus directly in the world: **Manage Roles, Manage Equipment, Spells, and Add to Group**.
* **Active Open-World Wanderer Assistance:** Nearby friendly wanderer bots actively assist you in combat against quest mobs and hostile faction enemies!

### 4. 🎁 Auto-Granted Full Class Heirlooms & Starter Quality-of-Life
* **Every Starting Player Fully Equipped:** Upon character creation and first login, players are automatically equipped with the entire canonical WotLK Heirloom collection tailored to their class and specs.
* **+20% to +30% Experience Gain:** Includes 2x **Dread Pirate Rings** (`50255`, +5% XP each), plus class-matching **Shoulders** (`42949..42985`, +10% XP) and **Chests** (`48677..48691`, +10% XP).
* **Multi-Spec Weapon Arsenal:** Players receive a complete weapon arsenal (2H Axes, 2H Swords, 2H Maces, Staves, 1H Maces, 1H Swords, Daggers, Ranged Bows/Guns) covering all leveling specializations (e.g. Retribution/Holy for Paladins, Feral/Balance for Druids, Enhancement/Elemental for Shamans).
* **Instant Weapon Proficiencies:** Corresponding weapon skills are taught automatically so players never encounter "Weapon proficiency needed" roadblocks.
* **88+ Slots of Starter Inventory:** Automatically equips **4x Glacial Bags** (22 slots each, entry `41600`) into empty bag slots so players never run out of inventory room.
* **Northrend Cold Weather Flight Ready:** Every character receives a **Tome of Cold Weather Flight** (`49177`) in their bags ready for level 68.
* **On-Demand Command:** Existing players can claim or refresh their heirloom set anytime with `.heirlooms` (or `.companion heirlooms`).

### 5. 🛡️ Instant Random Dungeon Finder (RDF) with Bot Roles
* **Instant Group Formation:** Queue solo or with friends in the standard Dungeon Finder (Normal or Heroic).
* **Automatic Role Fill:** If human players aren't available, the queue instantly fills within seconds with an intelligent **Tank**, **Healer**, and **3 DPS** NPCBots.
* **Mechanic-Aware Combat:** Dungeon bots handle dispels, interrupts, aggro management, and boss encounter mechanics.

### 6. ⚔️ Full Arena Matchmaking System (2v2, 3v3, 5v5)
* **Instant PvP Queues:** Queue solo or in teams across all classic WotLK arenas (Nagrand, Blade's Edge, Dalaran Sewers, Ruins of Lordaeron, Ring of Valor).
* **Autonomous Opponents & Teammates:** AI teams execute real PvP tactics: Line-of-Sight (LOS) positioning, Crowd Control chaining (stuns, silences, polymorphs), and synchronized burst phases.

### 7. 🏰 Lake Wintergrasp 40v40 Epic Siege Warfare
* **Full-Scale 40v40 Warfare:** Lake Wintergrasp is transformed into an autonomous 40 Alliance vs. 40 Horde clash.
* **Autonomous Siege Vehicles:** Bots construct, drive, and aim **Catapults, Demolishers, and Siege Engines**.
* **Multi-Crew AI & Player Boarding:** Bots operate multi-seat vehicles (driver + passenger turret gunners). Human players can board bot-driven vehicles or drive while bots provide turret support or infantry escort.
* **Organized Wall Assaults:** Squad leaders and siege vehicles follow strategic lanes (Westpark, Eastpark, Fortress Gates) rather than wandering randomly.
* **WotLK Military Promotions:** Authentic rank progression (Recruit ➡️ Corporal at 5 kills ➡️ Lieutenant at 10 kills) required to build heavy war machines.

### 8. 🚢 Strand of the Ancients (15v15) & Isle of Conquest (40v40)
*(Under active development, fully operational)*
* **Strand of the Ancients:** Attackers pilot Demolishers from the beach, assault colored gates, and plant seaforium charges while defenders man turret cannons.
* **Isle of Conquest:** Autonomous battles across the Docks, Hangar (Gunship aerial drops), and Siege Workshop with fortress keep teleporter support.

### 9. 👑 Autonomous City Raids (Stormwind & Orgrimmar)
* **40-Man Armies:** Epic 40-attacker raids assemble with dedicated 5-man subgroups and healers to march on enemy capital cities.
* **Faction Leader Boss Tactics:** At King Varian Wrynn or Thrall, bots execute real raid tactics—main tank facing boss away, secondary tanks swapping aggro on high burst/mortal strike, and casters maintaining safe distance during Whirlwind.
* **Defense Alerts & Victory Chest:** Global city defense alerts broadcast to players, and defeating the city leader spawns a celebration reward chest with **Emblems of Triumph and Gold**.

### 10. 💀 Authentic Post-Battle Landscapes: 30-Min Skeletons & 5-Min Attrition Respawns
* **5-Minute Respawn Timer ("Last Man Standing"):** When bots die in open-world battles or Wintergrasp, they remain dead for 5 minutes, making territory control and tactical skirmishes meaningful.
* **30-Minute Skeletal Remains (`CORPSE_BONES`):** After reviving, bot skeletons persist on the battlefield for **30 minutes**, leaving realistic trails of war and skirmishes across Azeroth.

### 11. 👹 Dark Knight Nemesis Roamers (5-Man World Bosses)
* Roaming elite bosses hostile to both Horde and Alliance patrol key zones, offering spontaneous, challenging open-world dungeon encounters with rare loot rewards (can be enabled/disabled via `worldserver.conf`).

### 12. 👥 Living Social `/who` List Integration
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

Custom migrations use separate manifests for world, characters and auth, explicit
prerequisites, SHA-256 checksums and a database execution ledger. Plan first:

```bash
python3 sql/custom/apply_custom_sql.py --scope world --database world2 --defaults-file /secure/mysql.cnf
```

Apply with `--apply --backup-dir /secure/trinity-backups`. The defaults file must
have mode 600. Do not pass database passwords on the command line. On an existing
installation, use `--baseline-existing --apply` only after verifying that every
listed migration is already present. An incomplete migration blocks retries until
its state is reviewed; SQL files containing DDL may commit independently.

The `sql/updates/characters/3.3.5/2026_11_17_00_characters.sql` migration adds the
heirloom delivery ledger and preserves legacy completion markers. The core updater
can apply it; an identical migration is available in the custom characters manifest.
Do not replay all legacy custom SQL on an existing production database.


## ⚙️ Configuration Reference (`worldserver.conf`)

Add or adjust these settings in your `worldserver.conf`:

```ini
###############################################################################
# AREA OF EFFECT (AoE) LOOTING
###############################################################################
AOELoot.Enable = 1
AOELoot.Range = 120.0
AOELoot.MaxCorpses = 60

###############################################################################
# DYNAMIC ZONE WANDERERS
###############################################################################
NpcBot.WanderingBots.DynamicZone.Enable = 1
NpcBot.WanderingBots.DynamicZone.BotsPerZone = 8
NpcBot.WanderingBots.DynamicZone.UpdateInterval = 15
NpcBot.WanderingBots.DynamicZone.MaxBots = 2000
NpcBot.WanderingBots.DynamicZone.SpawnBudget = 50
NpcBot.WanderingBots.Continents.Count = 0

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
# ARENA MATCHMAKING WITH NPCBOTS
###############################################################################
NpcBot.Enable.Arena = 1
NpcBot.WanderingBots.Arena.Enable = 1
NpcBot.WanderingBots.Arena.TargetTeamSize.2v2 = 2
NpcBot.WanderingBots.Arena.TargetTeamSize.3v3 = 3
NpcBot.WanderingBots.Arena.TargetTeamSize.5v5 = 5

###############################################################################
# CITY RAIDS & ROAMING NEMESIS BOSSES
###############################################################################
CityRaid.Enable = 1
CityRaid.Cooldown = 7200
CityRaid.Duration = 1800
CityRaid.AttackerCount = 40
CityRaid.DefenderCount = 25
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
| `.npcbot add` | Hire an NPCBot companion into your party |
| `.npcbot remove` | Dismiss an NPCBot companion |
| `.npcbot info` | Display companion bot stats and equipment |

### GM & Administrative Commands
| Command | Description |
|---|---|
| `.npcbot dynamic status` | View active zones populated with dynamic wanderers |
| `.npcbot dynamic update` | Force an immediate zone-player check and refresh |
| `.bf start 1` | Force start the Battle for Wintergrasp |
| `.bf stop 1` | Force end the Battle for Wintergrasp |
| `.revive <player>` | Resurrect a player with full/half HP |

---

## 📦 Complete Database Dumps (Fresh Install)

Pre-packaged, clean database archives ready for immediate deployment are located in [`sql/database_dumps/`](sql/database_dumps/):

* **`auth.sql.gz`**: Clean authentication schema, default RBAC roles/permissions, build 12340 support, and default realmlist (no personal accounts).
* **`characters.sql.gz`**: Clean characters schema, system worldstates, preconfigured NPCBot entries with party enabled, zero personal characters.
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
* **Compiler:** GCC 9+ or Clang 10+ (C++17 support)
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
* **[BugnarM](https://github.com/BugnarM):** Complete Edition architecture, AoE Looting, Dynamic Zone Wanderers, 40v40 Wintergrasp Bot War, City Raids, and advanced PvP AI systems.

---

## 📄 License
This project is licensed under the **GNU General Public License v2.0** - see the [COPYING](COPYING) file for details.
