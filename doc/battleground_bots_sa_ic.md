# NPCBots în Strand of the Ancients și Isle of Conquest

Implementare: 23 octombrie 2026, TrinityCore 3.3.5 cu NPCBots.

## Capacitate și participare

- Strand of the Ancients (SA, harta 607): țintă de **15 participanți pe echipă**.
- Isle of Conquest (IC, harta 628): țintă de **40 participanți pe echipă**.
- Jucătorii umani sunt incluși în această țintă; boții completează locurile prin sistemul existent de coadă NPCBots. Limitele șablonului battlegroundului rămân autoritare.
- Participanții autonomi sunt NPCBots reali, cu AI-ul existent de clasă, echipament, vindecare și rotații PvP. Controlerul nou se aplică wandering bots în SA/IC; companionii angajați nu primesc automat aceste ordine.

Configurația instalată are deja:

```ini
NpcBot.WanderingBots.Continents.Count = 0
NpcBot.WanderingBots.BG.Enable = 1
NpcBot.WanderingBots.BG.CapLevel = 1
NpcBot.WanderingBots.BG.TargetTeamPlayersCount.SA = 15
NpcBot.WanderingBots.BG.TargetTeamPlayersCount.IC = 40
```

## Trasee și ordine

Controlerul tactic citește starea reală a porților și obiectivelor o dată pe secundă. Folosește Dijkstra pe rețeaua SQL de waypointuri, apoi mișcare cu pathfinding nativ. Muchiile porților adverse intacte sunt închise. Nu există teleportare de recuperare sau deplasare directă de rezervă prin porți. Fiecare bot păstrează waypointul curent până îl atinge, iar mici decalaje laterale separă membrii formației.

Migrarea rezervă numai ID-urile **5200–5226** pe harta 607 și **5300–5322** pe harta 628: 27 noduri SA și 23 noduri IC. Nu șterge alte trasee personalizate. ID-urile sunt contractul dintre SQL și regulile din `botbgtactics.h`; schimbarea lor cere actualizarea ambelor și a testului.

### Strand of the Ancients

- Șoferii păstrează vehiculele sănătoase; vehiculele atacatoare asediază porți,
  iar infanteria capturează steaguri. Apărătorii țin fața porții și pot ocupa
  ambele tunuri native. Căutarea vehiculelor include ambele porturi și verifică
  accesibilitatea. Detalii în `sa_vehicle_roles_and_retention_2026_10_27.md`.
- Relicva vulnerabilă are prioritate față de steagurile necapturate. Capturatorii
  desemnați rămân pe jos și nu abandonează interacțiunile pentru vehicule sau
  bombe. Detalii și regresii în `sa_objective_capture_2026_10_27.md`.
- Porțile intacte sunt verificate fizic și pentru mișcarea din luptă, pentru
  ambele echipe și vehiculele boților. Apărătorii folosesc drumuri, coborâri
  verificate de pe zid și teleportere native; apărarea finală ține exteriorul
  camerei. Detalii în `sa_gate_collision_and_defense_2026_10_26.md`.
- În pregătirea ambelor runde, ambele echipe stau la spawn. Mișcarea tactică
  începe după pornirea rundei. Checkpointul comandat este păstrat până la sosire,
  pentru a evita întoarcerile repetate lângă debarcader/cimitir. Corecția din
  26 octombrie 2026 este documentată în `sa_movement_and_preparation_2026_10_26.md`.
- Atacatorii aleg două culoare și avansează în ordinea nativă: Green/Blue, Purple/Red, Yellow, Ancient, relicvă. O poartă laterală deschisă permite continuarea etapei corespunzătoare.
- O parte dintre infanteriști capturează cimitirele disponibile; ceilalți escortează vehiculele sau asaltează frontul.
- Apărătorii se repoziționează la frontul activ și pot ocupa tunurile native. AI-ul de clasă execută lupta și vindecarea.
- Atacatorii folosesc demolisherele existente și pot transporta bombe de la depozite, apoi planta încărcături native lângă poartă.
- Capturarea steagurilor și activarea relicvei trec prin vraja nativă de interacțiune, cu timp de cast și posibilitate de întrerupere. Serverul validează distanța, vizibilitatea, faza, echipa, obiectul și participarea botului.
- Schimbarea rundelor evacuează vehiculele, resuscitează boții, curăță controlul și comenzile tactice, inversează rolurile și aplică pregătirea. Pregătirea este eliminată la începutul fiecărei runde.
- Poarta și demolisherul distruse folosesc punctajul specific SA. A fost eliminată acordarea unui tip de punctaj de captură nesuportat de SA, care putea opri serverul.

