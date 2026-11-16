# Strand of the Ancients: debarcader, apărători și pregătire

## Cauze și corecții

Controlerul alegea următorul checkpoint, dar îl păstra ca punct de plecare numai
dacă botul se afla deja la mai puțin de 18 metri de el. Pe o legătură lungă,
botul era din nou atașat nodului inițial. După ce se depărta cu peste 18 metri de
acel nod, primea ordin să se întoarcă. Ciclul bloca atacatorii lângă debarcader și
apărătorii pe drumurile de la spawn/cimitir către front.

Waypointul comandat este acum păstrat până la atingerea razei de 18 metri.
Abia apoi se alege legătura următoare. Sunt păstrate filtrele de acces la noduri,
porțile și verificarea compartimentelor IC. La atingerea ultimului nod se șterge
cursorul tactic și se face apropierea locală de obiectiv.

În pregătire, apărătorii primeau ordine de deplasare spre porți la fiecare tick
AI, inclusiv în perioada de așteptare. Ambele echipe primesc acum STAY, iar o
deplasare existentă este oprită. Boții care stau deja nemișcați nu primesc repetat
comenzi de deplasare/oprire. La începutul rundei, STAY este eliminat prin fluxul
tactic existent, iar echipele avansează conform rolului.

Checkpointul 5222, din interiorul porții albastre, era proiectat pe o fâșie
izolată a mesh-ului. A fost mutat aproximativ doi metri pe componenta de teren
accesibilă dinspre cimitir: `1408.92, -220.981, 32.1082`. Schimbarea este inclusă
atât în migrarea de bază, cât și în migrarea incrementală
`sql/custom/world/npcbot_2026_10_26_01_sa_navigation.sql`, aplicată numai în world2.
Nu este necesară regenerarea tile-urilor de navigație.

## Verificări

- `python3 contrib/tests/siege_movement_regression.py`: compilează secțiunile
  reale de planificare și pregătire din sursă cu mișcători simulați. Atacatorii
  ajung pe ambele culoare de plajă, apărătorii ajung pe ambele fronturi, la trei
  viteze diferite. Ambele pregătiri sunt simulate pentru ambele echipe, cu 1200
  de tick-uri fără mișcare sau repetarea comenzii STAY.
- Același test, folosind planificatorul din HEAD anterior schimbării, reproduce
  oscilația lângă spawn și eșuează.
- `contrib/tests/sa_navigation_data.cpp`, compilat cu Detour-ul serverului,
  verifică datele reale instalate: 68 tile-uri compatibile, proiecțiile celor 27
  de checkpoint-uri și 34 de trasee direcționate de plajă/apărare. Vechea poziție
  5222 nu avea drum complet dinspre 5209; poziția corectată are.
- `contrib/tests/siege_battlegrounds.cpp`: trece verificările grafului SQL,
  celor 64 de stări de porți, traversărilor fizice și vehiculelor.
- Compilarea completă worldserver și `git diff --check`: trecute.

Log de compilare: `/tmp/sa-movement-final-build.log`.

La cererea utilizatorului, procesul activ nu a fost oprit sau repornit. Binarul
și datele pentru următoarea pornire sunt actualizate; meciul curent păstrează
codul și waypointurile încărcate anterior. Retestarea vizuală în client se face
după o repornire autorizată, în ambele runde și pe ambele echipe. Simularea
mișcării nu înlocuiește verificarea geometriei și a comportamentului în luptă.
