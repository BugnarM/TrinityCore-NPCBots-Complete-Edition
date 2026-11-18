# Dungeon Finder with NPCBots

The standard Dungeon Finder attempts human matchmaking first. After a configurable wait, it keeps the largest compatible set of available players and reserves bots to fill one tank, one healer, and three DPS slots. Solo players, incomplete groups, eligible hired companions, and groups continuing an active instance can use bot fill.

## Configuration

Example for filling random and explicitly selected normal or heroic dungeons after one second:

```ini
NpcBot.Enable = 1
NpcBot.Enable.Dungeon = 1
NpcBot.Enable.DungeonFinder = 1
NpcBot.DungeonBots.Enable = 1
NpcBot.DungeonBots.Queue.WaitTime = 1
NpcBot.DungeonBots.Queue.RandomOnly = 0
```

The wait starts when players finish role selection and enter the queue. `0` permits immediate bot fill after a human matchmaking attempt. The maximum wait is 3,600 seconds, and negative values fall back to 45. An older queue entry can match compatible players who joined more recently. Queues are checked at most once per second.

With `RandomOnly = 1`, players must select a random dungeon or seasonal Dungeon Finder event. With `0`, they can also choose specific normal and heroic instances. The normal checks for level, equipment, lockouts, roles, faction, ignored players, and common eligible dungeons still apply. Compatible groups retain their players and role assignments. Disabling NPCBots, dungeon access, or Dungeon Finder integration stops bot fill.

Finder bots are separate from the non-GM personal companion limit. A solo player can receive four bots. A player who queues with a personal companion can receive three more when the roles fit. [Personal companion limits](npcbot_player_limits.md)

## Proposals and template reservations

Humans confirm each new proposal and bot slots accept automatically. Existing members continuing a group already in its instance retain native automatic acceptance. Bot placeholders have proposal-specific GUIDs and exist in the confirmation window. They stay outside the player queue, persistent participant state, and wait-time statistics.

Templates are checked and reserved before the proposal appears. The reservation matches class to role so that a flexible class cannot consume the only template needed for another role. It respects class minimum levels, enabled classes, and race filters. When the complete role roster cannot be reserved, the group stays queued. A template cannot serve two proposals at once or be borrowed by battleground or arena generation while reserved.

Declines, timeouts, and owner disconnects release the reservation. Failed teleports have a 120-second reservation timeout. Acceptance rechecks player presence and reservation availability.

Actual bots are created when the leader enters the instance. If the leader is already inside, replacements appear without requiring a map change. Assigned roles are retained for later replacements. Random groups and generated bots receive `Luck of the Draw`. Generated bots are excluded from achievements that count random human party members. The feature reuses the existing NPCBot template and equipment pools.

## Verification

```sh
g++ -std=c++20 -O2 -Wall -Wextra -Werror contrib/tests/dungeon_finder_bots.cpp -o /tmp/lfg-bots-test
/tmp/lfg-bots-test
cmake --build build --target worldserver --parallel 4
```

The standalone suite uses production policy for timing, role fill, template allocation, and GUID counters. It compares 12,000 class distributions with an independent exhaustive search and checks insufficient templates, incompatible roles, and counter wraparound. It can also run with AddressSanitizer and UndefinedBehaviorSanitizer.

## In-game checks

1. Queue as a solo DPS and confirm that the proposal appears after the configured wait. Accept and verify that the group receives a tank, healer, and two DPS bots.
2. Repeat as a solo tank or healer and with two to four participants. The final composition should be one tank, one healer, and three DPS.
3. Queue five compatible humans before the deadline and verify human matchmaking.
4. Create two simultaneous proposals and check independent responses, distinct GUIDs, and exclusive template reservations.
5. Decline, time out, leave the queue, and disconnect in separate runs. Check cleanup and normal requeue behavior for members who accepted.
6. Replace a departing member while the leader stays inside the dungeon. Check replacement role, gear, and cleanup on completion or departure.
7. Check disabled fill, insufficient templates, an explicit selection under `RandomOnly = 1`, and level and lockout restrictions.
8. Check teleportation, `Luck of the Draw`, and loot in a live client.
