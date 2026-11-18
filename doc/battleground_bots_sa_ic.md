# Strand of the Ancients and Isle of Conquest bots

Wandering NPCBots fill vacant slots through the battleground queue, up to 15 participants per team in Strand of the Ancients (607) and 40 in Isle of Conquest (628). Human players count toward these targets. The native battleground template sets the final limit. Generated bots use their class AI, equipment, healing, and PvP rotations. The tactical controller applies to wandering bots in these maps.

## Example configuration

```ini
NpcBot.WanderingBots.Continents.Count = 0
NpcBot.WanderingBots.BG.Enable = 1
NpcBot.WanderingBots.BG.CapLevel = 1
NpcBot.WanderingBots.BG.TargetTeamPlayersCount.SA = 15
NpcBot.WanderingBots.BG.TargetTeamPlayersCount.IC = 40
```

## Routing and objectives

The tactical controller reads gate and objective state once per second, runs Dijkstra on the SQL waypoint graph, and uses native pathfinding for movement. Intact enemy gates block graph edges and physical passage. Bots keep a waypoint until they reach it, and lateral offsets separate formation members. The [base migration](../sql/custom/world/2026_10_23_01_battleground_bots_sa_ic.sql) reserves IDs 5200–5226 on map 607 and 5300–5322 on map 628. Coordinate changes to waypoint IDs with the tactical rules and tests.

### Strand of the Ancients

- Both teams stay at their starts during preparation. After the round starts, attackers advance through Green/Blue, Purple/Red, Yellow, Ancient, and the relic. Breaching a side gate can open the corresponding stage.
- Drivers keep healthy demolishers and can search both harbors for reachable replacements. Infantry captures graveyards, escorts vehicles, or attacks the front. Defenders relocate to the active gate, use verified wall descents and native teleporters, and can occupy both cannons.
- A vulnerable relic takes priority over uncaptured flags. Designated capturers stay on foot and finish interactions. Gate assaults use vehicle weapons and native seaforium charges from bomb depots.
- Flag and relic interactions use native casts with interruption. The server validates range, line of sight, phase, team, target object, and bot participation.
- Round changes evacuate vehicles, resurrect bots, clear crowd control and tactical orders, swap roles, and apply preparation. The preparation aura is removed when the round starts. Destroyed gates and demolishers use the battleground's scoring rules.

See the technical notes on [movement and preparation](sa_movement_and_preparation_2026_10_26.md), [gate collision](sa_gate_collision_and_defense_2026_10_26.md), [objective capture](sa_objective_capture_2026_10_27.md), and [vehicle roles](sa_vehicle_roles_and_retention_2026_10_27.md).

### Isle of Conquest

- Infantry uses keep teleporters in both directions for both factions. The bot rejoins the waypoint graph after transfer without being blocked by waypoint edges on closed gates. [Keep teleporter notes](ioc_keep_teleporters_2026_10_26.md)
- Eight logical groups of five are assigned by ID across Workshop, Docks, Hangar, Quarry, and Refinery. Captured bases keep sentries, most Workshop and Docks groups escort siege, and a reaction group responds to contested objectives.
- Flags use the same flow as players, with a cast, contested banner, 60-second timer, capture or defense, messages, scoring, vehicles, and spirit guides. Only fully controlled graveyards with an active spirit guide can resurrect bots. Quarry and Refinery provide resource bonuses, which are refreshed after changes of control or resurrection.
- After a breach, infantry advances toward the commander. Assigned bots try to capture the keep graveyard first, and some groups defend their own commander when their keep is breached.
- Death consumes reinforcements once, including falls. NPCBot vehicle kills are attributed to the driver. Commander deaths use the native victory flow.
- The Hangar group can use native transport teleportation and gunship cannons. Catapults can launch bot passengers through the native jump event.

## Vehicles and explosives

Bots occupy existing battleground vehicles, including passenger and turret seats. Healers usually remain on foot. A friendly player near the vehicle has priority and the bot can give up its seat. Vehicle AI selects spells from the actual vehicle template. Shots, rams, bombs, and gate damage use native effects. Cannons prioritize nearby enemy vehicles and lead moving targets. Dismounting restores the vehicle faction. Ground crews disembark for the final relic or commander assault.

SA bots stage on the beach. The cinematic boat ride remains a player flow, and autonomous bomb disarming is outside the current AI. Catapult launches target the exterior front. Rooftop and parapet infiltration and tactical parachute drops require client validation.

## Implementation entry points

| Area | Source |
| --- | --- |
| Tactical controller and policy | `src/server/game/AI/NpcBots/bot_battlegrounds.cpp` and `botbgtactics.h` |
| Native rounds, captures, graveyards, and scoring | `src/server/game/Battlegrounds/Zones/BattlegroundSA.cpp` and `BattlegroundIC.cpp` |
| Interaction cast completion and vehicle kill credit | `src/server/game/Spells/SpellEffects.cpp` and `src/server/game/Entities/Unit/Unit.cpp` |
| Isle of Conquest transport and catapult launch | `src/server/scripts/Northrend/IsleOfConquest/isle_of_conquest.cpp` |

## Verification

```sh
c++ -std=c++20 -O2 -Wall -Wextra -pedantic contrib/tests/siege_battlegrounds.cpp -o /tmp/siege-bg-test
/tmp/siege-bg-test
cmake --build build --target worldserver --parallel 4
```

The suite reads the production graph and checks gate-state combinations, access to the relic and commanders, exits from a team's own keep, reciprocal links, and coverage of the five bases. The base SQL was also applied twice on a separate MySQL database with sentinel rows outside its reserved ranges.

Use a battlemaster to queue in a client. `.debug bg` toggles minimum-player testing rules. Verify both SA rounds, preparation and role swaps, objectives, vehicles, and relic victory. In IC, verify the contested timer, resource buffs, resurrection, keep breach and interior graveyard, commander victory, and reinforcement victory. In mixed matches, check vehicle handoff, interrupted casts, kill attribution, transport movement, and blocked waypoints. Measure performance at full participation separately.
