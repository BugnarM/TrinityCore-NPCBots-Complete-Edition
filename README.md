# TrinityCore 3.3.5a – NPCBots Complete Edition

[![TrinityCore 3.3.5a](https://img.shields.io/badge/TrinityCore-3.3.5a%20(WotLK)-blue.svg)](https://github.com/TrinityCore/TrinityCore)
[![NPCBots Integrated](https://img.shields.io/badge/NPCBots-Trickerer%20Integrated-green.svg)](https://github.com/trickerer/Trinity-Bots)
[![C++17](https://img.shields.io/badge/C++-17-orange.svg)](https://en.wikipedia.org/wiki/C%2B%2B17)
[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-lightgrey.svg)](COPYING)

An advanced, production-grade distribution of **TrinityCore 3.3.5a (Wrath of the Lich King)** equipped with **NPCBots** and an extensive suite of custom autonomous AI systems. Designed for both solo-play enthusiasts and populated private servers seeking a truly living, breathing Azeroth without manual bot micromanagement.

---

## 🌟 Key Features

### 1. 🌍 Dynamic Zone Wanderers (`botdynamiczone`)
Standard wandering bot implementations spawn hundreds of entities across the entire world, consuming server CPU and memory even in zones where no player ever sets foot.
* **Player-Centric Dynamic Spawning:** Wandering bots only spawn in zones where active human players are present.
* **Level-Appropriate Scaling:** Bots dynamically match the level range of the player's zone (e.g., Level 1–10 in Elwynn Forest, Level 70–74 in Dragonblight, Level 80 in Icecrown).
* **Automatic Resource Cleanup:** When all human players leave a zone, the associated wanderer bots automatically despawn gracefully after an idle cooldown.
* **Configurable:** Controlled entirely via `worldserver.conf`:
  ```ini
  NpcBot.WanderingBots.DynamicZone.Enable = 1
  NpcBot.WanderingBots.DynamicZone.BotsPerZone = 8
  NpcBot.WanderingBots.DynamicZone.UpdateInterval = 15
  ```

### 2. 🏰 Wintergrasp 40v40 Bot War (`BattlefieldWG`)
A full-scale autonomous siege warfare system turning Lake Wintergrasp into an epic 40 Alliance vs 40 Horde clash:
* **Tactical Lane Progression:** Center, West, and East lanes coordinate assaults across bridges, workshops (*Broken Temple*, *Sunken Ring*), and fortress gates.
* **Autonomous Siege Engines:** Bots operate Catapults, Demolishers, and Siege Engines with multi-crew AI (driver navigation + passenger turret gunners).
* **Mounted Cavalry:** Bots mount epic PvP war mounts (*Black War Steed* for Alliance, *Black War Wolf* for Horde) at 200% movement speed between distant objectives and automatically dismount when engaging enemies.
* **Human-like PvP Tactics:** CC-breaking with PvP Medallions (`42292`), role-specific combat rotations (Warriors, Frost Mages, Healer Priests, Hunters), and priority target switching.
* **Fortress Breaching:** Progressive demolition of Outer Gates (`190375`), Courtyard Gates (`191805`), Vault Gates (`191810`), and ultimate assault on the Titan's Relic (`192829`).

### 3. ⚔️ Populated Battlegrounds & Dungeons
* **Full Battleground AI:** Support for Warsong Gulch, Arathi Basin, Alterac Valley, Strand of the Ancients, and Isle of Conquest with automated tactical routing and siege navigation.
* **Dungeon & Raid Solo Play:** Seamlessly recruit 5-man party or 10/25/40-man raid companion bots with dedicated tanking, healing, and DPS rotations.

---

## ⚙️ In-Game Commands

### NPCBots Dynamic Zone
| Command | Description |
|---|---|
| `.npcbot dynamic status` | View active zones populated with dynamic wanderers |
| `.npcbot dynamic update` | Force an immediate zone-player check and refresh |
| `.npcbot dynamic enable / disable` | Toggle dynamic wanderer spawning on the fly |

### Wintergrasp Battlefield Control (GM)
| Command | Description |
|---|---|
| `.bf start 1` | Force start the Battle for Wintergrasp |
| `.bf switch 1` | Trigger instant victory for Attackers (Breaches relic, opens VoA) |
| `.bf stop 1` | Trigger instant victory for Defenders (End by timer) |
| `.bf timer 1 <seconds>` | Set remaining battle duration |

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
* **[BugnarM](https://github.com/BugnarM):** Dynamic Zone Wanderers, Wintergrasp Bot War architecture, and Complete Edition integration.

---

## 📄 License
This project is licensed under the **GNU General Public License v2.0** - see the [COPYING](COPYING) file for details.
