# TrinityCore 3.3.5a — NPCBots Complete Edition

Run a solo, co-op, or community **Wrath of the Lich King** realm with NPC companions, roaming adventurers, and bot-supported PvE and PvP. This fork builds on TrinityCore and Trickerer's NPCBots and uses the **3.3.5a client, build 12340**.

**Azeroth is already in the fight. Bring up to four companions and join in.** Take your party across zones where AI adventurers are out fighting and helping each other. A wandering healer might patch you up on the road, or you might round a corner and find a battle already underway. Pick a direction and see where your party ends up. **[Jump into the public test realm](#play-on-the-public-ptr).**

[Play on the public PTR](#play-on-the-public-ptr) · [Install on Linux](#install-on-linux) · [Configure the realm](#configuration) · [Player and GM commands](#player-and-gm-commands) · [Troubleshooting](#troubleshooting) · [Technical documentation](#technical-documentation)

## Play on the public PTR

Take a companion into the field, meet roaming adventurers while questing, or put together a bot-supported group for a dungeon. The public test realm runs this project, and the portal shows its current status. **[Open the realm portal and create an account](https://wow.remaris.ro/)**

1. Click **Instant Create & Log In**. You can choose your own credentials or leave the fields blank to generate them. The portal allows one PTR account per IP address.
2. Save your username and password in a password manager before leaving the dashboard. Under **Account recovery**, you can also generate a private, one-time code that restores dashboard access on its own.
3. If the realm shows **OFFLINE**, click **Start Server** after signing in and wait for **ONLINE**. The PTR starts on demand and goes offline after being idle.
4. In your **Wrath of the Lich King 3.3.5a client (build 12340)**, set the following in `Data/<locale>/realmlist.wtf`, for example `Data/enUS/realmlist.wtf`. Launch `WoW.exe` and log in with your PTR username and password.

   ```text
   set realmlist logon.remaris.ro
   ```

> [!IMPORTANT]
> **You must save your credentials.** We do not send usernames or passwords by email. If you lose both your password and recovery code, request a reset and keep your credentials out of the public issue. [Request an account reset on GitHub](https://github.com/BugnarM/TrinityCore-NPCBots-Complete-Edition/issues/new?template=ptr-account-reset.md)

Build a party with up to four companions, watch wanderers fight and assist nearby players, or join a bot-supported Wintergrasp battle. The PTR allows up to **150 wanderers per zone** and **400 worldwide**. Wanderers have a **3-minute** revive timer. Ordinary open-world combat spawns covered by the respawn update return in **2 minutes**. [Report a bug or share feedback](https://github.com/BugnarM/TrinityCore-NPCBots-Complete-Edition/issues/new)

## What you can do

| Feature | In game |
| --- | --- |
| **Personal companions** | Use the Companion Hearthstone to bring an NPC along. Manage its roles, equipment, and spells through its gossip menu. Choose **Heal** to make an eligible class act as a healer. |
| **Healing AI** | Priest, Paladin, Druid, and Shaman healers prioritize injured players, can interrupt offensive casts for emergencies, and top off health between fights. Friendly wanderers can also heal and resurrect nearby players. |
| **Open-world wanderers** | Level-appropriate bots spawn around players in occupied zones, with population limits, distance thresholds, and a spawn budget. Active bots can appear in `/who`. |
| **Dungeons and raids** | The standard Dungeon Finder can fill vacant roles after a configurable wait. The custom Raid Finder assembles 10- and 25-player rosters for supported raids such as ICC, Ulduar, Naxxramas, and Trial of the Crusader. |
| **Arena and battlegrounds** | Bot-supported 2v2, 3v3, and 5v5 arenas, objective and vehicle AI in Strand of the Ancients and Isle of Conquest, and optional Wintergrasp siege battles with a target of 40 per faction. |
| **Capital city raids** | Stormwind and Orgrimmar events use 40 attackers, configurable defense rosters, graveyard reinforcement waves, throne-room leashes, and rewards for eligible participants. |
| **Loot and leveling** | Eligible creature quest drops go into bags on kill. Opening a corpse can gather eligible nearby loot into one window. The realm can also grant starter heirlooms and bags, auto-learn trainer spells, and run ghosts at 3× movement speed. |

## Install on Linux

### 1. Build the core and extraction tools

The project uses C++20 and CMake 3.18 or later. The example below uses Ubuntu 24.04 with GCC 13, a local MySQL server, and a user-owned installation prefix. The [Linux workflow](.github/workflows/linux-build.yml) also defines Clang 17 builds.

```bash
sudo apt update
sudo apt install git build-essential cmake ninja-build python3 mysql-server \
  libmysqlclient-dev libssl-dev libreadline-dev zlib1g-dev libbz2-dev \
  libboost-dev libboost-filesystem-dev libboost-locale-dev \
  libboost-program-options-dev libboost-regex-dev libboost-thread-dev

git clone https://github.com/BugnarM/TrinityCore-NPCBots-Complete-Edition.git
cd TrinityCore-NPCBots-Complete-Edition

prefix="$HOME/trinity-server"
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_INSTALL_PREFIX="$prefix" \
  -DTOOLS=ON
cmake --build build --parallel 4
cmake --install build
```

Adjust the build parallelism to available RAM. For repeated builds, install `ccache` and pass `-DCMAKE_CXX_COMPILER_LAUNCHER=ccache` when configuring. Set the cache limit with `ccache --max-size=10G` if that fits your disk budget.

### 2. Prepare the game data

Use your own WotLK 3.3.5a client files. From the client directory, run the installed extraction tools in this order.

```bash
"$prefix/bin/mapextractor"
"$prefix/bin/vmap4extractor"
mkdir -p vmaps
"$prefix/bin/vmap4assembler" Buildings vmaps
"$prefix/bin/mmaps_generator"
mkdir -p "$prefix/data"
cp -a dbc maps vmaps mmaps "$prefix/data/"
```

The four generated directories belong under `DataDir`. The navigation meshes matter for bots following terrain, moving through cities, and reaching encounter objectives.

### 3. Create and import the databases

Use the included [fresh-install database archives and import instructions](sql/database_dumps/README.md) for a new `auth`, `characters`, and `world` database. The world archive includes NPCBot templates, the Companion Hearthstone, and custom event data. Create a dedicated MySQL user with privileges on those three schemas.

Before the first start, run the character migration command in [Upgrade an existing realm](#upgrade-an-existing-realm). The bundled character archive needs the delivery ledger created by `2026_11_17_00_characters.sql`, and both entries in the current character manifest use `CREATE TABLE IF NOT EXISTS`. For world and auth, compare each migration plan against the imported snapshot before applying entries.

### 4. Configure and start the realm

```bash
cp -n "$prefix/etc/authserver.conf.dist" "$prefix/etc/authserver.conf"
cp -n "$prefix/etc/worldserver.conf.dist" "$prefix/etc/worldserver.conf"
```

- Set `LoginDatabaseInfo` in both files, and `WorldDatabaseInfo` and `CharacterDatabaseInfo` in `worldserver.conf`. Use your database user's credentials and keep the configuration files private.
- Set `DataDir` in `worldserver.conf` to the quoted absolute path of the `data` directory. Set `SourceDirectory` to the checked-out repository if the built-in SQL updater needs to locate the source tree.
- For local play, the dump's `auth.realmlist` points to `127.0.0.1:8085`. For remote clients, set the realm address to a hostname or IP that they can reach.

From the repository directory, start each process in its own terminal or service with its configuration file.

```bash
"$prefix/bin/authserver" -c "$prefix/etc/authserver.conf"
"$prefix/bin/worldserver" -c "$prefix/etc/worldserver.conf"
```

After worldserver finishes loading, create a player account at its console with `account create <username> <password>`. In the client's active locale directory, set `realmlist.wtf` to `set realmlist 127.0.0.1` for local play, or use the realm's reachable hostname for remote play.

## Upgrade an existing realm

Use the checkout matching the binaries you intend to deploy. Back up all three databases and keep the current binaries and configuration before updating. Review the upstream TrinityCore SQL updates and the fork's custom SQL separately.

Custom migrations are tracked by [world](sql/custom/world/manifest.json), [characters](sql/custom/characters/manifest.json), and [auth](sql/custom/auth/manifest.json). The installer checks file hashes, dependencies, and the database ledger. A dry run lists entries absent from the ledger.

```bash
python3 sql/custom/apply_custom_sql.py \
  --scope characters --database characters --defaults-file /secure/mysql.cnf
```

Repeat the dry run for each scope using your schema names. Compare the plan with the imported snapshot or the realm's migration history. The world archive already includes custom content, so ledger entries can be absent for changes that are already present. The installer supports `--baseline-existing` for verified existing changes. That flag records **all** pending entries for the chosen scope, so use it only after checking every entry in the plan. For a database with a mix of applied and unapplied entries, reconcile the history against the manifests before making writes.

Once the plan contains only migrations that should run, stop the realm processes that use the affected schema and apply the reviewed plan. Use a MySQL option file accessible only to its owner.

```bash
python3 sql/custom/apply_custom_sql.py \
  --scope characters --database characters --defaults-file /secure/mysql.cnf \
  --apply --backup-dir /secure/trinity-backups
```

The installer produces a compressed backup before writes. Carry forward your local settings when merging new options from the distribution files. On startup, check the database update and NPCBot load messages before opening the realm to players.

## Configuration

The defaults below come from [worldserver.conf.dist](src/server/worldserver/worldserver.conf.dist). Values in an installed realm's `worldserver.conf` can differ.

| Setting | Distributed value | Effect |
| --- | ---: | --- |
| `NpcBot.WanderingBots.DynamicZone.Enable` | `1` | Enables player-centered wanderer spawning. |
| `NpcBot.WanderingBots.DynamicZone.MaxBots` | `50` | Caps the dynamic population across the realm. |
| `NpcBot.WanderingBots.DynamicZone.BotsPerZone` | `50` | Sets the per-zone target within the global cap and available spawn points. |
| `NpcBot.WanderingBots.DynamicZone.ProximityRadius` | `350.0` | Searches for spawn points near players, in yards. |
| `NpcBot.WanderingBots.DynamicZone.DespawnRadius` | `700.0` | Distance from players used to retire wanderers. |
| `NpcBot.WanderingBots.DynamicZone.UpdateInterval` | `10` | Seconds between population scans. |
| `NpcBot.WanderingBots.DynamicZone.SpawnBudget` | `10` | Maximum new dynamic bots per scan. |
| `NpcBot.Wanderer.ReviveDelayMs` | `180000` | Three-minute revive delay for wanderers eligible to respawn. City raid waves have their own timers. |
| `NpcBot.Wanderer.BonesLifetimeSeconds` | `300` | Bones become eligible for cleanup after five minutes. Removal occurs on a cleanup pass. |
| `NpcBot.Healing.Emergency.OwnerHealthPct` / `GroupHealthPct` | `60` / `50` | Emergency thresholds for a bot's owner and other group players. The group key is `NpcBot.Healing.Emergency.GroupHealthPct`. |
| `NpcBot.Healing.TopOff.HealthPct` / `MinManaPct` | `95` / `25` | Out-of-combat top-off ceiling and mana floor for topping off above 85% health. The mana key is `NpcBot.Healing.TopOff.MinManaPct`. |
| `NpcBot.Wanderer.PlayerAid.Range` | `40.0` | Range in yards for ambient aid. Duel and stealthed-caster aid are off by default. |
| `QuestLoot.AutoCollect` | `1` | Attempts to collect eligible creature quest drops on kill within `MaxGroupXPDistance` (74 yards by default). |
| `AOELoot.Enable` / `AOELoot.Range` / `AOELoot.MaxCorpses` | `1` / `120.0` / `60` | Enables nearby corpse looting, with the radius measured from the corpse you open. |
| `AutoLearnSpells.Enable` | `1` | Learns eligible class trainer spells and ranks on level-up. |
| `Rate.Ghost.MoveSpeed` | `3.0` | Ghost movement multiplier. |
| `Companion.Heirlooms.Enable` | `1` | Delivers class-specific heirlooms and starter supplies, including bags, at login. |
| `Companion.Stone.CooldownSeconds` | `18000` | Five-hour cooldown starts after filling all four Hearthstone slots or dismissing the roster. GM accounts bypass the cooldown. |
| `NpcBot.DungeonBots.Enable` | `0` | Enable to fill standard Dungeon Finder groups with bots after the configured wait. |
| `Wintergrasp.BotWar.Enable` | `0` | Enables Wintergrasp siege bots. The target per faction counts players and bots. |
| `NpcBot.DarkKnight.Enable` | `0` | Enables the optional Dark Knight world-boss roamers. |

<details>
<summary>Example wanderer profile for busier zones</summary>

```ini
NpcBot.WanderingBots.DynamicZone.MaxBots = 200
NpcBot.WanderingBots.DynamicZone.BotsPerZone = 100
NpcBot.WanderingBots.DynamicZone.ProximityRadius = 400.0
NpcBot.WanderingBots.DynamicZone.DespawnRadius = 700.0
NpcBot.WanderingBots.DynamicZone.UpdateInterval = 10
NpcBot.WanderingBots.DynamicZone.SpawnBudget = 10
```

Edit the active configuration and use `.reload config`. Population adjusts on subsequent scans up to the configured caps. Run `.npcbot dynamic status` to see the effective settings and current count. The per-zone target shares the global cap with other occupied zones, so actual populations depend on eligible players and spawn points.

</details>

## Player and GM commands

In game, commands start with a period. In the server console, omit it for commands supported there. In-game commands may require RBAC permissions.

| Command or action | Use |
| --- | --- |
| Companion Hearthstone, item `70901` / `.companion [class]` | Summon or toggle a leveling companion. For example, `.companion priest`. Summon a new companion on a base map before entering an instance. |
| `.companion dismiss` | Dismiss an eligible personal companion. |
| Companion gossip → **Manage Roles** | Toggle Tank, Heal, DPS, and other supported roles. The bot's class and specialization are separate from its active roles. |
| `.raid list` / `.raid queue <raid> [10|25] [tank|heal|dps]` / `.raid status` / `.raid leave` | Browse supported raids, enter the queue, check status, or leave. |
| `.npcbot info <player>` | List a player's owned bots known to the runtime and see which are active. |
| `.npcbot dynamic status` | Inspect current wanderer settings and population. |
| `.reload config` | Apply reloadable worldserver settings. |

## Troubleshooting

**The build fails after switching branches or changing dependencies.** Configure a fresh build directory and rebuild the intended revision. When reporting a problem, include the commit, OS, compiler, CMake version, configuration command, and the relevant compiler or linker output.

**My priest is in the group but does not heal.** Open **Manage Roles** and confirm **Heal** is enabled. Discipline and Holy are specializations. The Heal toggle enables the companion's group-healing routine. Then check its per-bot health threshold, healing spell toggles, mana, and access to the target. An injured player needs to be on the same map and phase, in range, and reachable for the spell. A controlled caster or a player in active GM mode can also block a cast.

**An owned bot is missing after a restart.** Check `.npcbot info <player>` and the startup log. If the log says the entry is missing from `creature`, check that entry's saved ownership and spawn records against a backup. The bot needs a loadable spawn to be attached again.

**A quest drop stayed on the corpse.** Auto-collection only attempts eligible quest-required drops for a recipient who still needs them, can loot the creature, and is in range. Group loot rules and bag capacity still apply. Open the corpse for remaining loot and items that use other drop rules.

**Dungeon Finder stays queued.** Check that `NpcBot.Enable`, `NpcBot.Enable.Dungeon`, `NpcBot.Enable.DungeonFinder`, and `NpcBot.DungeonBots.Enable` are enabled. Check the wait time, the random-only option, and whether the queue can fill all required roles with eligible players and bot templates.

## Technical documentation

| Area | Guide |
| --- | --- |
| Population, spawning and despawning | [Dynamic Zone Wanderers](doc/dynamic_zone_population.md) |
| Healer policy, quest loot, and persistence | [Implementation audit](doc/audit_2026_11_17/IMPLEMENTATION.md) |
| Companion and queue limits | [Player NPCBot limits](doc/npcbot_player_limits.md) |
| Dungeon and raid matching | [Dungeon Finder](doc/dungeon_finder_bots.md) · [Raid Finder and equipment](doc/raid_finder_and_organic_equipment.md) · [Raid movement and hazards](doc/raid_hazards_and_autoequip.md) |
| PvP and siege objectives | [Arena Bots](doc/arena_bots.md) · [Strand of the Ancients and Isle of Conquest](doc/battleground_bots_sa_ic.md) · [Wintergrasp](doc/wintergrasp_bot_war.md) |
| Capital city assaults | [City raid tactics and lifecycle](doc/city_raids_tactics.md) |
| Database snapshots and migrations | [Fresh database archives](sql/database_dumps/README.md) · [Custom SQL installer](sql/custom/apply_custom_sql.py) |

Run the standalone C++ policy tests from the repository root.

```bash
python3 contrib/tools/run_custom_audit.py --jobs 2 --output-dir /tmp/trinity-custom-audit-tests
```

For the native C++ unit suite, configure with `-DBUILD_TESTING=ON`, build the project, and run `ctest --test-dir build --output-on-failure`. Use in-game checks for client flows, encounter behavior, and tick performance under load.

## Credits and license

The TrinityCore team maintains the core, Trickerer maintains the upstream NPCBots integration, and BugnarM maintains this Complete Edition fork. [TrinityCore](https://github.com/TrinityCore/TrinityCore) · [NPCBots upstream](https://github.com/trickerer/TrinityCore-3.3.5-with-NPCBots) · [BugnarM](https://github.com/BugnarM)

This project is licensed under the GNU General Public License v2.0. [COPYING](COPYING)
