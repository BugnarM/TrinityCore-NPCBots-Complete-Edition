# City raids: tactics, lifecycle and integration

The implementation is in `src/server/scripts/Events/city_raids.cpp`; the shared native-AI protocol and deterministic tactical policies are in `src/server/game/AI/NpcBots/botcityraid.h`. Events 86/87 retain their faction IDs, named SQL NPCBot prototypes, level 80, 40 attackers and 15 additional defenders (55 total). The attacking raid size is fixed at 40; a lower `CityRaid.AttackerCount` is ignored with a warning. Before marching, the coordinator verifies all 40 unique attackers are present and belong to the attacker group; defenders receive a separate roster/group check. A successful assembly logs the attacker, defender and total counts. Each waypoint advance and resurrection wave also logs its objective/location. Native class rotations, equipment, healing, buffs, immunities, mana and spell cooldowns remain authoritative.

## Corrections found during review

- The roster used literal spec IDs that did not identify the intended NPCBot specializations. It now uses the actual `BOT_SPEC_*` constants. Generation fails and rolls back the event if the requested faction/class pool is exhausted; a missing healer is never replaced with an arbitrary class.
- A collection of ungrouped wanderers is not a native raid. Both armies now receive temporary native groups, allowing group healing, buffs and dispels. Attackers have one healer per five-member subgroup; defender subgroups match the three checkpoints. These groups do not write character group rows or instance bindings, and are disbanded before bots are queued for deletion. Native subgroup changes now update the active bot group reference, correcting an existing reference mismatch. The manager stores group GUIDs rather than pointers that can dangle after external disbanding.
- Cooldowns now advance while idle. Manual event rising/falling edges are handled without continuously restarting a finished raid. The global game-event scheduler now skips `GAMEEVENT_INTERNAL` events, as their documented contract requires; it previously auto-stopped manually started raids on its next update. Apply `sql/custom/world/npcbot_2026_11_11_01_city_raid_internal_events.sql` for existing event rows. Automatic activation requires an actual non-GM player in city zone 1519/1637.
- Resurrection is event-owned: 20-second attacker waves and 30-second defender waves, corpse bones and ground-projected graveyard positions. Wave phases retain elapsed-time carry across irregular ticks. Independent NPCBot self-resurrection/group resurrection no longer bypasses the wave. Returning attackers traverse the route to the current frontline without engaging targets in transit. Stormwind now uses DBC safe location 107 at its gate, rather than the original mislabeled Northshire coordinate. Staging uses the Stormwind entrance at DBC safe location 1. Attackers can only pursue enemies inside a 60-yard corridor around the current march segment; defenders stay within 65 yards of their checkpoint. Stale off-route victims are dropped before controller movement executes.
- The recent native DPS combat movement and dynamic defender fallback changes are preserved. Healers retain safe positioning during ordinary guard fights; boss formation overrides native combat repositioning.
- Real installed mmaps exposed disconnected rooftop/ramp destinations. Stormwind staging and keep approach, and Orgrimmar's defender graveyard and Valley of Wisdom approach, were corrected. A dedicated per-raid 8192-node Detour query handles long city corridors that can exhaust the normal 1024-node query. The global map query is unchanged.
- Paths use actual Detour corridor corners, with the native 74-poly/74-point limits. Invalid paths, absent mmaps, shortcut paths and overflow shortcuts are rejected. Incomplete prefixes are followed and replanned when their spline ends. Accepted valid endpoints are cached rather than recalculated continuously.
- The added global defense-alert cooldown map was unsafe across map threads. Producers now queue area/faction values; the world thread deduplicates and broadcasts after map workers finish. Throttling is independent per area and defending faction, at most once per 60 seconds; the pending queue is bounded to 256 distinct alerts.

## Boss phase

One main tank receives taunt permission and a stable pull destination facing away from the approach. Standby tanks stay on the flanks. Melee uses a rear arc and evacuates to 15 yards during Varian's actual Whirlwind spell 41056. Eight healers use a 19-yard ring; fourteen ranged DPS use a 30-yard ring. The ideal combined caster formation has more than 10 yards between positions. Local navigation/line-of-sight probes avoid blindly marching casters through walls.

DPS waits for the tank to acquire the boss, a two-second lead and nontrivial native threat. The coordinator requests a tank swap below 35% health, after a 25-percentage-point burst, or below 70% with a substantial healing reduction, provided a replacement has at least 60% health. An eight-second swap cooldown prevents oscillation. Human tanks are respected. Taunts use normal learned spells and cooldowns; no threat values are fabricated. Taunt immunity still applies.

Native dispel selection prioritizes removable control effects, healing reductions and tanks, while respecting each actual cure spell's dispel mask, range and line of sight. Physical or otherwise undispellable stuns/debuffs are not forcibly removed.

