# Isle of Conquest: folosirea teleporterelor fortărețelor

Corecție din 26 octombrie 2026, pentru NPCBots autonomi din IC.

Blocajul raportat la Hoardă are două puncte tratate acum explicit:

- Apropierea de platformă nu mai depinde de vizibilitatea centrului obiectului.
  La distanța de interacțiune, AI-ul identifică platforma pereche activă a
  aceleiași facțiuni și cel mai apropiat world trigger nativ 22515 de lângă ea.
  Transferul folosește acel punct de sosire și efectul vizual de teleportare.
  Nu este o teleportare de recuperare: cere platforme reale, active, în faza
  botului, participare la lupta activă, infanterie vie, fără transport, vehicul,
  combat, cast sau control. Fără platformă ori trigger, transferul este refuzat.
- După ieșire, cel mai apropiat waypoint putea fi exact pe o poartă intactă.
  Verificarea fizică respingea spline-ul până la acel punct și botul rămânea
  lângă teleporter. Alegerea punctelor de intrare/ieșire din graf și păstrarea
  checkpointului verifică acum și segmentul față de ziduri și porți. Vechiul
  checkpoint este curățat după transfer.

Regulile acoperă cele trei perechi ale fiecărei facțiuni, în ambele sensuri.
După o breșă proprie, mecanismul existent poate continua pe jos. Jucătorii
folosesc în continuare interacțiunea nativă. Nu sunt necesare SQL sau config noi.

## Verificări

- `python3 contrib/tests/ioc_keep_teleporters.py`: compilează și execută
  funcția reală de transfer și ramura reală de alegere a platformei, folosind
  coordonatele din `BattlegroundIC.h` și migrarea nativă cu cele 14 triggeruri.
  Verifică toate cele 12 platforme, direcțiile, distanța, facțiunea, fazele,
  starea luptei, combat/control/cast, lipsa destinației și continuarea după transfer.
- `python3 contrib/tests/siege_movement_regression.py`: plannerul real și SQL-ul
  grafului; include ieșirile native ale ambelor fortărețe și checkpointuri vechi
  către porți închise. Codul anterior reproduce eșecul la reatașarea după teleport.
  Verifică în continuare traseele și pregătirea din Strand of the Ancients.
- `contrib/tests/ioc_navigation_data.cpp`: datele reale Detour/mmaps din server,
  56 tile-uri compatibile, 23 noduri, 64 trasee direcționate și apropierea de
  toate cele 12 platforme fără traversarea zidurilor intacte.
- `contrib/tests/siege_battlegrounds.cpp`: toate cele 64 stări ale porților,
  ambele echipe și verificările existente de acces și vehicule.
- Compilarea țintei `worldserver` și `git diff --check`.

Testele controllerului simulează obiectele și mișcarea; testul Detour utilizează
geometria reală. Confirmarea vizuală a unei partide cu boții rămâne de făcut în joc.
