# Isle of Conquest — crash și navigație, 25 octombrie 2026

## Dovezi păstrate

La 20:10:49, procesul `worldserver` a primit SIGSEGV. Linia kernelului indică `ip 0x55b056fae346` în segmentul executabil început la `0x55b055f03000`. Segmentul ELF are adresa virtuală `0x439000`; adresa simbolică rezultată, `0x14e4346`, se rezolvă în `Trinity::Assert`, `Errors.cpp:76`, în binarul instalat la momentul crash-ului. Disassembly-ul confirmă instrucțiunea de scriere intenționată la adresa zero folosită de nucleu pentru oprirea după o aserțiune.

Acest lucru confirmă o aserțiune, nu identifică funcția apelantă. Apport a ignorat executabilul neîmpachetat; limita de core era zero, iar sesiunea `screen` a dispărut fără să păstreze mesajul stderr. Nu există un core dump/stivă a acelui meci care să confirme exact aserțiunea declanșată.

Logul jocului conține și:

- Tile-uri de navigație pentru harta 628 cu versiunea 16, deși nucleul și generatorul actual folosesc versiunea 15.
- Deplasări care trec de la pathfinding nereușit la fallback direct.
- Deplasarea unui bot în zona vechiului waypoint 5318, cu înălțime crescândă față de coordonata estimată din SQL.

Backup-ul incidentului, inclusiv binarul original, logurile, tile-urile și SQL-ul de revenire, este în `/home/trinity/source-archives/ioc-crash-20261025-x7o_zyc4`.

## Corecții în cod

1. **Vehicule autonome:** pasagerii primesc ROOT la urcare. Verificarea generală a crowd control-ului returna controlul către rutina clasei, care putea apela `DoVehicleActions` → `CheckVehicleAttackTarget` → `_getVehicleTarget`, unde exista `ASSERT(!IAmFree())`. Vehiculele de asediu păstrează acum controlul tactic în această situație; rutina veche pentru vehicule cu proprietar nu mai rulează pentru boții autonomi. Este o cale de aserțiune concretă și compatibilă cu dovezile, fără a pretinde că am recuperat stiva lipsă a incidentului.
2. **Porți proprii:** `ICEdgeOpen` verifică acum cele șase porți pentru ambele echipe. O poartă prietenoasă intactă nu mai este considerată traversabilă.
3. **Portaluri:** când infanteria trebuie să intre/iasă din propria fortăreață și nu există o breșă, se deplasează la obiectul nativ de teleportare al echipei și îl folosește prin `GameObject::Use`. Sunt verificate existența, disponibilitatea, distanța și LOS. Vehiculele nu sunt teleportate prin acest mecanism.
4. **Atașare la graf:** punctul de plecare se alege în compartimentul fizic real, nu de cealaltă parte a zidului. Un checkpoint comandat nu este tratat ca deja atins de la 250 de metri.
5. **Spline-uri:** mișcările calculate ale wanderers IC și vehiculelor conduse de ei refuză shortcut-uri, tile-uri lipsă, capete îndepărtate de mesh și traversări ale porților intacte. Sunt acceptate prefixe navigabile parțiale pentru drumurile mai lungi decât limita nativă de 74 de poligoane; nu se forțează destinația prin obstacole. Apropierea de ținte pentru atac verifică și barierele fortăreței. Deplasarea pe transporturi și salturile explicite își păstrează mecanismele native.
6. **Evade:** wanderers SA/IC nu mai folosesc patrularea generică și fallback-ul ei direct pentru revenirea după luptă; sistemul tactic de BG deține această deplasare.
7. **Diagnostic:** pe Linux, aserțiunile păstrează mesajul și un backtrace în `TrinityCore.assert.log`, în directorul curent al procesului, înainte de oprirea nativă. Nu sunt suprimate aserțiunile nucleului.

## Corecții ale datelor

Cele șase checkpoint-uri de drum aveau înălțimi aproximative. Punctul 5318 proiectat pe cel mai apropiat poligon ajungea pe o porțiune izolată de stâncă, care nu era accesibilă de pe drum. A fost mutat pe o porțiune verificată a drumului de coastă; celelalte cinci puncte au primit înălțimile navigabile.

Au fost actualizate migrarea pentru instalări noi și migrarea incrementală `sql/custom/world/npcbot_2026_10_25_01_ioc_navigation.sql`. Doar migrarea incrementală a fost aplicată în baza activă `world2`, cu backup pentru cele șase rânduri.

Cinci tile-uri incompatibile au fost mutate în arhivă: `6282831`, `6283030`, `6283231`, `6283332`, `6283338`. Generatorul corespunzător nucleului nu produce geometrie traversabilă pentru aceste cinci tile-uri din datele sursă existente. Nu le-am schimbat artificial antetul și nu am instalat tile-uri speculative. Cele 56 de tile-uri compatibile au rămas nemodificate; ele acoperă punctele de spawn, obiectivele și drumurile verificate. Intervenția asupra fișierelor de navigație este limitată la harta 628.

## Verificare

- Compilare `worldserver` din sursa modificată, fără pornirea serverului pentru test.
- Cele șapte suite offline NPCBots trec cu `-Wall -Wextra -Werror`.
- Regresii noi în suita SA/IC: 64 de stări ale porților pentru fiecare echipă, 768 traversări fizice în ambele direcții, ROOT-ul pasagerilor, separarea rutinei cu proprietar și respingerea flag-urilor de shortcut/fallback.
- Suita SA/IC trece cu AddressSanitizer și UndefinedBehaviorSanitizer.
- Testul opțional `contrib/tests/ioc_navigation_data.cpp` încarcă datele reale în Detour-ul compilat din același repository: **56 de tile-uri**, **23 de checkpoint-uri**, **64 de legături direcționate**, apropierea de portalul interior al ambelor echipe. Include drumurile în mai multe prefixe și verificarea segmentelor față de ziduri.
- Un executabil separat de test a apelat implementarea reală `Trinity::Assert`, cu SIGSEGV interceptat în acel proces de test. Mesajul și backtrace-ul au fost păstrate în fișier chiar fără consolă. Nu s-a pornit sau oprit niciun server pentru acest test.

Aceste verificări nu reproduc un meci complet cu clientul. Retestarea în joc rămâne necesară pentru a confirma comportamentul în timpul capturilor, luptei, urcării în vehicule și folosirii portalurilor. Worldserver a rămas oprit; authserver-ul existent nu a fost modificat sau repornit.

## Instalare

Binarul corectat este instalat în `/home/trinity/server/bin/worldserver`. SHA256: `c603a7872b3d3d653659ed602c89420d2aa63d3c619e62cd81043132ec1999c5`. Compilarea finală nu a emis avertismente. Manifestul și logurile verificărilor sunt în arhiva incidentului indicată mai sus. Worldserver nu a fost pornit.
