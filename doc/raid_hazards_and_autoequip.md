# Raid hazards and NPCBot AutoEquip

## Hazard avoidance

`bot_ai::CheckHazardAvoidance()` runs before normal class rotations and movement in `GlobalUpdate()`. Every 250 ms it checks hostile dynamic objects and creature triggers near the bot, including Defile, Slime Puddle, Choking Gas Bomb, Shadow Trap, and Legion Flame. Defile and Slime Puddle use encounter scaling. Legion Flame uses the 3-yard radius from damage spell 66877.

The bot prefers a 5-yard sidestep and considers 32 directions, favoring short exits. For larger hazards it extends the destination outside the perimeter with a safety margin and a 40-yard limit. The destination must clear all nearby hazards. The routine checks line of sight, height, and every segment of the mmap route. A bot can leave a hazard that already contains it. Routes that enter another hazard or carry the bot farther into its current hazard are rejected.

A valid exit interrupts the current cast and takes movement priority. If no safe path exists, normal class AI can continue. Hazard avoidance applies in dungeons and raids, including Finder bots. Controlled bots, teleports, jumps, and encounter vehicle passengers are excluded. `FULLSTOP` and `INACTION` suppress avoidance. `STAY` permits emergency movement. Effects with more than 4 yards of vertical separation are ignored. This is reactive avoidance of supported ground effects.

## AutoEquip

Select one of your active companions. Both owner and bot must be out of combat, and the bot must be alive and clear of vehicles and teleports. The command requires RBAC permission `RBAC_PERM_COMMAND_NPCBOT_CREATENEW` (70036). It applies to bots in the caller's BotMgr.

```text
.npcbot autoequip
.npcbot autoequip blue
.npcbot autoequip t7
.npcbot autoequip t8
.npcbot autoequip t9
.npcbot autoequip t10
.npcbot autoequip wrathful
.npcbot autoequip 264
.npcbot autoequip organic t10
```

| Preset | Primary item-level band | Selection |
| --- | --- | --- |
| `blue`, `dungeon`, `preraid` | 187–200 | Rare gear, with eligible epic item-level 200 relics |
| `t7`, `naxx` | 200–213 | Five pieces from the same Tier 7 ItemSet |
| `t8`, `ulduar` | 219–232 | Tier 8 armor, originally item level 219 or 226 |
| `t9`, `toc` | 232–258 | Tier 9 for the owner's faction |
| `t10`, `icc` | 251–277 | Five compatible Tier 10 pieces |
| `wrathful`, `pvp` | 251–284 | Wrathful gear, including compatible elite weapons |
| Number from `1` to `284` | Cap minus 15 through cap | Strict item-level cap and optional set bonuses |

Presets are case-insensitive. `auto` and `default` use a level-scaled selection below 80 and the blue preset at 80. Named endgame tier and Wrathful presets require level 80. Custom classes can use blue, level-scaled, or numeric selection. For optional tier mixing, see [organic equipment](raid_finder_and_organic_equipment.md).

Planning uses the native equipment rules, allowed item catalog, level, class, spec, role stat weights, and separate entries for rare weapons and compatible relics. It checks two-handed weapons, off-hand compatibility, Titan's Grip, unique items, and shared uniqueness categories across normal and heroic variants. PvP uses Wrathful set pieces and PvP accessories, with PvE relics where appropriate for the role. If an accessory has no suitable item in the primary band, the command can use a lower one and reports how many slots took that fallback.

The replacement is planned and allocated before existing gear is removed. The companion's shirt is retained. Native unequip handling returns player items to inventory or by mail when inventory is full. Generated items are bound, eligible items receive enchants, and normal equipment handling updates stats, combat procs, and character database persistence. Existing gear is retained on ordinary attachment when already suitable for the bot's level.

The [AutoEquip help migration](../sql/custom/world/npcbot_2026_10_24_00_autoequip.sql) installs help for `.help npcbot autoequip`.

## Verification

```sh
g++ -std=c++20 -O2 -Wall -Wextra -Werror -pedantic contrib/tests/hazard_avoidance_and_autoequip_test.cpp -o /tmp/hazard-autoequip-test
/tmp/hazard-autoequip-test
cmake --build build --target worldserver --parallel 4
```

The suite includes the production `bothazard.h` and `botautoequip.h` headers. It checks 181,704 routes across 8,000 randomized layouts and samples each path independently. Equipment tests cover invalid presets, item-level thresholds, five-piece set selection, and uniqueness conflicts. The suite also supports AddressSanitizer and UndefinedBehaviorSanitizer.

The historical world catalog check found five slots for 111 eligible ItemSets (19 Tier 7, 19 Tier 8, 38 Tier 9, 19 Tier 10, and 16 Wrathful). In a client, check growing Defile, Legion Flame, overlapping hazards near walls, and resumption of healing or DPS after leaving the hazard. Test shields, Fury dual wield, caster weapons, full bags, and gear persistence after relog or restart.
