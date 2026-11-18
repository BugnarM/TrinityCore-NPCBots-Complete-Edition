# Wintergrasp: navigația vehiculelor și recuperarea din blocaje

## Problemă și implementare

Mișcarea generică prin `MovePoint` poate ajunge la fallback-ul cu două puncte din
`MoveSplineInit::MoveTo` atunci când calculul drumului eșuează. PathGenerator
poate produce și un shortcut atunci când lipsesc tile-uri. Aceste căi nu oferă
garanția unui drum traversabil în jurul clădirilor, stâncilor sau copacilor.

`BotWarVehicleNavigation` gestionează acum deplasările vehiculelor autonome ale
modulului BotWar, inclusiv apropierea de ținta de asediu:

- Calculează drumul cu `forceDestination = false` și acceptă numai path type
  NORMAL (1) sau INCOMPLETE (4), cu cel puțin două puncte. Respinge shortcut-uri,
  tile-uri lipsă, capete departe de mesh și drumuri care depășesc limita nativă.
- Verifică fiecare segment prin LOS VMAP și gameobjects dinamice, în faza
  vehiculului. Verificarea include centrul și două raze laterale, cu raza de
  gabarit limitată la 0,75–2,5 metri și o înălțime de verificare de 0,75–2 metri.
- Lansează numai punctele verificate prin `MoveSplineInit::MovebyPath`. Această
  cale nu folosește fallback-ul direct al mișcării generice.
- Pentru drumuri lungi sau blocaje, încearcă puncte la 64/24 metri, în față,
  lateral și în spate. Cel mult patru variante sunt evaluate la o reîncercare,
  cu interval de trei secunde. Pozițiile recente sunt evitate pentru a limita
  pendularea. Nu există teleportare de recuperare.
- Verifică progresul și segmentele următoare o dată pe secundă, pe o distanță
  echivalentă cu cel puțin două secunde de mers plus raza vehiculului. După patru secunde
  fără progres încearcă o direcție diferită, chiar dacă navmesh-ul considera
  vechiul drum valid. O coliziune nouă oprește spline-ul și declanșează recalculare.
- Păstrează colțul efectiv următor al spline-ului. Nu validează o diagonală peste
  un viraj încă neatins, evitând opriri/replanificări artificiale lângă colțuri.
- Actualizarea unui obiectiv înlocuiește imediat drumul vechi. Un prefix terminat
  nu înseamnă waypoint tactic atins: traseul avansează numai la apropierea reală
  de destinație, la cel mult 12 metri.

Vehiculele controlate de oameni rămân în fluxul nativ. Oprirea la tragere,
predarea către un jucător, lipsa șoferului și abandonarea vehiculului anulează
recuperarea autonomă. ROOT și vrăjile care opresc deplasarea sunt respectate.

Nu sunt necesare SQL, configurație sau addon nou. Configurația instalată avea
deja `vmap.enableLOS = 1` și `CheckGameObjectLoS = 1`.

## Verificări și limite

```sh
python3 contrib/tests/wintergrasp_vehicle_navigation.py
python3 contrib/tests/wintergrasp_tower_defense.py
python3 contrib/tests/wintergrasp_map_lifecycle.py
python3 contrib/tests/wintergrasp_movement_speed.py
```

Toate trec. Primul test compilează clasa reală din sursă cu mișcare, navmesh și
coliziune simulate. Verifică ocolirea unui obstacol, retragerea dintr-un buzunar
în U, apariția unui obstacol pe un spline existent inclusiv pe segmente dense,
anticiparea coliziunii înainte ca vehiculul să depășească segmentul verificat,
viraje fără replanificări
inutile, schimbarea obiectivului, blocaj fizic fără progres, obstacole complet
închise urmate de eliberare, lipsa navmesh-ului, drumuri în prefixe, ROOT și
gabaritul lateral. Testul de turnuri verifică și sosirea reală la waypoint.

Compilarea completă worldserver a reușit fără avertismente. Log:
`/tmp/wg-vehicle-navigation-deploy-build.log`.

Validarea în client la obiectele observate de utilizator rămâne necesară.
Verificările folosesc datele de coliziune disponibile; nu sunt o simulare
poligonală a întregului model al vehiculului. Un obiect fără model de coliziune
în datele extrase nu poate fi detectat prin aceste raze. Dacă nu există nicio
ieșire traversabilă, vehiculul stă și reîncearcă după eliberarea drumului;
nu traversează deliberat obstacolul pentru a evita așteptarea.
