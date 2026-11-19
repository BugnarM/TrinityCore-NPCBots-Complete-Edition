# Personal companions and Finder group limits

`NpcBot.MaxBots.NonGM` controls how many personal companions a non-GM account can recruit and control. A value of `4` limits the character to four active personal companions (allowing a full 5-player party with 1 Tank, 1 Healer, and 2 DPS). The check applies during recruitment, through the recruiter menu, when taking control, and when reattaching an owned or shared bot. GM security is evaluated for the account independently of `.gm on/off`. The normal recruitment path retains the GM level-based setting in `NpcBot.MaxBots`. The Companion Hearthstone has its own GM exception. Normal hire costs and eligibility rules still apply.

A character with more previously owned companions than the current limit can reattach only up to that limit at the same time. Existing ownership and equipment are retained. Use the NPCBot recruiter or the Companion Hearthstone to obtain a companion.

Temporary Dungeon Finder bots are outside the personal cap. A normal or heroic party has five slots, with one tank, one healer, and three DPS. A solo player can receive four bots, or three more when queuing with a personal companion and compatible roles. Additional human participants reduce the number of bot slots. Generated bots stay in the instance when the player teleports out and follow the normal Finder cleanup on completion or group exit. [Dungeon Finder](dungeon_finder_bots.md)

## Example configuration

```ini
NpcBot.MaxBots.NonGM = 1
NpcBot.DungeonBots.Enable = 1
NpcBot.DungeonBots.Queue.WaitTime = 1
NpcBot.DungeonBots.Queue.RandomOnly = 0
```

## Verification

```sh
c++ -std=c++20 -O2 -Wall -Wextra -Werror contrib/tests/npcbot_player_limits.cpp -o /tmp/npcbot-player-limits
/tmp/npcbot-player-limits
c++ -std=c++20 -O2 -Wall -Wextra -Werror contrib/tests/dungeon_finder_bots.cpp -o /tmp/npcbot-rdf-limits
/tmp/npcbot-rdf-limits
```

These tests exercise the production limits, previously owned companions, GM and temporary-bot exemptions, group fill, and incompatible roles. Use a client to verify recruitment on a standard account and teleportation into a dungeon.
