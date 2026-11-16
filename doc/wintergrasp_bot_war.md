# Wintergrasp Bot War — implementare și verificare

TrinityCore 3.3.5a + NPCBots.

Corecții curente (2026-11-08): [AI vehicule, captură și ofensivă](wintergrasp_aggression_2026_11_08.md), apoi [ateliere, ranguri și coordonare](wintergrasp_realism_2026_11_08.md), apoi [recuperare, opt profiluri, PvP, asediu și statistici](wintergrasp_upgrades_2026_11_08.md). Raportul nou descrie starea acestei intervenții; verificările istorice de mai jos nu sunt validare în client pentru versiunea curentă.


## Starea verificării

- Compilare completă `worldserver`: reușită, inclusiv linkarea binarului final. Log: `/tmp/wg-worldserver-build-final.log`.
- Test autonom de populație și repartizare: reușit.
- Migrare SQL de la prototipul vechi și aplicare repetată în MySQL temporar: reușită. Rezultate: `/tmp/wg-migration-verified.tsv` și `/tmp/wg-equipment-verified.tsv`.
- Binar candidat: `/root/TrinityCore-3.3.5-with-NPCBots/build/bin/RelWithDebInfo/bin/worldserver`.
- Activarea pe server și verificarea în client sunt încă de efectuat. Configurația instalată are în continuare `Wintergrasp.PlayerMax = 100`; noul modul necesită SQL-ul și opțiunile de mai jos.

## Capacitatea bătăliei

Armatele se activează numai în timpul unei bătălii active și după ce cel puțin
un jucător real a acceptat intrarea în luptă. Înscrierea în coada pentru următoarea
bătălie nu este suficientă. Intrarea după începutul bătăliei activează armatele
în același mod; dacă ultimul participant real pleacă, boții sunt retrași. La o nouă
intrare, popularea începe după aproximativ patru secunde, cu limita de creare de
mai jos. Calendarul nativ al bătăliilor continuă să ruleze.

