# Arena bots

NPCBots fill vacant slots in 2v2, 3v3, and 5v5 teams in Nagrand (559), Blade's Edge (562), Ruins of Lordaeron (572), Dalaran Sewers (617), and the Ring of Valor (618). Solo players and incomplete groups can queue. The bracket determines the team size. Pending groups are rechecked every five seconds until an invitation is issued or they leave the queue.

## Configuration

In `worldserver.conf`:

```ini
NpcBot.Enable = 1
NpcBot.Enable.Arena = 1
NpcBot.WanderingBots.Arena.Enable = 1
NpcBot.WanderingBots.Arena.TargetTeamSize.2v2 = 2
NpcBot.WanderingBots.Arena.TargetTeamSize.3v3 = 3
NpcBot.WanderingBots.Arena.TargetTeamSize.5v5 = 5
```

Use `0` to disable bot fill for a bracket. Other values normalize to the bracket size. The generator needs available class templates, independently of continent and battleground bot generation. If either team cannot be filled, the proposal is abandoned.

Rated queues still verify arena team membership. Matches with generated participants are practice matches. They leave rating, MMR, and rated achievements unchanged, and declining the invitation carries no rating penalty. The generated opponent has `ArenaTeamId = 0`. Full human teams use normal rated matchmaking.

## Navigation and combat

The [arena mesh migration](../sql/custom/world/2026_10_24_00_arena_bot_mesh.sql) is transactional and repeatable. It replaces only its reserved IDs and installs 11 nodes on each of the five maps. Each graph has two spawn points, reciprocal links, and cover nodes marked `BOTWP_FLAG_ARENA_COVER`.

Bots use the starting position assigned by matchmaking and pick the waypoint graph for the actual instance after teleporting. Gold/Green flags and ally/enemy relationships follow the match team, including for pets and same-faction opponents. In the Ring of Valor, the map timer moves bots still at the base of the elevators to the arena floor when the platform animation ends.

Team AI selects a shared target using health and defensive effects. It avoids crowd-controlled targets without attackers and can use Polymorph, Blind, Fear, or Hammer of Justice on an enemy healer outside the main focus. Native spell checks apply to costs, cooldowns, range, immunities, and line of sight. Bots prioritize available defensive abilities below 30% health. Shield Wall requires the appropriate stance and a shield.

Generated healers seek cover when attacked by a caster. A candidate must break enemy line of sight, preserve sight of the patient, and be reachable on the navmesh. Healing an ally below 40% takes priority. Line of sight and moving obstacles are checked at runtime.

Eliminated bots remain dead until the match ends. Falls, pets, and environmental deaths trigger the victory check after kill credit. Simultaneous team elimination results in a draw. Victory checks are suspended during preparation.

## Verification

From the repository root:

```sh
g++ -std=c++20 -O2 -Wall -Wextra -Werror "$PWD/contrib/tests/arena_bots.cpp" -o /tmp/arena-test
/tmp/arena-test
cmake --build build --target worldserver --parallel 4
```

The standalone suite exercises production policies for team fill, configuration normalization, team flags, and victory. It checks faction-compatible SQL routes and rejects missing, isolated, and cross-map edges.

```sql
SELECT mapid, COUNT(*), SUM((flags & 1048576) != 0) AS cover_nodes
FROM creature_template_npcbot_wander_nodes
WHERE id BETWEEN 5400 AND 5490
GROUP BY mapid;
```

Expected totals and cover counts are 559 (11/4), 562 (11/2), 572 (11/4), 617 (11/2), and 618 (11/4).

In a client, check each map and bracket with a solo player, a full group, and an incomplete group where applicable. Verify participant counts, starting positions, gates, flags, hostile reactions, and same-faction teams. Check wins after the player dies, invitation declines, queue exits, disconnects, falls, respawn rules, and practice rating. Observe movement on bridges, pipes, and elevators.