### Isle of Conquest

- Teleporterele fortărețelor sunt folosite explicit de infanteria autonomă,
  în ambele sensuri și pentru ambele facțiuni. După transfer, waypointurile de
  pe porți închise nu mai blochează reatașarea la graf. Corecția și verificările
  sunt în `ioc_keep_teleporters_2026_10_26.md`.
- Boții primesc opt grupe logice de câte cinci după ID, repartizate inițial între Workshop, Docks, Hangar, Quarry și Refinery. Repartizarea efectivă depinde de ID-urile participanților disponibili.
- Bazele capturate păstrează santinele; majoritatea grupelor din Workshop/Docks escortează asediul. Grupul de reacție încearcă să recupereze obiective contestate.
- Steagurile folosesc același flux ca jucătorii: cast, banner contestat, cronometru nativ de 60 secunde, captură/apărare, mesaje, punctaj, vehicule și spirit guides.
- Numai cimitirele complet controlate, cu spirit guide activ, sunt eligibile pentru înviere. Quarry și Refinery nu sunt indexate ca cimitire.
- După breșa unei porți adverse, infanteria avansează spre comandant; membri desemnați încearcă întâi capturarea cimitirului din fortăreață. Unele grupe apără comandantul propriu când fortăreața este penetrată.
- Moartea boților consumă întăririle o singură dată, inclusiv pentru căderi. Omorurile din vehicule conduse de NPCBots sunt atribuite șoferului. Moartea comandantului folosește victoria nativă.
- Bonusurile Quarry/Refinery sunt sincronizate și pentru boți după schimbarea controlului sau înviere.
- Membri ai grupei Hangar pot folosi teleportarea spre transportul nativ și tunurile gunshipului. Catapultele pot lansa pasageri NPCBots cu evenimentul nativ de salt.

## Vehicule și explozibili

Boții ocupă vehiculele battlegroundului, inclusiv locuri de pasager sau turele disponibile. Nu creează vehicule înlocuitoare. Vindecătorii rămân de regulă pe jos. Un jucător prietenos aflat lângă vehicul are prioritate, iar botul poate elibera postul.

Armele sunt alese dintre vrăjile efectiv prezente pe șablonul vehiculului. Tragerea, loviturile de berbec, bombele și distrugerea porților folosesc efectele native, fără scădere artificială de viață. Tunurile prioritizează vehiculele ostile apropiate și estimează poziția țintelor în mișcare. Părăsirea vehiculului restabilește facțiunea sa și nu îl șterge. La asaltul final asupra relicvei/comandantului, echipajele terestre coboară.

## Fișiere principale

- `src/server/game/AI/NpcBots/bot_battlegrounds.cpp`: controler tactic, deplasare, vehicule, bombe, interacțiuni.
- `src/server/game/AI/NpcBots/botbgtactics.h`: reguli pure pentru porți, etape și repartizarea grupelor.
- `src/server/game/AI/NpcBots/bot_ai.{h,cpp}`: integrare în ciclul AI și intrare/ieșire din vehicule.
- `src/server/game/Battlegrounds/Zones/Battleground{SA,IC}.{h,cpp}`: stări native, capturi, runde, punctaj, cimitire și condiții de victorie.
- `src/server/game/Battlegrounds/Battleground.cpp`: eliminarea bonusurilor de echipă și de pe NPCBots.
- `src/server/game/Spells/SpellEffects.cpp`: finalizarea castului de interacțiune NPCBot.
- `src/server/game/Entities/Unit/Unit.cpp`: atribuirea omorurilor din vehicule.
- `src/server/scripts/Northrend/IsleOfConquest/isle_of_conquest.cpp`: transport și lansarea pasagerilor NPCBots.
- `sql/custom/world/2026_10_23_01_battleground_bots_sa_ic.sql`: migrarea idempotentă a celor 50 de noduri.
- `contrib/tests/siege_battlegrounds.cpp`: verificarea regulilor împreună cu graful SQL de producție.

