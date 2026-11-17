# Complete Database Dumps for TrinityCore + NPCBots (Fresh Install)

This directory contains full database archives for a clean (**fresh install**), with zero personal data (no player accounts, passwords, emails, or personal characters).

## Contents

1. **`auth.sql.gz`**
   - Full schema for the `auth` database.
   - RBAC tables (standard TrinityCore roles and permissions).
   - `build_info` for client version 3.3.5a (build 12340).
   - `realmlist` pre-configured with default realm (Realm 1: "Trinity", 127.0.0.1:8085).
   - No registered user accounts (ready for `account create <user> <pass>` from the server console).

2. **`characters.sql.gz`**
   - Full schema for `characters` tables.
   - System tables and worldstates (including initial state for `BOT_GIVER_ENTRY`).
   - `characters_npcbot` with all bot templates preconfigured (`roles` with party flag active, `owner = 0` / free).
   - Zero player characters (0 characters, inventories, or guilds).

3. **`world.sql.gz`**
   - Complete world database (equivalent to `world2`), ready to import as `world`.
   - Includes all creatures, quests, scripts, spells, and custom items (Companion Hearthstone `70901`, custom world bosses, wanderer waypoints, NPCBots extras).

---

## Import Instructions (Fresh Install)

On the target server with MySQL/MariaDB:

```bash
# 1. Create databases with utf8mb4 charset
mysql -u root -p -e "
CREATE DATABASE IF NOT EXISTS auth DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE DATABASE IF NOT EXISTS characters DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE DATABASE IF NOT EXISTS world DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
"

# 2. Import compressed archives directly into the databases
zcat auth.sql.gz | mysql -u root -p auth
zcat characters.sql.gz | mysql -u root -p characters
zcat world.sql.gz | mysql -u root -p world
```

The server is now ready to start with `authserver` and `worldserver`.
