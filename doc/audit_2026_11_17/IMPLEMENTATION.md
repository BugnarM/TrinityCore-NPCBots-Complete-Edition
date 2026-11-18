# NPCBots audit implementation, November 17, 2026

This report records the changes delivered after the [original audit](../archive/audits/2026-11-17/README.md). The deployment manifest records the source revision, build identity, and checksums for binaries and configuration.

## Implemented changes

| Audit | Implementation |
| --- | --- |
| H1 | The emergency healing path excludes free bots before accessing the owner as a Player. It checks map, phase, assist eligibility, and line of sight. Offensive cast interruption bypasses the old 75% roll and requires a known heal that is ready, affordable, and available in its spell school. |
| H2 | Heal start and cancellation use the same out-of-combat and personal ceilings. An eligible heal at 91–94% is allowed to finish. Both units must be out of combat for top-off. Native costs and the global cooldown still apply. |
| H3–H5 | Living targets from solo and group members, pets, vehicles, and marked units share an eligibility check. GUIDs are deduplicated and resolved again before casting. Candidates are ranked by urgency and preferences within each tier. The comparator uses cached values and failed casts fall through to the next target. |
| H6 | Above 85% health, top-off chooses the cheapest available direct heal. Regrowth checks its own aura outside emergencies. Druids with both Heal and Tank roles can still heal from feral forms. |
| H7 | A per-update BotMgr snapshot holds group caster GUIDs. Current spells and their targets are read live. Staggered scans, per-target retry timers, and reusable storage bound repeated work. Optional diagnostics aggregate decisions, candidates, failures, interruptions, and p50/p95/p99/max timings. |
| L1 | Loot owns quest-item transfer, validates an unconsumed quest list, and maintains source and destination counters. The primary corpse is also consolidated. Free-for-all, conditional, group, and already viewed items stay on their original corpse. Loot rights and range to the primary corpse are checked before transfers. |
| C1 | BotMgr attaches a companion before committing ownership. Selection excludes offline-owned templates. The class switch prepares a replacement and attempts to restore the previous companion if attachment fails. Stone calls are serialized and persist cooldown after success. Dismiss and switch ignore temporary Finder bots and remove generated companion creatures after detachment. |
| C2 | The character/item ledger resumes partial heirloom deliveries and records granted items even if they are later sold. Inventory, ledger, and completion marker are written in one InnoDB transaction. Legacy markers remain valid. Rogue, Priest, Mage, Warlock, and Druid weapon proficiencies were corrected. |
| D1 | The Dark Knight timer uses elapsed scan time. Dynamic spawn fallback stays inside the despawn radius. Floating-point settings are validated and bounded. |
| I1 | The custom installer tracks world, characters, and auth separately with SHA-256 hashes, dependencies, a verified gzip backup, a migration ledger, and database locking. Applied migrations are skipped and mismatches or incomplete migrations stop the installer. Character schema changes are shipped as SQL migrations. |
| T1 | GM companion and city-raid tests were updated, with extra coverage for healer targeting, quest loot, heirloom delivery, and migration manifests. |
| G1 | The release manifest generator and optional CMake ccache/PCH integration were added. The deployment used a persistent 10 GB cache. A PCH probe recorded a cache hit with `pch_defines,time_macros`. |

## Behavior and settings

The distributed values are documented in [worldserver.conf.dist](../../src/server/worldserver/worldserver.conf.dist). This audit used the settings below. The later Wanderers defaults and route guidance are maintained in [Dynamic Zone Wanderers](../dynamic_zone_population.md).

- Out-of-combat top-off ceiling of 95%. The 25% mana floor applies to optional top-off above 85%. Emergency healing has a separate path.
- Healing scans at 750 ms out of combat and 250 ms in combat, with a 750 ms retry and a GUID-based stagger.
- Wanderer aid within 40 yards. By default, ambient aid excludes duelists and avoids breaking the healer's stealth or invisibility. Ambient resurrection looks for a route around obstacles.
- Ghost speed multiplier of 3×, with existing ghosts recalculated on reload.
- At the time of this audit, bones were eligible for cleanup after 1,800 seconds. The wanderer revive delay was 300,000 ms. City raids and Wintergrasp retain separate resurrection timing.
- AoE loot radius of 120 yards and up to 60 corpses, with validated inputs and an administrative maximum of 300.
- Companion Stone cooldown of 18,000 seconds, with first-use bypass. Starter heirlooms can be disabled.
- City defenders use a 65-yard leash and the boss uses a 90-yard event boundary. The events cover Stormwind and Orgrimmar.
- Wintergrasp uses a player limit and bot target of 40 per faction with human priority.

AutoLearn counts successful spell changes, including ranks, reactivations, and trainer wrappers. It logs a warning if the 15-pass limit is reached while there is still progress.

`LoadBotCreatureFromDB` requires a base map with instance ID 0. The stone now returns a controlled error when asked to create a companion inside an instance. Summon before entering and use normal bot teleportation. GM accounts can bypass the personal cap, subject to technical group and counter limits.

The updated path avoids treating the historical free-bot `master` alias as a Player. It retains Unit and Group reads on their existing update threads and avoids a global cache of Unit pointers.

## Verification and deployment history

```sh
python3 contrib/tools/run_custom_audit.py --jobs 2 --output-dir /tmp/trinity-custom-audit-tests
python3 contrib/tests/aoe_quest_transfer.py --sanitize
python3 contrib/tests/heirloom_delivery.py --sanitize
python3 contrib/tests/healer_targets.py --sanitize
cmake --build build --target worldserver authserver --parallel 4
```

At the time of the audit, the final runner passed 53 of 53 cases. The three new harnesses and the healer policy passed ASan/UBSan. LeakSanitizer was disabled under ptrace. Live encounter behavior, pathing, duels, stealth, logout, teleport, and MSPT under load require client or runtime tests.

A temporary MariaDB run applied the character migration and created the ledger. A second run skipped the applied SQL. The deployed character migration followed a verified backup, preserved existing character and marker counts, and used the existing InnoDB inventory, item, character, and marker tables.

A deployment manifest can be generated with:

```sh
python3 contrib/tools/trinity_release_manifest.py \
  --build /path/to/build --install /path/to/server \
  --output /path/to/backup/release.json
```

The verified upstream revision was `a824f510b72012de45b27c7ccf574dc424a25483`. This fork started from a squashed import and had no merge base with that upstream. Reconciliation requires an explicit provenance base for importing and comparing changes.
