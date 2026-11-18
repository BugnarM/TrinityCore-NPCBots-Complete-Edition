# Raid Finder and organic equipment

Raid Finder builds 10- and 25-player groups for supported Wrath raids, using eligible human players first and NPCBots to fill vacant roles.

## Queue and configuration

```text
.raid list
.raid queue icc 25 heal
.raid queue naxx 10 tank
.raid queue ulduar 10 dps
.raid status
.raid leave
```

`.raid find` and `.rf` are aliases. Raid aliases include `toc`/`totc` and `ony`/`onyxia`. When the role is omitted the finder infers it from specialization, presence, or form. A group leader can queue eligible, connected members. Premade groups stay together and retain their declared roles.

Enable NPCBots and raid access before enabling the finder:

```ini
NpcBot.Enable = 1
NpcBot.Enable.Raid = 1
NpcBot.RaidFinder.Enable = 1
NpcBot.RaidFinder.WaitTime = 30
NpcBot.RaidFinder.MinItemLevel.Ratio = 1.0
```

The finder is disabled by default. The average item-level thresholds are 160 for T7, 187 for T8, 200 for T9, and 219 for T10. The ratio scales those thresholds, and 0 disables the item-level requirement. Native access requirements still apply. Vault of Archavon requires Wintergrasp control outside an ongoing battle.

## Matching and entry

A complete human raid can launch immediately. After the wait, the finder reserves templates for the missing roles. If the full complement is unavailable, the request stays queued. Eligibility is rechecked for level, equipment, combat, teleports, vehicles, other queues, group membership, access requirements, difficulty, and permanent lockouts. The finder creates fresh raids at supported normal difficulties.

Entrances come from `areatrigger_teleport` through `GetMapEntranceTrigger()`. Leaving, logging out, or changing the group cancels the request or reservation. Players have 120 seconds to arrive. Bots spawn after all humans enter the same instance. A failed creation is cleaned up, and spawned bots are removed when the owner leaves the raid or map or when the feature is disabled.

The 3.3.5 client receives queue feedback through chat and existing Raid Browser and group packets.

## Organic equipment

For a selected active companion, an organic preset such as `.npcbot autoequip organic t10` applies role- and specialization-aware selection within the preset's limits. Organic presets can mix tiers to meet equipment and set-bonus targets. Attached companions without real equipment can receive organic gear after their level is set. The planner checks item required levels against the bot and the receiving player. [AutoEquip and raid hazards](raid_hazards_and_autoequip.md)

For implementation history, the earlier audit records the original rollout and its verification. [October 2026 audit](archive/audits/prompt_audit_2026_10_25.md)
