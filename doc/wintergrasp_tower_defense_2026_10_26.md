# Wintergrasp: garnizoane la turnurile atacatorilor

## Problema identificată

Atacatorii aveau 90 de soldați pe culoarele de asalt și 30 repartizați atelierelor,
fără grupe BotWar pentru apărarea turnurilor sudice. Apărătorii trimiteau câte
15 soldați spre fiecare turn, împreună cu vehicule de asediu. Doar gărzile și
tunurile native rămâneau la turnuri. Când toate cele trei turnuri cad, nucleul
reduce timpul de luptă cu zece minute.

## Repartizare nouă

La un roster de 120 de boți, atacatorii primesc 25 de soldați pe fiecare culoar
de asalt și câte 15 gardieni la fiecare turn sudic: 75 pentru cetate și 45 pentru
turnuri. Nu sunt creați participanți suplimentari. Intrarea jucătorilor reali
reduce roster-ul prin bugetul existent.

Ordinele depind de rolul atacator/apărător, indiferent dacă acesta este Alliance
sau Horde. Repartizarea apărătorilor rămâne 75 la cetate și 45 spre turnuri.
Garnizoanele și asaltul sunt intercalate în roster, pentru ca efectivele reduse
să primească și apărare sudică. De la 30 de boți în sus, toate cele trei turnuri
au o grupă de gardă.

Atacatorii vest/est capturează în continuare Broken Temple/Sunken Ring înainte
să avanseze. Cele 30 de posturi statice de atelier au fost înlocuite de apărarea
turnurilor, împreună cu 15 posturi mutate din asalt. Gardienii nu se îmbarcă în
vehicule libere și nu își abandonează astfel turnul.

După distrugerea unui turn, supraviețuitorii pornesc către culoarul de asalt
corespunzător. Soldații refăcuți și vehiculele ulterioare primesc și ele obiectivul
actualizat, în loc să fie trimise repetat la un turn deja distrus. Un obiect de
turn care nu este încă disponibil în grilă nu este considerat distrus.

Pozițiile centrală și estică sunt cele ale gărzilor native existente; poziția
vestică folosește apropierea de turn din traseul existent. Formațiile și
pathfinding-ul sunt păstrate. Numărul participanților, statisticile, daunele,
condițiile de capturare și penalizarea nativă de zece minute nu sunt modificate.

## Verificări

```sh
c++ -std=c++20 -O2 -Wall -Wextra -Werror contrib/tests/wintergrasp_bot_war.cpp -o /tmp/wg-tower-defense-test
/tmp/wg-tower-defense-test
python3 contrib/tests/wintergrasp_tower_defense.py
python3 contrib/tests/wintergrasp_map_lifecycle.py
python3 contrib/tests/wintergrasp_movement_speed.py
```

Toate trec. Testele verifică bugetele de populație, coeziunea grupelor,
repartizarea 75/45, acoperirea turnurilor la efective reduse, ambele facțiuni și
toate cele opt combinații de turnuri distruse. Testul nou compilează funcțiile
reale de rezolvare a culoarului și actualizare a traseului, cu dependențe simulate:
verifică menținerea postului la turn intact/indisponibil, redirecționarea
supraviețuitorilor, păstrarea slotului și obiectivele pentru înlocuitori.

Compilarea completă worldserver a reușit fără avertismente:
`/tmp/wg-tower-defense-build.log`. Nu este necesar SQL sau un addon nou.

Schimbarea remediază lipsa garnizoanelor din tactică. Rata reală de victorii
și geometria deplasării trebuie urmărite în meciuri pe ambele roluri; testele
offline nu demonstrează un procent de victorii sau o cucerire garantată.
