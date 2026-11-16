# Wintergrasp Vehicle Map (WoW 3.3.5a)

Copy the `WGVehicleMap` directory to the client's `Interface/AddOns/` directory
and enable **Wintergrasp Vehicle Map** on the character selection screen.
Restart the client, or use `/reload` if the addon was already installed.

During a battle, open the Wintergrasp map (`M`) or battlefield map (`Shift+M`).
Blue squares are allied vehicles; red squares are enemy vehicles. The letters
mean **C**atapult, **D**emolisher and **S**iege Engine. Hover for the vehicle type
and allegiance. Both bot and player vehicles are included, even outside normal
view distance. Fixed fortress cannons and vehicle accessory turrets are excluded.

Requires a worldserver built with the vehicle map updater in `WintergraspBotWarMapScript`. The server sends
complete snapshots once per second to players in Wintergrasp, independently of
`Wintergrasp.BotWar.Enable`. Vehicles disappear from the next snapshot when
destroyed or despawned. An empty snapshot clears the map after battle; leaving
the zone and a five-second data timeout also clear pins.

Wire protocol: private `LANG_ADDON` whispers with prefix `WGVM1`, delivered from
the receiving player's own GUID. `B;sequence` starts a snapshot;
`V;sequence;team,kind,x,y|...` contains rows; `E;sequence` commits it. Coordinates
are Wintergrasp map fractions multiplied by 10000, rounded to integers; team
0 is Alliance, 1 is Horde; kind 1/2/3 is catapult/demolisher/siege engine. Messages
including the prefix stay below the 255-byte client limit. Partial snapshots
are discarded and never replace the last complete map.
