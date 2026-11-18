# Wintergrasp: vehicule active și ofensivă — 2026-11-08

## Probleme confirmate în cod

1. `npc_wg_bot_vehicle::OnCharmed` și callback-ul turelei apelau necondiționat `UnitAI::OnCharmed(false)`. Acesta programează schimbarea AI-ului, iar `Unit::UpdateCharmAI` selectează `PossessedAI` pentru un vehicul. După îmbarcarea botului, scriptul care conduce și folosește armele era înlocuit. Callback-urile păstrează acum scriptul pentru șoferii/tunarii 80501–80508. Controlul unui jucător folosește în continuare mecanismul nativ și restaurează scriptul la coborâre.
2. Soldații mobili păstrau `home` la spawn. `Creature::CanCreatureAttack` verifică distanța victimei față de acest punct; după un drum lung, țintele puteau fi respinse. Home urmărește acum poziția soldatului în afara luptei; la angajare se fixează originea, cu limita existentă de urmărire de 100 m.
3. Ranged opreau deplasarea pentru spell-uri chiar când ținta depășea raza de 30 m. Acum se apropie înainte de atac și urmăresc limitat țintele care ies din vizibilitate; după 8 secunde de astfel de actualizări reiau ordinele.
4. Un soldat ajuns anterior la atelier, apoi deplasat de luptă, putea aștepta departe de steag cât timp atelierul aparținea inamicului. Verificarea distanței și revenirea sunt executate înaintea așteptării capturii.
5. `MoveFollow` spre un vehicul întrerupea actualizarea traseului tactic. Escortele își continuă acum propriul traseu comun. Numai primele două locuri ale grupei pot căuta locuri de echipaj, iar infanteria de pe flanc nu se îmbarcă cât timp trebuie să captureze primul atelier. Șoferul construit de coordonator rămâne la volan.
6. Apărătorii și vehiculele desemnate atelierelor nordice porneau în cetate și încercau să ajungă la steag prin ziduri/porți intacte. Dacă atelierul este deținut, pornesc acum la acesta.
7. Siege Engine încerca Ram la 24 m. În DBC-ul instalat, efectul 87 al spell-ului 54109 folosește SpellRadius 8, adică 5 m. Apropierea urmărește acum 3 m, iar lovitura este lansată la maximum 4 m. Actualizarea traseului nu mai înlocuiește apropierea de zid cu revenirea la checkpoint.
8. Execute folosea 47838 (Incinerate). Folosește acum 47471 (Execute).
9. Formația finală de la relicvă este comprimată pentru a nu lăsa soldații desemnați în afara razei de interacțiune.

## Comportament și echilibru

La 120 de boți atacatori: 30 centru, 30 vest, 30 est și câte 10 la fiecare turn sudic. Anterior: 25 pe culoar și 15 la fiecare turn. Grupele rămân stabile la intrarea/ieșirea jucătorilor. Apărătorii își păstrează distribuția.

Infanteria nu mai lansează Fire Cannon 57609 asupra clădirilor sau vehiculelor. Cannon rămâne arma turelei cu tunar, Ram a șoferului și Hurl Boulder a demolisherului. Catapulta oferă sprijin împotriva infanteriei. Nu sunt introduse scăderi directe ale vieții clădirilor, capturi instantanee sau victorii forțate. Vehiculele continuă să respecte capacitatea BattlefieldWG.

## Verificări

Au trecut:

- `wintergrasp_combat_control.py`: execută callback-urile de producție pentru cele opt tipuri de soldat, coborâre, control de jucător și restaurare; verifică apropierea ranged și recuperarea după pierderea vizibilității.
- `wintergrasp_tower_defense.py`: ambele facțiuni, toate combinațiile de turnuri, redistribuire și revenirea efectivă la un atelier inamic după deplasare.
- `wintergrasp_bot_war.cpp`: bugete, limite, prioritatea oamenilor și distribuția 90/30.
- `wintergrasp_vehicle_navigation.py`: navigația de producție în scenarii simulate de coliziune/blocaj și recuperare.
- `wintergrasp_movement_speed.py`, `wintergrasp_map_lifecycle.py`.
- `wintergrasp_vehicle_cleanup.cpp`: 5808 de scenarii de curățare.
- `wintergrasp_realism.py`: verificări statice existente; acestea nu simulează o bătălie.

Build: `/home/trinity/build/npcbots`, target `worldserver`, `-j2`.
Log final: `/tmp/wg-aggression-20261108/build-final.log`.

Nu a fost efectuată o bătălie în client. Aceste verificări confirmă corecțiile mecanismelor, nu o rată a victoriilor sau traversarea tuturor rampelor cu datele reale de coliziune. De urmărit în joc: capturarea ambelor ateliere nordice, vehicule care continuă după îmbarcare, lovituri reale asupra fiecărei porți, deblocarea/capturarea relicvei și o bătălie cu facțiunile inversate. Timpul și rezultatul trebuie măsurate înaintea altor ajustări de echilibru.
