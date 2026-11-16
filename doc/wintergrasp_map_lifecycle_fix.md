# Wintergrasp: hartă și activarea armatelor la intrarea jucătorilor

Corecție din 26 octombrie 2026.

## Cauză

Au fost înregistrate două `WorldMapScript` pentru Northrend (571): armatele de
boți și, ulterior, pozițiile vehiculelor. Macro-ul `SCR_MAP_END` din ScriptMgr.cpp
face `return` după primul script compatibil. În funcție de ordinea parcurgerii
registrului, harta putea actualiza numai vehiculele, fără armatele de boți,
deși BattlefieldWG avea o bătălie activă și invita jucătorii să participe.

Pozițiile vehiculelor sunt acum gestionate de `WintergraspVehicleMapUpdater`,
un membru al unicului `WintergraspBotWarMapScript`. Acesta îl apelează înaintea
verificărilor pentru armate, astfel încât actualizarea/curățarea hărții să
funcționeze și între bătălii sau cu BotWar dezactivat.

## Activare numai cu participanți reali

Armatele sunt create numai dacă bătălia este activă, BotWar este activat și
`GetWarPlayerCount` raportează cel puțin un jucător real într-una dintre echipe.
Acceptarea invitației la luptă este evenimentul relevant; simpla prezență în
zonă sau acceptarea cozii înainte de luptă nu este suficientă.

Nu este necesar ca jucătorul să fie prezent la pornirea programată: intrarea
mai târziu activează armatele în aceeași bătălie. Boții apar după aproximativ
patru secunde, apoi cel mult zece pe facțiune/secundă. La plecarea ultimului
participant real, roster-ul este curățat. O revenire reia popularea. Calendarul,
clădirile și condițiile native de victorie nu sunt schimbate.

## Verificare

```sh
python3 contrib/tests/wintergrasp_map_lifecycle.py
c++ -std=c++20 -O2 -Wall -Wextra -Werror contrib/tests/wintergrasp_bot_war.cpp -o /tmp/wg-population-human-gate
/tmp/wg-population-human-gate
c++ -std=c++20 -O2 -Wall -Wextra -Werror contrib/tests/wintergrasp_vehicle_cleanup.cpp -o /tmp/wg-cleanup-human-gate
/tmp/wg-cleanup-human-gate
```

Toate trec. Testul nou compilează metoda `OnUpdate` extrasă din sursa reală cu
dependențe simulate; verifică înregistrarea unică, lipsa armatelor fără oameni,
prima intrare întârziată, actualizarea bugetului la alte intrări, plecarea
ultimului om, revenirea, sfârșitul luptei, restart rapid, dezactivare și
actualizarea hărții în fiecare situație. Nu simulează clientul sau deplasarea
efectivă a soldaților.

Compilarea completă a reușit fără avertismente:
`/tmp/wg-human-gate-build.log`. Binarul este instalat pentru următoarea repornire;
procesul care rulează deja trebuie repornit pentru a prelua modificarea.
Pentru validare în joc, intră după începutul luptei, acceptă invitația și
urmărește logurile `Wintergrasp BotWar` după aproximativ 15 secunde; apoi
părăsește zona cu ultimul participant și verifică dispariția armatelor.