Limita WotLK este **120 de participanți pe facțiune, 240 în total**. Referință: [notele oficiale Blizzard pentru patch 3.2.0, arhivate de Blue Tracker](https://www.bluetracker.gg/wow/topic/eu-en/10222156565-world-of-warcraft-patch-notes-320/).

În implementare, boții completează locurile libere:

```text
boți = max(0, min(TargetPerTeam, PlayerMax, 120) − jucători înscriși în luptă)
```

Exemplu: 17 jucători Alliance înseamnă cel mult 103 boți Alliance. Intrarea unui jucător eliberează un loc de bot la următoarea actualizare, în maximum aproximativ o secundă. Jucătorii morți rămân participanți; boții morți își reocupă locul după cel puțin 30 de secunde. Vehiculele au plafon separat, cel oferit de ateliere; șoferii și tunarii sunt membri ai celor 120, nu soldați suplimentari.

`PlayerMax` este limitat în BattlefieldWG la intervalul 1–120. Dacă configurația existentă conține 100, limita efectivă rămâne 100 până la actualizarea configurației și repornirea serverului.

## Ce există în cod

- Plafon de populație și roster stabil, cu prioritate pentru jucătorii reali.
- Grupe de cinci: doi melee, un healer și doi ranged, distribuiți între opt profiluri. Rolul de luptă este ales după entry; `unit_class` reprezintă clasa statistică NPC, nu specializarea unui jucător.
- Ordine individuale: culoar, număr de grupă, loc în formație și vehicul de escortat.
- Trei culoare de asalt, garnizoane pentru fiecare turn sudic, trei contraatacuri
  spre turnuri, trei poziții de apărare a cetății și două obiective de atelier.
- Garnizoane active la atelierele nordice (Broken Temple și Sunken Ring): apărătorii
  de pe flancuri contestă steagurile la începutul bătăliei, iar la pierderea atelierului
  efectuează o retragere tactică spre porțile exterioare ale cetății.
- Puncte de start sudice pentru atacatori: aripile Vest și Est pornesc din taberele
  sudice Westpark și Eastspark, înaintează călare peste poduri și asaltează atelierele
  nordice; doar după capturare acestea devin baze avansate de spawn și blindate.
- Vehicule defensive în curtea cetății: apărătorii construiesc catapulte (Plague Barrel
  anti-infanterie) și demolishere defensive din atelierele interioare ale cetății.
- Prioritizare a blindatelor de asediu: soldații și vehiculele de apărare își concentrează
  daunele pe tancurile care atacă porțile. Fire Cannon (57609) este folosit de turela vehiculului;
  soldații nu folosesc arma tunului ca explozibil portabil.
- Last Stand la Titan Relic: la distrugerea Porții Seifului (191810), toți apărătorii
  din cetate se retrag în sala relicvei (5440.38, 2840.49, 430.28) și formează un perimetru;
  relicva nu poate fi revendicată dacă botul atacator se află în luptă.
- Garnizoanele atacatorilor păstrează turnurile și nu pleacă după vehicule libere.
  După distrugerea unui turn, supraviețuitorii și înlocuitorii lor întăresc asaltul
  central, vestic sau estic corespunzător. Flancurile de asalt capturează în
  continuare atelierele nordice înainte să avanseze.
- Capturarea atelierelor prin bara nativă, împreună cu jucătorii și sub aceleași limite de viteză. Boții îmbarcați nu capturează ateliere.
- Atacatorii vest/est securizează atelierul înainte să avanseze. Garnizoana centrală se retrage spre poarta seifului după distrugerea zidului interior. Grupele care au distrus turnul sudic se îndreaptă apoi spre un atelier nordic.
- Checkpoint-uri înaintea porților și zidurilor; avansarea se blochează până la distrugerea obstacolului. O clădire care nu este încă disponibilă în hartă nu este considerată distrusă.
- Pathfinding la deplasare, formații cu poziții stabile, alergare de bază de 7 m/s,
  mount terestru de 14 m/s pentru drumuri lungi și descălecare la apropierea
  obiectivului sau la angajare. Multiplicatorii sunt setați prin `SetSpeedRate`,
  iar deplasarea folosește explicit modul de alergare.
- Alegerea țintelor, verificări de vizibilitate, distanță de luptă pentru ranged, heal și în afara luptei, abilități defensive și trinket PvP cu cooldown de două minute. Urmărirea unui adversar este limitată la 100 m de locul angajării.
- Vehicule cu șofer bot; Siege Engine poate primi tunar bot în turela accesoriu existentă în DB. La apropierea unui jucător aliat la 8 m, botul eliberează scaunul pentru preluare. Îmbarcarea și predarea controlului trebuie validate cu clientul.
- Vehiculele autonome folosesc spline-uri calculate și verificate cu navmesh,
  VMAP și coliziunea obiectelor dinamice. Sunt verificate centrul și marginile
  laterale. La un blocaj încearcă prefixe scurte, ocoliri laterale și retragere,
  cu verificarea progresului și reîncercări limitate. Nu folosesc fallback direct
  când un traseu este invalid. Vezi `wintergrasp_vehicle_navigation_2026_10_26.md`.
- Construirea/refacerea vehiculelor la un inginer dintr-un atelier deținut, după deplasarea constructorului și cinci secunde de construcție. Se verifică rangul, prezența, controlul local și capacitatea nativă BattlefieldWG. Rămâne un loc liber dacă facțiunea are participanți umani. Căutările nereușite sunt reîncercate la zece secunde.
- Daune de asediu prin spell-urile native; fără scăderi arbitrare de HP. Catapulta are comportament antiinfanterie, fără daune artificiale asupra porților.
- Capturarea relicvei doar pentru atacatori, la cel mult 5 m, cu vizibilitate, după deblocarea nativă a relicvei și trei secunde fără luptă.
- Curățare la încheiere, dezactivare și schimbarea bătăliei. Un vehicul ocupat de un om este păstrat când se eliberează locul soldatului, inclusiv dacă omul este tunar în turela secundară.
- Populare treptată: prima actualizare după 4 secunde, cel mult 10 soldați/facțiune/secundă. Summon-urile și deplasarea sunt gestionate din actualizarea hărții Northrend.
- Mesaje de diagnostic `Wintergrasp BotWar` în categoria de log `bg.battlefield`: prima raportare după aproximativ 15 secunde, apoi la fiecare 30 de secunde; includ oameni, boți, buget și vehicule.

Pentru o facțiune completată cu 120 de boți, distribuția inițială este cea de mai
jos. Este necesar cel puțin un participant real în cealaltă facțiune; două armate
de boți nu mai pornesc singure o luptă fără participanți reali.

| Facțiunea tactică | Obiectiv | Soldați |
|---|---|---:|
| Atacatori | Asalt central | 30 |
| Atacatori | Asalt vestic | 30 |
| Atacatori | Asalt estic | 30 |
| Atacatori | Apărare turn sudic central | 10 |
| Atacatori | Apărare turn sudic vestic | 10 |
| Atacatori | Apărare turn sudic estic | 10 |
| Apărători | Cetate: centru / vest / est | 45 / 15 / 15 |
| Apărători | Turn sudic central / vestic / estic | 15 / 15 / 15 |

Grupele păstrează ordine stabile când populația se schimbă. Cu jucători reali sau pierderi în luptă, numărul activ pe fiecare obiectiv poate fi mai mic decât în tabel.

## Fișiere

Repository: `/root/TrinityCore-3.3.5-with-NPCBots`.

| Fișier în repository | Responsabilitate |
|---|---|
| `src/server/scripts/Northrend/zone_wintergrasp.cpp` | Roster, trasee, soldați, șoferi, tunari, tunuri și relicvă |
| `src/server/scripts/Northrend/wintergrasp_bot_war.h` | Bugetul de populație și repartizarea grupelor |
| `src/server/scripts/Battlefield/BattlefieldWG.cpp` / `.h` | Capturarea atelierelor, identificarea clădirilor, numărarea vehiculelor și limita de participanți |
| `src/server/game/Battlefield/Battlefield.cpp` / `.h` | Extensie pentru participanți scriptati la capturare și acces la numărul de jucători |
| `src/server/worldserver/worldserver.conf.dist` | Opțiunile BotWar și limita implicită de 120 |
| `sql/custom/world/2026_10_23_00_wintergrasp_bot_war.sql` | Template-uri 80501–80508, echipament, mișcare și legarea scripturilor |
| `contrib/tests/wintergrasp_bot_war.cpp` | Test autonom pentru plafon și repartizare |

Aceste armate folosesc **ScriptedAI**, separat de NPCBots personali angajați cu `.npcbot add`. SQL-ul nu scrie în `creature_template_npcbot_wander_nodes`; deplasarea este stabilită de obiectivele bătăliei în C++.

## Traseele și obstacolele

Pozițiile clădirilor sunt cele din `WGGameObjectBuilding` în BattlefieldWG.cpp. Pozițiile atelierelor sunt preluate din steagurile încărcate pe hartă; fallback-urile provin din spawn-urile SQL existente. Checkpoint-urile intermediare rămân de validat în client, pe mmaps/vmaps instalate.

| Culoar | Succesiunea obiectivelor |
|---|---|
| Centru | Staging sud → pod → poarta 190375 → zidul interior 191805 → poarta 191810 → relicva 192829 |
| Vest | Broken Temple → pod vestic → zid 190371 → zid interior 191805 → poarta 191810 → relicvă |
| Est | Sunken Ring → rampă/pod estic → zid 190372 → zid interior 191805 → poarta 191810 → relicvă |
| Contraatac centru | Exterior cetate → pod sud → turn 190357 → atelier estic |
| Contraatac vest | Exterior vestic → Broken Temple → turn 190356 → atelier vestic |
| Contraatac est | Exterior estic → Sunken Ring → turn 190358 → atelier estic |

Contraatacurile apar în exteriorul cetății pentru a nu necesita traversarea unei porți încă intacte. Relocarea prin teleport a unui bot blocat nu este implementată; deplasarea este reîncercată cu pathfinding.

## Corecții față de documentul/prototipul inițial

Verificate în `/home/trinity/server/data/dbc/Spell.dbc` și în SQL-urile Wintergrasp din repository:

| Utilizare | ID corect |
|---|---:|
| Mortal Strike nivel 80 | 47486; 16857 este Faerie Fire (Feral) |
| Demolisher Hurl Boulder | 50896, cu proiectil 50999; 54107 este Ram |
| Siege Engine Ram | 54109 |
| Siege Turret Fire Cannon | 57609; 54118 este Improved Soul Leech |
| Tower Cannon Fire Cannon | 51421, cu destinație pe sol |
| Catapult Plague Barrel | 57606 |
| Catapult Flame Breath | 50989 |
| Frostbolt / Flash Heal nivel 80 | 42842 / 48071 |

Comanda GM reală din acest repository este **`.bf start 1`**, nu `.bf wg start`.

## Instalare și configurare

Noul modul este implicit dezactivat în configurația distribuită. Fișierele serverului instalat și baza live nu sunt actualizate automat de compilare. Scriptul vechi `/root/wintergrasp_setup.sql` nu trebuie rulat după migrarea nouă, deoarece ar reintroduce setările vechi.

1. Compilează binarul candidat:

   ```bash
   cd /root/TrinityCore-3.3.5-with-NPCBots
   cmake --build build --target worldserver -j2
   ```

   Rezultat: `build/bin/RelWithDebInfo/bin/worldserver`.

2. Aplică `sql/custom/world/2026_10_23_00_wintergrasp_bot_war.sql` în baza **world**. Migrarea este opțională, poate fi repetată și actualizează și template-urile prototipului vechi. Nu folosi o bază auth/characters.

3. Pentru profilul 120v120, configurează serverul instalat:

   ```ini
   Wintergrasp.Enable = 1
   Wintergrasp.PlayerMax = 120
   Wintergrasp.PlayerMin = 0
   Wintergrasp.PlayerMinLvl = 77
   Wintergrasp.BattleTimer = 30
   Wintergrasp.NoBattleTimer = 150
   Wintergrasp.BotWar.Enable = 1
   Wintergrasp.BotWar.TargetPerTeam = 120
   ```

   Pentru un test inițial mai mic, folosește `TargetPerTeam = 40`. Setarea existentă `PlayerMinLvl = 1` este permisivă pentru testare, dar nu corespunde accesului WotLK obișnuit.

4. Instalează binarul și repornește serverul în fereastra de mentenanță. Nu presupune că simplul `.reload config` reconstruiește limita de participanți din BattlefieldWG.

## Verificare automată

```bash
cd /root/TrinityCore-3.3.5-with-NPCBots
c++ -std=c++20 -O2 -Wall -Wextra -pedantic contrib/tests/wintergrasp_bot_war.cpp -o /tmp/wg-test
/tmp/wg-test
```

Testul verifică peste trei milioane de combinații de țintă/capacitate/oameni, prioritatea oamenilor, cazurile fără locuri libere, păstrarea grupelor și acoperirea celor trei turnuri.

SQL-ul a fost verificat în MySQL 8.0.46, într-un datadir temporar separat: schema repository-ului, template-uri sursă minimale, scriptul prototipului vechi, apoi migrarea nouă aplicată de două ori. Au rezultat opt template-uri de soldat, opt seturi de echipament și legăturile de script așteptate. Aceasta validează migrarea și repetabilitatea ei, nu conținutul complet al bazei live.

Compilarea și testele automate nu reprezintă validare vizuală a traseelor sau măsurare de performanță la 240 de participanți.

## Playbook GM — de executat în joc

1. Intră în Wintergrasp, apoi pornește bătălia:

   ```text
   .go xyz 5364 2835 409 571
   .bf start 1
   ```

2. Acceptă invitația la luptă. Verifică logurile `Wintergrasp BotWar` după 15–30 de secunde. La o facțiune cu un jucător înscris și țintă 120 trebuie să existe maximum 119 boți. Intră/ieși cu încă un caracter și verifică eliberarea/completarea locului.

3. Observă steagurile atelierelor nordice. Forțele vest/est trebuie să aștepte capturarea înainte să avanseze; capacitatea vehiculelor se schimbă prin worldstate-urile native. Omoară/îndepărtează boții de lângă un steag și verifică oprirea contribuției lor la capturare.

4. Urmărește fiecare culoar pe poduri și rampe. Verifică poziția Z, urcările, formația și absența trecerilor prin obiecte solide. La obstacole, soldații așteaptă, iar vehiculele folosesc spell-urile de asediu. Notează coordonata exactă a oricărui blocaj pentru ajustarea checkpoint-urilor.

5. Verifică succesiunea porților, inclusiv zidul interior 191805 pe flancuri. Viața clădirilor trebuie modificată de spell-urile native, iar distrugerea să genereze evenimentele și worldstate-urile obișnuite.

6. Verifică șoferul și, pentru Siege Engine, tunarul. Apropie-te ca aliat, ocupă volanul, condu, coboară și verifică reluarea ordinelor. Repetă cu turela; confirmă că nu dispar vehicule ocupate atunci când un jucător nou înlocuiește un bot din roster.

7. Urmărește toate cele trei turnuri sudice: 190356, 190357, 190358. Verifică reducerea nativă a timpului după distrugerea celor trei și deplasarea grupelor spre ateliere după terminarea obiectivului.

8. Omoară soldați și vehicule. Confirmă refacerea după minimum 30 de secunde, respectarea plafonului și eliberarea sloturilor de vehicul la moarte/despawn.

9. După distrugerea porții 191810, verifică deblocarea relicvei. Angajează botul aflat la relicvă pentru a-i întrerupe capturarea; apoi verifică victoria, schimbarea apărătorului și accesul nativ la Vault of Archavon.

10. Oprește și repetă bătălia cu facțiunile inversate:

    ```text
    .bf stop 1
    .bf start 1
    ```

    Verifică absența armatelor vechi, inclusiv la stop/start rapid. Repetă la 40, apoi 120 participanți pe parte și urmărește latența/update time din loguri înainte de a considera profilul mare validat.

## Ce mai trebuie pentru fidelitate completă

Versiunea actuală include recuperare, coordonare și opt profiluri; nu reproduce întregul comportament al unor jucători veterani. Vezi raportul de upgrade pentru implementarea și limitele curente. Rămân de implementat/validat: celelalte clase și specializări, rotații PvP complete, tactici de flancare mai avansate decât redistribuirea după presiunea atelierelor, folosirea acoperirii/strafe, conversații contextuale și recompense de onoare echivalente jucătorilor. Numele interne atribuite prin `SetName` trebuie verificate în client; cache-ul de template poate păstra numele generic.

Nu sunt încă validate în client traseele, accesoriile/turelele din baza instalată, preluarea controlului sau performanța 120v120. Pentru a numi experiența apropiată de jocul real trebuie încheiat playbook-ul de mai sus pe ambele facțiuni.
