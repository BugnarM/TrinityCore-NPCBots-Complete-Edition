# Populare dinamică open world — 25 octombrie 2026

Modul adaugă wander bots în zonele continentale ocupate de jucători reali. Nu necesită alegerea manuală a hărților sau zonelor în configurație. Folosește rutele WanderNode existente pentru a păstra pozițiile și deplasarea controlate.

## Comportament

- Scanarea sesiunilor are loc implicit la 15 secunde. Instanțele, battlegroundurile, arenele, jucătorii în zbor și cei în teleportare nu declanșează popularea.
- Populația este identificată prin perechea hartă + zonă; harta 0 este validă.
- Ținta implicită este 8 boți pe zonă, maximum 200 în întregul modul. Dacă este nevoie, bugetul global se distribuie uniform între zonele eligibile.
- Maximum 8 încercări de spawn pe scanare, cu rotație între zone. Popularea este graduală și poate necesita mai multe scanări; pool-ul de template-uri este comun cu celelalte sisteme NPCBots.
- Nivelul se alege din intersecția intervalului punctului de spawn cu minimul clasei. Un Death Knight nu poate apărea într-un punct de nivel 1–10. Bonusul de nivel al rangurilor elite nu se aplică acestor boți; nivelul ales rămâne fix pe durata populației dinamice.
- Echiparea folosește generatorul existent pentru wanderers, potrivit clasei și nivelului.
- Facțiunea trebuie să fie permisă de punctul de spawn. Nu există fallback care să ignore restricțiile de facțiune.
- Sunt preferate punctele SPAWN; în lipsa lor pot fi folosite puncte obișnuite cu legături. Punctele NOT_A_START_POINT și cele fără legături sunt excluse.
- Destinațiile de patrulare, revenire și înviere rămân în zona de origine. Un bot ajuns fizic în afara acesteia, de exemplu în timpul luptei, este retras la scanarea următoare.
- După plecarea ultimului jucător, boții sunt trimiși spre despawn prin mecanismul nativ. Memoria și template-urile temporare sunt eliberate prin cleanup-ul existent; grilele urmează temporizatorul normal de descărcare al nucleului. Nu se pretinde descărcare instantanee.
- Reducerea țintei sau dezactivarea modulului retrage și boții deja generați. Resetarea retrage boții; zonele ocupate vor fi populate din nou la scanările următoare.

## Configurare

```ini
NpcBot.WanderingBots.DynamicZone.Enable = 1
NpcBot.WanderingBots.DynamicZone.BotsPerZone = 8
NpcBot.WanderingBots.DynamicZone.UpdateInterval = 15
NpcBot.WanderingBots.DynamicZone.MaxBots = 200
NpcBot.WanderingBots.DynamicZone.SpawnBudget = 8
```

Limitele acceptate sunt 0–100 boți/zonă, 1–300 secunde, 0–1000 global și 1–32 încercări/scanare. Valorile negative nu pot produce overflow.

Populația manuală rămâne separată:

```ini
# Doar populare dinamică; nu creează populația statică pe continente.
NpcBot.WanderingBots.Continents.Count = 0
# Această listă continuă să controleze populația manuală.
NpcBot.WanderingBots.Continents.Maps = 0,1,530,571
```

Modul dinamic activ la pornire încarcă metadatele rutelor de pe continente independent de lista manuală. Dacă era dezactivat la pornire și unele continente au fost excluse, activarea lor completă necesită repornirea serverului pentru încărcarea rutelor.

Comenzi administrative:

- `.npcbot dynamic status` — hartă, zonă, jucători, populație/țintă, interval de nivel.
- `.npcbot wander zone` — același raport.
- `.npcbot dynamic reset` — retrage populația curentă; modul continuă să funcționeze.

## Date și limite

Verificare read-only în `world2`: 816 noduri/26 zone pe harta 0, 1507/21 pe harta 1, 1361/8 pe 530, 1260/9 pe 571. Aceste numere descriu datele, nu garantează eligibilitatea fiecărui nod sau existența unui template liber.

Intervale observate în date: Elwynn 1–10, Westfall 8–20, Stranglethorn 33–45, Borean Tundra 68–73, Icecrown 77–80. Nivelul efectiv respectă punctul ales; nu nivelul jucătorului care vizitează zona. Zonele fără puncte eligibile nu sunt populate și nu se creează rute sau coordonate speculative. Faza standard a wanderers rămâne cea nativă.

## Verificare offline

Testul `contrib/tests/dynamic_zone_test.cpp` include politica folosită în producție (`botdynamiczonepolicy.h`). Vechea simulare duplicată a managerului a fost eliminată.

- 19.440 intersecții de nivel/clasă, verificând fiecare nivel selectabil.
- 144.600 configurații de buget, verificând distribuția uniformă și limitele locale/globale.
- Izolarea hartă/zonă, harta 0, compatibilitatea cu modul manual, configurări negative/extreme.
- Testul trece și cu AddressSanitizer/UndefinedBehaviorSanitizer.
- Toate cele șapte suite offline NPCBots au trecut.

Compilarea nucleului și instalarea binarului sunt consemnate în manifestul arhivei de instalare. Serverele nu au fost pornite pentru acest lucru. Spawn/despawn, deplasarea, vizibilitatea în faze și consumul de resurse necesită în continuare verificare în joc; testele de politică nu certifică aceste integrări runtime.

## Instalare verificată

Binar: `/home/trinity/server/bin/worldserver`. SHA256: `c97fba0e19fa124eb0b1e99421eea1ba05461f4cc69fa789a4a7730beacc6526`.

Backup, surse, loguri și manifest: `/home/trinity/source-archives/dynamic-zone-20261025-y3798w4a`. Compilarea finală nu a emis avertismente. Configurația activă are modul dinamic pornit și `Continents.Count = 0`; serverele de joc au rămas oprite.


## Spawn spacing and November route updates

The dynamic controller evaluates player proximity and checks that an anchor has at least one same-zone exit for the selected faction. The generated level must be usable on an exit. Candidate selection favors the areas with fewer nearby bots, then marked spawn points, then proximity to players. Road nodes remain eligible unless they carry the movement-only flag `0x80000`. The original global and zone limits still apply, with an additional cap around each spawn point.

```ini
NpcBot.WanderingBots.DynamicZone.SpawnSpacingRadius = 50.0
NpcBot.WanderingBots.DynamicZone.MaxNearbyBots = 8
```

Both options are read at each scan. The movement controller prefers destinations outside its four most recent visited nodes when the current node has alternatives. Flags that explicitly allow backtracking retain their existing behavior. The normal faction, level, zone and link-weight rules still apply.

The [route migration](../sql/custom/world/npcbot_2026_11_18_05_wanderer_route_refinement.sql) adds 23 waypoints, repairs open-world returns and adds ground routes through Storm Peaks, Icecrown, Sholazar Basin, Zul’Drak, Dragonblight, Netherstorm and Deadwind Pass. Ulduar and the Black Temple have local circuits for the tested exterior ground section. The Naxxramas approach stays on the ground below the citadel. The Tempest Keep flight approach is outside these ground routes. Existing battleground and Wintergrasp objective graphs retain their directed links.

The SQL graph is loaded when worldserver starts. Use the migration manifest for a fresh install. For an existing realm, check that the anchors and previous migrations are present and back up the database before applying the route migration.
