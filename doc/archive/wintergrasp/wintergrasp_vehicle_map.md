# Vehicule Wintergrasp pe hartă

Implementare: 26 octombrie 2026.

Serverul trimite o dată pe secundă pozițiile vehiculelor ambelor facțiuni către
jucătorii aflați în Wintergrasp. Addonul `contrib/addons/WGVehicleMap` le desenează
pe harta Wintergrasp (`M`) și pe harta de luptă (`Shift+M`): albastru pentru aliați,
roșu pentru inamici; C = catapultă, D = demolisher, S = siege engine. Tooltip-ul
arată tipul și apartenența.

Sunt incluse vehiculele construite de oameni și de boți, ocupate sau libere,
indiferent de distanța de vizibilitate a jucătorului. Sunt excluse tunurile fixe
și turelele accesorii. Registrul folosit este cel nativ `BattlefieldWG::m_vehicles`;
nu se încarcă grile și nu se creează creaturi suplimentare. Citirea pozițiilor și
trimiterea se fac din actualizarea hărții Northrend.

Actualizarea vehiculelor este un helper apelat de `WintergraspBotWarMapScript`,
singurul `WorldMapScript` înregistrat pentru Northrend. Nucleul execută doar
primul script de hartă compatibil; înregistrarea separată a afișării vehiculelor
ar putea împiedica actualizarea armatei de boți. Helper-ul rulează și în pauza
dintre bătălii, pentru a trimite snapshot-uri goale și a elimina marcajele.

Marcajele dispar după distrugere/despawn la următorul snapshot. Snapshot-urile
goale de după luptă, schimbarea zonei și expirarea datelor după cinci secunde
înlătură marcajele vechi. Addonul publică numai snapshot-uri complete.

## Instalare

1. Compilează și instalează worldserver din această sursă; scriptul nou este
   înregistrat în `AddSC_wintergrasp`. Nu necesită SQL sau modificări în config și
   funcționează independent de `Wintergrasp.BotWar.Enable`.
2. Copiază folderul `WGVehicleMap` în client, la `Interface/AddOns/WGVehicleMap`.
3. Activează **Wintergrasp Vehicle Map** în lista de addonuri. Dacă tocmai ai
   instalat folderul, repornește clientul.
4. Pornește worldserver cu binarul actualizat și intră într-o bătălie Wintergrasp.

Protocolul este documentat în README-ul addonului. Nu produce mesaje vizibile
în chat și nu necesită mesaje sau comenzi trimise de client către server.

## Verificare

- Compilare completă `worldserver`, inclusiv linkare: reușită; log de instalare
  inițială: `/tmp/wg-vehicle-map-build.log`.
- `contrib/tests/wintergrasp_vehicle_map.lua`: reușit cu biblioteca Lua 5.3 locală;
  addonul folosește sintaxă și API de limbaj compatibile cu Lua 5.1.
- Testele execută addonul real într-un mediu UI simulat: ambele echipe,
  inversarea culorilor pentru Horde, coordonate, tooltip, cele două hărți,
  snapshot-uri incomplete, eliminări, date invalide, loturi de 48 de vehicule,
  expirare, schimbarea zonei și reconectare.

Nu există încă validare vizuală în client. În joc trebuie verificat un vehicul
aliat și unul inamic, apoi deplasarea și distrugerea lor, pe harta mare și pe
harta de luptă, inclusiv după schimbarea dimensiunii ferestrei hărții. La instalare,
worldserver era oprit; binarul instalat se va folosi la următoarea pornire.
