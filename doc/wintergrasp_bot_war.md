# Wintergrasp Bot War

Wintergrasp Bot War fills vacant battlefield slots with NPCBots during an active battle with human participants. Bots use class combat AI and receive battlefield orders for infantry, workshop capture, defense, and siege. The native battle schedule and entry rules still apply.

## Configuration

Set these options in `worldserver.conf` to enable a battle with up to 40 participants per faction:

```ini
Wintergrasp.Enable = 1
Wintergrasp.PlayerMax = 40
Wintergrasp.PlayerMin = 0
Wintergrasp.PlayerMinLvl = 77
Wintergrasp.BattleTimer = 30
Wintergrasp.NoBattleTimer = 150
Wintergrasp.BotWar.Enable = 1
Wintergrasp.BotWar.TargetPerTeam = 40
```

Wintergrasp and Bot War are disabled in the distributed configuration. `PlayerMax` is clamped to 1–120. The effective number of bots on each side is:

```text
max(0, min(TargetPerTeam, PlayerMax, 120) - enrolled human players)
```

With a target of 40, 17 humans leave room for at most 23 bots on that side. Human arrivals take priority. Dead human players still count as participants, while bot replacements are delayed. Vehicle capacity is separate and comes from controlled workshops. Drivers and gunners count toward the faction's roster.

At least one human must accept entry to an active battle before the bots appear. Queuing for a future battle does not start population. Bots are retired when the last human participant leaves. The first population update is delayed, and new squads are added in batches. To change the battlefield's constructed capacity, schedule a worldserver restart.

For a fresh installation, review the [world migration manifest](../sql/custom/world/manifest.json). It includes the original battlefield setup, NPCBot spawn nodes, and later waypoint links. Use extracted maps, vmaps, and mmaps for Northrend. [Base setup](../sql/custom/world/2026_10_23_00_wintergrasp_bot_war.sql) · [Native spawn nodes](../sql/custom/world/npcbot_2026_11_09_01_wintergrasp_native_bots.sql) · [Waypoint links](../sql/custom/world/npcbot_2026_11_15_01_wintergrasp_wander_links.sql)

## Infantry, workshops, and siege

- Squads keep stable orders as the roster changes. Attackers use central, western, and eastern approaches and defend the three southern towers. Defenders contest the northern workshops, cover fortress entrances, and fall back as objectives are lost.
- Attacker flanks stage at southern camps and secure nearby workshops before using them as forward vehicle bases. Workshop captures use the native progress bar and speed limits. Infantry contributes and vehicle riders are excluded from capture.
- Drivers and gunners use native vehicle spells. Catapults target infantry, while Demolishers and Siege Engines can damage buildings. Defenders prioritize dangerous siege vehicles. Construction depends on controlled workshops, engineers, rank, and vehicle capacity.
- Vehicle routes use ground navigation and collision checks for the center and sides of a vehicle. Recovery uses shorter paths, lateral detours, and bounded retries for stalled movement.
- Friendly players can take vehicle seats. Cleanup preserves vehicles occupied by humans, including turret gunners. Check handoff and resumption of bot orders in a client.
- Infantry waits at intact gates and walls. Defenders fall back toward the Titan Relic when the vault gate falls. Attackers can claim the relic after native unlock, within range and line of sight and after staying out of combat.

| Lane | Main objectives |
| --- | --- |
| Center | South staging → bridge → gate 190375 → inner wall 191805 → vault gate 191810 → relic 192829 |
| West | Broken Temple → western bridge → wall 190371 → inner wall → vault gate → relic |
| East | Sunken Ring → eastern ramp or bridge → wall 190372 → inner wall → vault gate → relic |
| Defender counterattacks | Fortress exterior → southern towers 190356, 190357, or 190358 → nearby workshops |

Historical technical notes cover changes to [vehicle attacks and capture](wintergrasp_aggression_2026_11_08.md), [workshops and ranks](wintergrasp_realism_2026_11_08.md), [recovery and siege](wintergrasp_upgrades_2026_11_08.md), [native wanderers](wintergrasp_native_wanderers_2026_11_09.md), and [vehicle navigation](wintergrasp_vehicle_navigation_2026_10_26.md).

## Game objects and spell IDs

| Vehicle or object | Native spell |
| --- | --- |
| Demolisher | Hurl Boulder 50896 (projectile 50999), Ram 54107 |
| Siege Engine | Ram 54109 |
| Siege turret | Fire Cannon 57609 |
| Tower cannon | Fire Cannon 51421, with a ground target |
| Catapult | Plague Barrel 57606, Flame Breath 50989 |

The battlefield roster, vehicle orders, and relic logic live in `src/server/scripts/Northrend/zone_wintergrasp.cpp`. Population and squad policy are shared through `wintergrasp_bot_war.h`. Native building, workshop, vehicle-cap, and player-cap handling lives in `src/server/scripts/Battlefield/BattlefieldWG.cpp`.

## Verification

```sh
c++ -std=c++20 -O2 -Wall -Wextra -pedantic contrib/tests/wintergrasp_bot_war.cpp -o /tmp/wg-test
/tmp/wg-test
```

The standalone suite checks capacity, human priority, and squad assignment. Use the `bg.battlefield` logs and a game client to verify mixed human and bot battles.

1. Enter Wintergrasp and start a test battle with `.bf start 1`. Accept the battle invitation and check population and vehicle counters in the log.
2. Observe workshop capture, gate and wall order on each lane, building damage, and movement over bridges and ramps.
3. Test vehicle handoff for drivers and turret gunners. Verify human-occupied vehicles survive roster cleanup.
4. Destroy all three southern towers and confirm native timer and objective changes. Check roster replacement and release of vehicle slots.
5. Breach the vault gate and verify relic interruption, victory, faction reversal, and access to the Vault of Archavon.
6. Use `.bf stop 1` and repeat for the other faction. Measure update time before raising the roster above 40 per side.

Visual route quality and performance at 120 participants per side require separate in-game validation.
