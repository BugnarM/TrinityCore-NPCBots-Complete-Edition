# Dynamic Zone Wanderers

This system populates continental zones around active players using the existing WanderNode routes. Instances, battlegrounds, arenas, and players in flight or teleporting are excluded from the population scan. Populations are keyed by map and zone, including map ID 0.

## Configuration

The distributed settings are:

```ini
NpcBot.WanderingBots.DynamicZone.Enable = 1
NpcBot.WanderingBots.DynamicZone.BotsPerZone = 50
NpcBot.WanderingBots.DynamicZone.MaxBots = 50
NpcBot.WanderingBots.DynamicZone.UpdateInterval = 10
NpcBot.WanderingBots.DynamicZone.SpawnBudget = 10
NpcBot.WanderingBots.DynamicZone.ProximityRadius = 350.0
NpcBot.WanderingBots.DynamicZone.DespawnRadius = 700.0
NpcBot.WanderingBots.DynamicZone.SpawnSpacingRadius = 50.0
NpcBot.WanderingBots.DynamicZone.MaxNearbyBots = 8
```

The per-zone target accepts 0–100, the global limit 0–1,000, the scan interval 1–300 seconds, and the spawn budget 1–32 attempts per scan. The controller shares the global budget across eligible zones and rotates attempts among them. The actual population depends on suitable nodes, local spacing, available templates, and faction limits.

Set `NpcBot.WanderingBots.Continents.Count = 0` to run only dynamic continental populations. `NpcBot.WanderingBots.Continents.Maps` governs manual populations. When dynamic spawning is enabled at startup, the core loads the continental routes independently of that manual selection. If the feature was disabled at startup and maps were excluded from the manual list, restart to load their route data before enabling population there.

## Spawn selection and movement

- The controller searches in two dimensions around active players. If no eligible node is inside the proximity radius, the fallback searches up to the despawn radius in the same zone.
- A starting node needs a same-zone exit available to the selected faction. The bot's level comes from the node range, the class minimum, and reachable exit levels. For example, Death Knights are excluded from level 1–10 nodes. Bots use existing class- and level-appropriate wanderer equipment.
- The controller favors anchors with fewer nearby bots, then marked spawn points, then proximity to players. Regular road nodes can serve as fallback anchors. `BOTWP_FLAG_NOT_A_START_POINT` (`0x80000`) reserves a node for movement.
- `SpawnSpacingRadius` is the radius used to count nearby bots around each anchor, from 0 to 250 yards. Set it to 0 to disable spacing. `MaxNearbyBots` is a local cap from 0 to 100. Setting the cap to 0 still lets selection prefer quieter nodes.
- Patrol and return routes stay within the home zone. A bot that leaves its zone during combat is retired on a subsequent scan. When alternatives exist, route selection prefers nodes beyond the four most recent visits. Backtracking flags and link weights still apply.
- Departing players, lower population targets, and disabling the module trigger normal despawn and template cleanup. Map grids follow the core's normal unload timer.

Use `.npcbot dynamic status` or `.npcbot wander zone` for the population report. `.npcbot dynamic reset` retires current bots and lets occupied zones repopulate on subsequent scans. Spacing and population settings are read again after `.reload config`. SQL routes are loaded at startup.

## Waypoint data

The [route migration](../sql/custom/world/npcbot_2026_11_18_05_wanderer_route_refinement.sql) adds 23 waypoints and repairs routes in the open world. It adds ground routes in Storm Peaks, Icecrown, Sholazar Basin, Zul'Drak, Dragonblight, Netherstorm, and Deadwind Pass. Ulduar and Black Temple have local exterior circuits. The Naxxramas approach stays on the ground below the citadel. Tempest Keep requires a separate flight approach. Battleground and Wintergrasp objective graphs retain their directed links.

Use the custom migration manifest for a fresh install. On an existing realm, check the anchor nodes and prior migrations and back up the database before applying the route migration.

## Verification

`contrib/tests/dynamic_zone_test.cpp` exercises the production policy in `botdynamiczonepolicy.h`, including 19,440 class-level intersections and 144,600 population budget combinations. It also covers map and zone isolation, configuration bounds, and spawn priorities. Test player movement, spawn and despawn, phase visibility, and resource use in a live client with the realm's own route data.
