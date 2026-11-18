# Wintergrasp: Comportament Wander pentru Soldații în Poziții Strategice

## Cauza Staționării (Root Cause)
În implementarea inițială a asaltului/apărării din Wintergrasp (`src/server/scripts/Northrend/zone_wintergrasp.cpp`):
1. **Puncte finale / Strategice**: În rutele de apărare a cetății (`KeepCenter`, `KeepWest`, `KeepEast`), de apărare a turnurilor sudice (`GuardSouth`, `GuardSouthWest`, `GuardSouthEast`) sau la ateliere (`WorkshopWest`, `WorkshopEast`), rutele conțin un singur punct de destinație.
2. **Sosirea la destinație**: Când `Arrived` devenea `true`, condițiile:
   - `Blocked(wg)` (așteptare la porți/ziduri închise)
   - `Point + 1 >= BotRoutes[Route].size()` (capăt de rută tactică / obiectiv atins)
   - `Point == 0 && (Route == West || Route == East)` și atelierul nu e încă asigurat
   executau un simplu `return;`.
3. **Imobilitate completă**: `VehicleNavigation.Move` oprește mișcarea la finalul spline-ului (`StopMoving()`). Fără logică de patrulare sau wander, soldații rămâneau complet înghețați în aceeași poziție, dând impresia de statui sau boți blocați.
4. **Viteză spline**: `BotWarVehicleNavigation::Launch` apela forțat `spline.SetWalk(false);`, împiedicând orice deplasare la pas (`walk`).

## Implementare Soluție

1. **Viteză de deplasare dinamică în spline (`Launch`)**:
   - `spline.SetWalk(vehicle->IsWalking());`
   - Permite deplasarea la pas când soldatul are `SetWalk(true)`, păstrând alergarea normală (`run`) pentru blindate și deplasări tactice.

2. **Distanță de oprire adaptivă**:
   - Vehiculele păstrează toleranța de 4.0m conform gabaritului.
   - Infanteria folosește pragul de 1.5m (`vehicle->IsVehicle() ? 4.f : 1.5f`), prevenind oprirea prematură.

3. **Logică `UpdateWander` în `BotRouteMovement`**:
   - Exclude automat vehiculele (`bot->IsVehicle()`), pasagerii (`bot->GetVehicle()`), soldații în luptă (`bot->IsInCombat()`) și unitățile cu restricții de mișcare.
   - La sosirea la destinație, timerul de patrulare este inițializat decalat (`urand(2000, 7000)`) pentru ca soldații din același pluton să nu se miște sincron.
   - Când timerul expiră, soldatul descalecă dacă era călare (`bot->Dismount()`), trece la pas (`bot->SetWalk(true)`), selectează un punct aleatoriu la o distanță de 4.5m – 12.0m în jurul ancorei strategice (`anchor = Destination(bot)`), proiectează punctul pe teren navigabil prin `ProjectBotWarGround` și inițiază deplasarea prin `VehicleNavigation.Move(bot, target)`.
   - Între deplasări, soldatul staționează temporar ca santinelă timp de 5 – 11 secunde (`urand(5000, 11000)`).

4. **Toleranță ancoră mărită la 25m**:
   - În `Update`, verificarea `bot->GetExactDist2d(&destination) > 25.f` permite soldatului să se miște liber pe o rază de 12m în jurul punctului strategic fără a reseta prematur `Arrived = false`.
   - Dacă soldatul este împins sau se îndepărtează peste 25m, trece automat la alergare (`bot->SetWalk(false)`) și revine la postul său.

5. **Revenire instantanee la luptă și avansare**:
   - La intrarea în luptă (`AttackStart`), `_route.WanderTimer = 0`, viteza revine la alergare, iar lupta are prioritate deplină.
   - Când o poartă este dărâmată (`!Blocked(wg)`) sau un atelier este securizat, `UpdateWander` se întrerupe imediat, punctul rutei avansează (`++Point`), iar trupele pornesc în alergare către următorul obiectiv.

## Validare și Teste

- Toate cele 15 suite automate din `contrib/tests/wintergrasp_*.py` au trecut cu succes (100% PASS), inclusiv suita dedicată `contrib/tests/wintergrasp_soldier_wander.py`.
- Binarul `worldserver` a fost compilat pe mașina remote fără erori și instalat în `/home/trinity/server/bin/worldserver`.
- Serverul a fost repornit curat prin `/home/trinity/control_servers.sh` și este operațional pe porturile 3724 și 8085.
