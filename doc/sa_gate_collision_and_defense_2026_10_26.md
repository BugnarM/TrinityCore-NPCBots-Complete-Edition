# Strand of the Ancients: porți solide și apărarea camerei

Corecție din 26 octombrie 2026 pentru NPCBots autonomi și vehiculele conduse de ei.

## Porți

`BattlegroundSA::IsBotGateSegmentClear` verifică segmentele de mișcare față de
modelele WMO reale ale celor șase porți care nu sunt distruse. Verifică centrul
și laturile moverului, faza obiectelor și starea actuală a porților. Dacă lipsește
un model, o verificare conservatoare locală blochează scurtătura din dreptul porții.

Verificarea se execută în `MoveSplineInit::Launch`, după stabilirea poziției
reale de pornire, pentru mișcare tactică, urmărire în luptă, `MovebyPath`, deplasări
directe și vehicule conduse de boți. O comandă invalidă oprește spline-ul anterior.
Săriturile native sunt verificate pe arc, în 16 segmente; spline-urile curbe nu
pot rotunji un colț verificat. Infanteria și apărătorii nu au excepții de facțiune.

Pathfindingul SA refuză acum scurtăturile generate la lipsa datelor sau la
eșecul căutării, la fel ca protecția existentă pentru IC. O poartă distrusă
redeschide trecerea fără alte comenzi. Controalele jucătorilor rămân native.

## Apărători

Apărătorii încearcă mai întâi o cale fizic validă spre front. Când trebuie să
treacă de pe zid spre teren, pot merge pe platforma unui tun nativ și efectua
o coborâre scurtă, cu mecanismul normal de săritură. Coborârea cere:

- o cale validă până la platformă și apropierea efectivă de ea;
- teren de sosire la 3–20 m mai jos, la aproximativ 12 m lateral;
- arc liber față de porți și geometria statică/dinamică;
- un drum verificat de la sosire până la obiectiv, inclusiv pe mai multe
  prefixe atunci când corridorul Detour depășește limita nativă.

Nu există sărituri de pe sol peste porți. Dacă este nevoie de transfer pe zid,
AI-ul ajunge la unul dintre cele cinci obiecte native 192819 și folosește cel
mai apropiat world trigger 23472 al acelei platforme. Interacțiunea cere distanța
nativă, fază corectă, luptă activă și apărător viu, fără vehicul, transport,
combat, cast sau control. Transferul aplică lockoutul nativ 54643. Nu se fac
teleportări de recuperare la distanță sau prin porți fără platformă.

Infanteria apărătoare ține exteriorul fiecărei porți active; gărzi desemnate pot
ocupa ambele tunuri native ale porții. După ultima breșă,
numai atacatorii primesc relicva ca obiectiv; apărătorii păstrează intrarea
camerei. Nu sunt schimbate imunități, viață, damage sau timerele rundelor.

`PathGenerator::CalculatePathFrom` permite verificarea unui drum de la o
viitoare sosire fără deplasarea temporară a botului. Apelul existent
`CalculatePath` continuă să folosească poziția curentă a moverului.

## Verificări

- `python3 contrib/tests/sa_gate_movement.py`: execută funcțiile reale de
  planificare, transfer, coborâre și verificare la lansarea spline-ului cu
  obiecte simulate. Include chase/direct/vehicul, lockout, roluri, distanțe,
  obstacole și toate cele 64 stări ale porților pentru obiectivul final.
- `python3 contrib/tests/sa_gate_collision_data.py`: compilează funcția reală
  de coliziune cu biblioteca `common` și modelele WMO instalate; verifică
  768 traversări pentru cele 64 stări, modele lipsă/dezactivate și faze.
  Folosește mmaps și vmaps reale pentru drumurile către șapte fronturi de
  apărare, inclusiv platformele native, sosirile și coborârile de pe zid.
- Regresiile pentru pregătire/debarcader SA, teleporterele IC și regulile
  grafului SA/IC rămân în suitele existente.
- Compilarea și linkarea țintei `worldserver`, plus `git diff --check`.

Nu sunt necesare SQL sau configurații noi. Testele geometrice și cele cu
obiecte simulate nu înlocuiesc confirmarea vizuală într-o partidă completă.
