# Wintergrasp: ateliere, ranguri și coordonare — 2026-11-08

Această intervenție continuă corecțiile din `wintergrasp_aggression_2026_11_08.md`.

## Construcție la atelier

Constructorul este liderul grupei, aflat în viață, neîmbarcat și în afara luptei. Nu mai este teleportat și nu îi este întreruptă lupta pentru a construi. Se deplasează cu pathfinding până la un inginer prietenos dintr-un atelier controlat de facțiunea sa. Căutarea inginerilor este locală: 100 m în jurul steagului, respectiv 250 m în cetate, cu verificarea suplimentară a distanței față de atelier. Constructorii din câmp nu sunt trimiși în cetate prin ziduri intacte.

Construcția durează cinci secunde după sosire la cel mult 8 m. La fiecare actualizare se verifică viața și facțiunea inginerului, controlul atelierului, prezența și starea constructorului, rangul și capacitatea nativă BattlefieldWG. Pierderea atelierului, intrarea în luptă sau epuizarea capacității anulează comanda. Rămâne un loc liber pentru constructorii umani dacă există participanți reali în facțiune. Un vehicul nou așteaptă îmbarcarea șoferului și nu se deplasează singur până atunci.

## Ranguri câștigate

Boții pornesc Recruit. Primesc credit dacă sunt în viață, la maximum 40 m de un adversar eligibil ucis de facțiunea lor. Sunt eligibili jucătorii, soldații scriptului și gărzile Wintergrasp 30739/30740; monștrii obișnuiți, sinuciderea și uciderea unui aliat nu oferă credit. Callback-urile acoperă kill-urile jucătorilor, soldaților, vehiculelor, turelelor și tunurilor autonome.

Pragurile reproduc `BattlefieldWG::PromotePlayer`: cinci credite pentru Corporal, zece pentru Lieutenant. Corporal poate construi catapultă; Lieutenant poate construi demolisher sau Siege Engine. Aura afișată și stivele urmăresc rangul. Creditul este păstrat în slotul soldatului la moarte/respawn și resetat la curățarea bătăliei. Nu sunt schimbate rangurile sau recompensele native ale jucătorilor.

## Recuperare și apărare după presiune

La fiecare 15 secunde, coordonatorul măsoară prezența soldaților și jucătorilor PvP în raza de 80 m a celor patru ateliere de câmp. Alege grupe de infanterie pentru recuperarea atelierelor pierdute sau întărirea celor atacate. Prioritizează atelierele pierdute, apoi diferența numerică locală și apropierea grupelor disponibile.

Sunt redirecționate cel mult patru grupe (20 de soldați) simultan și cel mult două grupe printr-o decizie de întărire a unui obiectiv. Nucleul cetății, gărzile turnurilor intacte și grupele cu vehicule/constructor ocupat nu sunt selectate ca rezerve noi. Trupele aflate în cetate nu sunt trimise prin ziduri intacte înapoi pe câmp. Ordinele noi se aplică după ieșirea din luptă, fără oprirea forțată a acesteia.

Grupele desemnate păstrează obiectivul până la captură și eliminarea presiunii inamice; apoi reiau asaltul/apărarea inițială. Contraatacurile își păstrează progresia turn -> atelier, fără resetarea traseului de către coordonator. Atelierele sudice au obiective și steaguri proprii. Infanteria care capturează nu pleacă să ocupe scaune de vehicul; șoferul desemnat își poate conduce vehiculul spre obiectiv.

## Abilități și echipaje

- Timerele abilităților soldaților și armelor vehiculelor sunt calculate după rezultatul real al lansării. Eșecul produce o reîncercare limitată la 500 ms, fără consumarea artificială a întregului cooldown AI.
- O abilitate secundară eșuată nu mai suprimă atacul principal sau lovitura melee disponibilă.
- Whirlwind și Cone of Cold folosesc ținta proprie; Charge folosește rangul 3. Nova și Blink nu mai sunt lansate simultan sub același GCD.
- Shield verifică și Weakened Soul. Healerii fără mana nu se opresc repetat pentru vindecări imposibile; ranged fără resurse pot continua cu atacuri melee.
- Demolisherul se retrage la distanța de tragere când este prea aproape; proiectilele și Ram folosesc verificările native ale spell-urilor, fără bypass general cu `triggered=true`.
- La apropierea la 8 m a unui aliat viu, pe jos, neîmbarcat, în afara luptei și cu vizibilitate, botul eliberează scaunul de șofer/tunar. Are o pauză de reîmbarcare de 20 s. Un pasager uman nu este evacuat de această logică. Un aliat în luptă sau călare nu întrerupe echipajul doar trecând pe lângă vehicul.

## Verificare

Build complet `worldserver` în `/home/trinity/build/npcbots`, cu `-j2`.
Log: `/tmp/wg-realism-20261108/build-release.log`.

Nouă suite de regresie: `wintergrasp_logistics.py`, `wintergrasp_combat_control.py`, `wintergrasp_tower_defense.py`, `wintergrasp_vehicle_navigation.py`, `wintergrasp_movement_speed.py`, `wintergrasp_map_lifecycle.py`, `wintergrasp_realism.py`, `wintergrasp_bot_war.cpp`, `wintergrasp_vehicle_cleanup.cpp`. Log: `/tmp/wg-realism-20261108/tests.log`.

Testele noi execută metodele de producție pentru alegerea inginerului, construcție, acordarea creditului de kill, ranguri, păstrarea progresului la moarte, rezultatele spell-urilor, predarea scaunelor și coordonarea ambelor facțiuni. Harta, mișcarea și spell-urile sunt simulate în aceste teste. Verificarea statică `wintergrasp_realism.py` nu este o simulare de luptă.

Nu a fost rulată o bătălie în client și nu este demonstrată o anumită rată a victoriilor. Rămân necesare verificarea traseelor pe mmaps/vmaps reale, observarea unei construcții la fiecare atelier, preluarea efectivă a vehiculelor din client și bătălii cu facțiunile inversate. Cele patru profiluri de clasă și infrastructura traseelor rămân simplificate; coordonarea nouă reacționează la presiune, fără a reproduce toate tacticile unor jucători reali.

## Verificare în joc după pornirea binarului

1. Pornește bătălia cu `.bf start 1`, acceptă invitația cu un jucător real și urmărește populația din logurile `Wintergrasp BotWar`.
2. Observă primele lupte: recruții nu trebuie să construiască imediat blindate grele; după promovare trebuie să se deplaseze la un inginer aliat.
3. Pierde atelierul în timpul celor cinci secunde de construcție: comanda trebuie anulată, fără vehicul nou sau teleportarea constructorului.
4. Capturează un atelier din spatele frontului: trebuie să reacționeze grupe de infanterie disponibile, să rămână până la captură și apoi să reia ordinele inițiale.
5. Apropie-te pe jos și în afara luptei de un vehicul condus de bot; verifică scaunul eliberat și preluarea controlului. Repetă la turelă. Vehiculul fără șofer trebuie să rămână oprit.
6. Repetă cu cealaltă facțiune atacatoare și urmărește porțile, relicva, durata bătăliei și performanța serverului. Nicio victorie nu este acordată artificial.
