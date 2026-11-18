# Strand of the Ancients: capturarea steagurilor și relicvei

Corecție din 27 octombrie 2026, pentru atacatorii NPCBots autonomi.

Ordinele tactice concurau cu interacțiunile: un bot trimis la steag putea
alege un demolisher înainte să înceapă capturarea; selecția steagurilor putea
înlocui relicva vulnerabilă; verificarea combatului împiedica încercarea
castului chiar lângă obiectiv. Transportul bombelor putea devia același bot.

## Comportamentul nou

- Relicva disponibilă are prioritate față de orice cimitir necapturat.
  Verificările folosesc starea nativă a porților și obiectul activ.
- Un membru din fiecare grup logic de cinci (`GUID % 5 == 4`) rămâne pe jos
  pentru capturi. Selectează cel mai apropiat steag disponibil și necapturat.
  Alți infanteriști pot captura un steag aflat la cel mult 20 m.
- Boții desemnați nu abandonează obiectivul pentru vehicule sau bombe.
  Vehiculele atacatoare primesc numai obiective de asediu la porți. La relicvă,
  toate echipajele terestre primesc ordinul de a coborî și continuă pe jos.
- Lângă obiectiv, SA încearcă vraja nativă `Open Flag` (21651) și în combat.
  Oprește atacul automat, demontează și elimină forma animală înainte de cast.
  Combatul și aggro-ul nu sunt șterse. Castul păstrează timpul și posibilitatea
  nativă de întrerupere. Numai un cast acceptat blochează AI-ul ca interacțiune
  în curs; eșecurile nu sunt tratate ca o captură începută.
- La finalul castului, `Spell::EffectOpenLock` apelează validatorul SA existent.
  Acesta verifică runda, echipa, participarea, obiectul exact, distanța,
  vizibilitatea, faza și disponibilitatea. Captura nu se produce anticipat.

`CaptureGraveyard` continuă să aplice efectele native: schimbarea cimitirului,
spirit guide, steag și demolisherele atelierului lateral. Boții obișnuiți pot
alege aceste vehicule prin selecția existentă a vehiculului apropiat. Nu se
creează vehicule suplimentare, nu se forțează victoria și nu se schimbă
durata capturilor sau a rundelor. Nu este necesară o migrare SQL.

## Verificări

`python3 contrib/tests/sa_objectives.py` compilează codul real de selecție,
ordinea interacțiunii, eligibilitatea pentru vehicule, finalizarea vrajei și
handlerul SA, cu obiecte simulate. Acoperă toate cele 64 stări ale porților,
ambele echipe, toate cele trei cimitire, relicva prioritară, combatul, vehiculele,
casturile eșuate și refuzul capturilor invalide. Verifică și că un apel de
începere a castului nu capturează imediat obiectivul.

Regresiile existente pentru porți/apărare, pregătire și trasee SA, respectiv
teleporterele IC, au trecut. Ținta `worldserver` este recompilată și linkată;
`git diff --check` nu raportează probleme.

Confirmarea vizuală a unei partide complete rămâne o verificare în client.