## Victory, rewards and retreat

Victory requires a raid-confirmed boss kill, or a boss observed alive and engaged before death. An already dead city leader never awards victory. Victory plays sound 8454/8455 and sends faction war yells to players in the city zone.

The temporary celebration chest uses templates 990086/990087. Apply `sql/custom/world/npcbot_2026_11_10_04_city_raid_rewards.sql` to the world database before running the new binary. Its AI intercepts normal chest loot. Defaults award two Emblems of Triumph (47241) and ten gold per eligible character. Eligibility requires at least 20 sampled seconds alive in active attacking-side combat near the raid, level 80 and proximity to the boss at victory. This measures combat presence; it is not a damage/healing attribution system. Bag space and the money cap are checked before awarding; repeated clicks are serialized and cannot claim twice. Characters are saved through normal native persistence. Claims and chests are event-local, with no custom durable reward ledger.

Duration expiry stops offensive behavior and resurrection. Attackers withdraw backwards from their nearest reached waypoint; defenders withdraw toward their city graveyard. Cleanup has a 90-second upper bound. Manual stop and shutdown disband groups and remove event bots/chests.

```ini
CityRaid.Enable = 1
CityRaid.AutoStart = 1
CityRaid.Duration = 1800
CityRaid.Cooldown = 7200
CityRaid.AttackerCount = 40
CityRaid.DefenderCount = 25
CityRaid.Reward.Emblems = 2
CityRaid.Reward.Gold = 10
CityRaid.Reward.Seconds = 180
```

## Threading and performance contract

`MapManager::Update` joins its worker updates before `WorldScript::OnUpdate` runs in this branch. The coordinator publishes orders, creates/disbands groups, mutates bot prototype storage and handles claims on the world thread at that barrier. Creature controllers apply combat and spline operations on their own map thread. No map-thread death callback mutates the event manager. The boss-kill signal and navigation-token counter are atomic; reward clicks and defense alerts use short protected queues carrying GUIDs/values rather than retained entity pointers.

The dedicated navigation query is shared only by controllers on the same continent map, where creature updates are serial. It is held with the event runtime until the final controller is destroyed. `PathGenerator::SetNavMeshQuery` rejects a query attached to another map's mesh. If map update scheduling is changed to parallel creature updates within one map, this ownership model must change with it.

Spawns are staggered at two per 250-ms decision. The user's revised movement budget of 64 controller path queries per decision per city is retained. Normal native class movement can issue additional queries. Steady controller movement does not explicitly call `LoadGrid`; spawning and the initial graveyard projection do. These limits avoid unbounded script work, but do **not** establish a guarantee of zero engine grid locks or zero frame spikes. Actual server tick timings under simultaneous raids and players still require a live load test.

## Validation

```sh
c++ -std=c++20 -O2 -Wall -Wextra -Werror contrib/tests/city_raid_tactics.cpp -o /tmp/city-tactics
/tmp/city-tactics
cmake --build /home/trinity/build/npcbots --target worldserver -j4
```

The historical Python harnesses compiled coordinator, controller, and queue methods with engine fakes. The navigation check used the installed navigation libraries to verify 32 ground positions and 62 routes against `/home/trinity/server/data/mmaps`, including incomplete-prefix progress with the native corridor and point caps. [Historical test sources](https://github.com/BugnarM/TrinityCore-NPCBots-Complete-Edition/tree/292c4317c6/contrib/tests)

The remaining in-game checks are both bosses with real guards, terrain-constrained caster spread and healing LoS, human tank participation, resurrection during combat, chest eligibility, manual stop/restart and tick latency with both cities active. A full build plus these tests does not substitute for that encounter/load validation. The forward-routing follow-up prepares a controlled deployment with the old executable, relevant database rows and logs backed up before restart.

## Live deployment: 2026-11-11

The repaired executable and reward/internal-event migrations were installed with a controlled restart. Backup: `/home/trinity/source-archives/city-raids-inward-20261111-081320`. Authserver remained running.

Live event 86 assembled exactly 40 attackers and 15 additional defenders. The log recorded consecutive waypoint advances 1 through 9, a coordinated tank-swap request, and raid attacker Gennia (940241, druid, level 80) killing King Varian Wrynn (29611, level 83). The 55 event bots were listed in Stormwind, with no bots in the Elwynn listing during this check. This validates the full Stormwind approach and boss kill; the corresponding Orgrimmar routes passed navmesh and combat-policy tests but were not separately played through live in this validation.

The saved live log and `live-validation.json` are in the backup directory. Nine focused test suites passed, including the scheduler regression and the Echo Ridge/Goldshire combat-detour rejection. Normal low-level dynamic-zone wanderers remain independent of city raids and can generate their own PvP/defense activity.
