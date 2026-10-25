# RDF automat cu NPCBots

Completarea automată se face în coada normală Dungeon Finder, după un interval
configurabil. Matchmaking-ul încearcă întâi să formeze grupuri cu jucătorii
existenți. După expirarea intervalului, păstrează combinația compatibilă cu cei mai
mulți participanți disponibili și rezervă boții necesari pentru un tank, un healer
și trei DPS. Sunt acceptate înscrierea solo, grupurile incomplete, boții angajați
incluși în grup și continuarea unei instanțe deja începute.

## Configurare

```ini
NpcBot.Enable = 1
NpcBot.Enable.Dungeon = 1
NpcBot.Enable.DungeonFinder = 1
NpcBot.DungeonBots.Enable = 1
NpcBot.DungeonBots.Queue.WaitTime = 45
NpcBot.DungeonBots.Queue.RandomOnly = 1
```

`WaitTime` se măsoară în secunde, de la intrarea în coadă, după alegerea rolurilor.
Valoarea `0` permite completarea imediată după încercarea de matchmaking uman;
limita superioară este 3600 de secunde. Valorile negative folosesc implicit 45.
Un participant mai vechi poate forma un grup și cu jucători compatibili care au
intrat mai recent în coadă. Cozile sunt reverificate cel mult o dată pe secundă.

Cu `RandomOnly = 1`, participanții umani trebuie să fi selectat un dungeon random
sau un eveniment sezonier RDF. Valoarea `0` permite și selecții explicite de
instanțe normale/eroice. Nu se creează raiduri prin acest mecanism.

Distribuția păstrează controalele existente pentru roluri, facțiune, ignorări,
intersecția dungeon-urilor eligibile și verificarea inițială a restricțiilor LFG.
Nu sunt eliminate roluri sau membri pentru a forța completarea unui grup
incompatibil. Dezactivarea modulului, accesului NPCBots în dungeon sau integrării
Dungeon Finder oprește completarea automată.

## Propunerea și șabloanele de boți

Fiecare propunere nouă cere confirmarea jucătorilor umani. Numai locurile
completate cu boți sunt acceptate automat. Pentru continuarea unui grup deja în
instanță se păstrează acceptarea automată nativă a membrilor existenți.

Locurile de boți din fereastra de confirmare au GUID-uri distincte între propuneri.
Ele există exclusiv în propunere: nu intră în coada de jucători, nu primesc stare
persistentă de participant și nu intră în statisticile timpilor de așteptare.
Șabloanele sunt verificate și rezervate înainte de afișarea propunerii. Alocarea
folosește matching între clase și roluri, astfel încât o clasă flexibilă să nu
consume singurul șablon disponibil pentru un alt rol obligatoriu.

Rezerva respectă nivelul minim al clasei, clasele activate și filtrarea raselor.
Dacă nu poate forma întregul complement de roluri, grupul rămâne în coadă fără o
propunere incompletă. Șabloanele rezervate nu pot fi împrumutate simultan de altă
propunere sau de generatorul de boți pentru battleground-uri/arene.

Refuzarea sau expirarea propunerii eliberează rezerva. Deconectarea proprietarului
eliberează de asemenea șabloanele, iar o limită de 120 de secunde recuperează
rezervările rămase după un teleport nereușit. La acceptare sunt reverificate
prezența jucătorilor și disponibilitatea rezervării.

Boții reali sunt creați la intrarea liderului în instanță. Dacă liderul se află
deja în dungeon, completarea se face direct, fără a depinde de un nou eveniment
de schimbare a hărții. Rolurile efectiv atribuite sunt păstrate pentru
completări/continuări ulterioare. Grupurile random și boții generați beneficiază
de `Luck of the Draw`. Boții nu sunt numărați drept jucători umani aleatorii pentru
realizările RDF legate de gruparea cu alți jucători.

Această implementare reutilizează șabloanele și echiparea generatorului existent.
Nu necesită o migrare SQL nouă și nu modifică datele din `world2` la instalare.

## Verificări offline

```sh
g++ -std=c++20 -O2 -Wall -Wextra -Werror contrib/tests/dungeon_finder_bots.cpp -o /tmp/lfg-bots-test
/tmp/lfg-bots-test
cmake --build build --target worldserver --parallel 4
```

Suita folosește funcțiile de producție pentru eligibilitatea temporală,
completarea rolurilor, alocarea șabloanelor și generarea contoarelor de GUID.
Compară 12.000 de distribuții de clase cu o căutare exhaustivă independentă și
verifică insuficiența șabloanelor, rolurile incompatibile și revenirea contorului
la capătul intervalului. Se poate rula și cu AddressSanitizer/UndefinedBehaviorSanitizer.

Aceste teste nu pornesc realm-ul și nu simulează clientul WoW sau teleportarea.

## Validarea în joc, după autorizarea pornirii

1. Un DPS solo în RDF: fără propunere cu boți înainte de termen; apoi un tank,
   un healer și doi DPS, după acceptarea jucătorului.
2. Tank/healer solo și grupuri de 2–4 participanți: compoziție finală 1/1/3.
3. Cinci oameni compatibili înainte de termen: matchmaking uman normal.
4. Două propuneri simultane: ferestre și răspunsuri independente, fără GUID-uri
   reutilizate și fără șabloane împrumutate de două ori.
5. Refuz, expirare, ieșire din coadă și deconectare: fără grupuri fantomă sau
   rezervări persistente; participanții care acceptaseră sunt readăugați normal.
6. Un membru pleacă dintr-o instanță activă: continuarea completează rolul lipsă,
   chiar dacă liderul nu mai schimbă harta.
7. Mod dezactivat, șabloane insuficiente și selecție explicită cu `RandomOnly = 1`:
   fără completare automată. Verificați păstrarea controalelor de nivel și lockout.
8. Verificați teleportarea, echiparea, rolurile, buff-ul, loot-ul și curățarea
   boților la terminarea/ieșirea din dungeon.

`worldserver` și `authserver` trebuie să rămână oprite până la solicitarea
explicită a utilizatorului.
