# Regresie Wintergrasp: infanterie blocată și ieșire prematură din luptă

## Dovezi

Bătălia din 2026-11-09 încheiată la 08:50:16 a durat 1.800.037 ms. Logul nativ înregistrează zero kill-uri, zero morți de boți, zero capturi și zero vehicule construite. S-au înregistrat 139/132 episoade de blocare, dar numai 6/28 recuperări. Acest rezultat confirmă raportul din client; testele anterioare pe hărți simulate nu au detectat regresia din datele reale.

Sonda separată cu bibliotecile native Detour/MMAP a găsit cinci coordonate Z aproximative la peste șapte metri de suprafața reală, între cele unsprezece puncte verificate. Exemple: `(4750,2840,380)` are suprafața navigabilă la aproximativ 361,24; `(5100,3100,385)` la 367,68. Toate cele șase coridoare de asalt/contraatac testate sunt conectate folosind filtrul nativ de teren și replanificarea prefixelor valide. O căutare lungă poate întoarce un prefix din cauza bugetului de noduri; aceasta nu înseamnă că destinația este izolată.

## Corecții

1. Punctele de destinație sunt proiectate pe suprafața navigabilă din mmaps atunci când se emite/reîncearcă ordinul, nu la fiecare tick. Pozițiile de spawn folosesc formația individuală și sunt proiectate înainte de crearea soldatului. Nu sunt teleportați boți deja existenți.
2. Infanteria acceptă un prefix real de navmesh marcat cu abaterea de înălțime a destinației (`FARFROMPOLY_END`). Pozițiile inițiale invalide, lipsa unui traseu și shortcut-urile rămân respinse. Blindatele păstrează verificarea strictă existentă.
3. Navmesh-ul validează terenul pentru infanterie; verificarea suplimentară de coliziune urmărește obiectele dinamice, inclusiv porțile. Infanteria nu mai este respinsă de razele statice de gabarit ale vehiculelor pe pante/poduri. Verificările de barieră și captură rămân active.
4. Originea de luptă este inițializată și pentru un bot angajat primul de un inamic. Anterior, dacă `AttackStart` propriu nu preceda intrarea în luptă, limita de urmărire putea fi evaluată față de `(0,0,0)`, declanșând evade. Originea rămâne fixă pe durata angajării, iar următoarea luptă capătă o origine nouă.
5. Logul periodic include acum `fighting`, `moving`, `corporals` și `lieutenants`, pentru a verifica progresia reală. Construirea rămâne condiționată de kill-uri apropiate, rang, inginer, atelier deținut și capacitate. Nu se acordă puncte sau ranguri gratuite.

Nu este schimbat filtrul global `PathGenerator` și nu este introdusă permisiune suplimentară pentru pante: o modificare experimentală a fost eliminată după verificarea coridoarelor reale.

## Verificare

Au trecut 17 suite, inclusiv cele două noi:

- `contrib/tests/wintergrasp_combat_origin.py`: execută metoda de producție pentru un apărător atacat înainte de propriul `AttackStart`, păstrarea limitei de 100 m și angajări ulterioare.
- `contrib/tests/wintergrasp_real_map_navigation.py`: folosește bibliotecile native și datele instalate, execută proiecția de producție și verifică unsprezece puncte și șase coridoare, inclusiv replanificarea prefixelor incomplete.

Testul navigatorului verifică suplimentar acceptarea marcajului de înălțime pentru infanterie, păstrarea stricteții vehiculelor și blocarea porților dinamice. Celelalte teste verifică în continuare rangurile, construcția, coordonarea, capturile, limitele și curățarea.

Loguri: `/tmp/wg-stall-20261109/tests.log`, `/tmp/wg-stall-20261109/map-probe.log`, `/tmp/wg-stall-20261109/build-release.log`.

După activarea binarului, verificarea runtime urmărește apariția boților în mișcare/luptă și apoi promovările. O verificare vizuală completă și rata victoriilor nu sunt înlocuite de aceste teste.
