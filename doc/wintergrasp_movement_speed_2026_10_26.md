# Wintergrasp: viteza de alergare și mount

## Cauză și corecție

Soldații foloseau `SetSpeed(MOVE_RUN, 1.f)` și `SetSpeed(MOVE_RUN, 2.f)`.
În acest nucleu, `SetSpeed` primește viteza absolută în metri pe secundă,
iar `SetSpeedRate` primește multiplicatorul vitezei de bază. Rezultatul era
1 m/s pe jos și 2 m/s pe mount, mult sub alergarea normală.

Helper-ul `SetBotWarMovementSpeed` setează acum `SetWalk(false)` și
`SetSpeedRate(MOVE_RUN, mounted ? 2.f : 1.f)`: 7 m/s pe jos, respectiv
14 m/s pentru mount-ul terestru de nivel 80. Este folosit la inițializarea
soldaților, luptă, vindecare, mount/dismount și înainte de urcarea în vehicule
sau turele. Astfel, viteza de mount nu rămâne activă la coborârea din vehicul.

Pornirea unui segment de traseu setează de asemenea `SetWalk(false)`, inclusiv
pentru vehicule, astfel încât spline-ul și urmărirea acestora să folosească
alergarea. Multiplicatorul vehiculelor rămâne cel al template-ului lor.

Nu sunt necesare schimbări SQL sau de configurație.

## Verificare

```sh
python3 contrib/tests/wintergrasp_movement_speed.py
python3 contrib/tests/wintergrasp_map_lifecycle.py
```

Ambele trec. Testul de viteză folosește helper-ul real și extrage din Unit.cpp
tabelele de viteze și implementările `GetSpeed`/`SetSpeed`. Reproduce eroarea
de 1/2 m/s și verifică vitezele 7/14 m/s, modul run și tranzițiile mount/dismount,
pentru creaturi cu și fără control de jucător. Verifică și integrarea la trasee
și îmbarcare. Nu este o simulare vizuală a clientului sau un test al tuturor
efectelor PvP asupra vitezei.

Compilarea completă worldserver a reușit fără avertismente. Log:
`/tmp/wg-movement-speed-build.log`. Binarul este instalat în
`/home/trinity/server/bin/worldserver`; procesul trebuie repornit controlat
pentru încărcarea corecției.