## Verificare și instalare

Migrarea a fost aplicată în **world și world2**, într-o tranzacție comună. În ambele baze au fost verificate 27 noduri SA și 23 IC; numărul rândurilor din afara intervalelor administrate a rămas neschimbat. Backupurile anterioare sunt în `/root/sa-ic-backups/20261023T203025Z`.

Verificări automate:

1. Compilarea completă a `worldserver`, inclusiv noul fișier C++, și recompilarea după ultimele ajustări.
2. Testul C++ citește SQL-ul de producție și verifică toate cele 64 combinații ale porților SA și IC, accesul la relicvă/comandanți, ieșirea din fortăreața proprie, legăturile reciproce și acoperirea celor cinci baze.
3. Migrarea a fost executată de două ori într-o bază MySQL izolată, cu rânduri-santinelă în afara intervalelor. Rândurile au fost păstrate.
4. `git diff --check`; existența fișierelor de hartă și mmaps pentru 607/628.

Reproducerea verificărilor din rădăcina repositoryului:

```bash
c++ -std=c++20 -O2 -Wall -Wextra -pedantic contrib/tests/siege_battlegrounds.cpp -o /tmp/siege-bg-test
/tmp/siege-bg-test
cmake -S . -B build
cmake --build build --target worldserver -j4
git diff --check
```

Executabilul rezultat, `build/bin/RelWithDebInfo/bin/worldserver`, a fost instalat în `/home/trinity/server/bin/worldserver`, după salvarea executabilului anterior. SHA-256 al sursei și destinației coincide: `78a9b70c5032c9b0e08321a8c4d24335d2b271353865ee6ccc79fff4e0402de4`. Starea finală și hashurile sunt consemnate în `installation.json` din directorul de backup. **worldserver și authserver sunt oprite** și trebuie păstrate astfel până la instrucțiunea utilizatorului.

## Verificare necesară din client

Compilarea și testele grafului nu demonstrează deplasarea vizuală, geometria fiecărui traseu sau performanța unei partide complete. Nu a fost pornit serverul pentru un playtest.

După pornirea autorizată, se intră în coada SA/IC prin battlemaster. Comanda GM `.debug bg` comută modul de testare a cerinței minime de jucători; nu creează singură o partidă. `.bf start` aparține sistemului battlefield și nu este comanda de pornire SA/IC.

- SA: verificați cele două runde și inversarea rolurilor, dispariția pregătirii, fiecare breșă, capturile cimitirelor, demolisherele, tunurile, încărcăturile și victoria la relicvă.
- IC: verificați captură/apărare contestată și cronometrul de 60 secunde, spawnul vehiculelor, bonusurile resurselor, învierea, breșa fortăreței, cimitirul interior, lupta cu comandantul și victoria prin întăriri.
- Ambele: verificați partide mixte cu jucători, eliberarea vehiculelor pentru oameni, întreruperea casturilor, atribuirea omorurilor din vehicule și lipsa blocajelor la waypointuri.
- Hangar/catapulte: verificați poziționarea pe transport, mișcarea tunurilor și saltul pasagerilor înainte de a considera aceste rute validate vizual.

Limite explicite: atacatorii NPCBots SA sunt pregătiți pe plajă; nu simulează călătoria cinematică în bărci, care rămâne nativă pentru jucători. Nu există dezamorsare autonomă a bombelor. Lansarea catapultelor urmărește frontul exterior; infiltrarea pe acoperișuri/parapete și debarcarea tactică cu parașuta nu sunt validate. Încărcarea CPU la efective maxime trebuie măsurată într-o partidă reală.
