# Implementarea auditului NPCBots — 17 noiembrie 2026

Acest document înlocuiește recomandările de remediere din raportul istoric pentru codul livrat prin acest commit. Manifestul instalării este generat separat în directorul de backup, cu SHA-ul sursei, identitatea build-ului și checksum-urile binarelor/configurației; nu publică parole ori conținutul configurației.

## Corecții integrate

| Audit | Implementare |
|---|---|
| H1 | Urgențele exclud boții liberi înainte de orice acces Player la master; verifică hartă, fază, assist și LoS. Întreruperea este independentă de roll-ul de 75%, numai dacă există un heal cunoscut, disponibil și accesibil ca power/school. |
| H2 | Start și cancel folosesc același plafon OOC/personal. Nu se mai anulează un heal valid la 91–94%. Ambele unități trebuie să fie OOC; GCD și costurile rămân native. |
| H3–H5 | Contract comun pentru ținte vii; colecție solo/grup/pets/vehicles/icons, deduplicare GUID, verificare înainte de cast. Prioritate pe benzi de urgență, preferințe cumulative în bandă, fallback la următoarea țintă. Comparatorul nu dereferențiază Unit. |
| H6 | Top-off peste 85% alege heal-ul direct disponibil cu cost minim; evită reînnoirea inutilă a HoT-urilor. Regrowth respectă propria aură în afara urgențelor. HEAL+TANK nu blochează healerul druid exclusiv din cauza formei/rolului de tank. |
| H7 | Snapshot de GUID-uri pentru casterii grupului pe update-ul BotMgr; spell-urile și țintele sunt citite live, astfel încât pornirea/întreruperea/terminarea cast-urilor nu lasă rezervări stale. Scanări eșalonate, retry per țintă, vector reutilizat. Proiecții semnate și perioade nenule. Diagnostic opțional agregat: decizii, candidați, eșecuri, întreruperi și histograme p50/p95/p99/max. |
| L1 | Transferul quest loot este acum API în Loot, cu verificarea qlist-ului neconsumat și contabilizare sursă/destinație. Consolidare și pe cadavrul principal. FFA, loot condiționat, loot de grup sau deja vizualizat rămâne pe cadavrul sursă. Flag-urile se curăță doar prin contabilizarea nativă. Cererea trebuie să aibă drept de loot și distanță validă la cadavrul principal înaintea oricărei mutări. |
| C1 | BotMgr atașează companionul înainte de commit-ul owner-ului. Selecția nu poate prelua template-uri deținute offline. Schimbarea de clasă păstrează botul anterior până la construirea celui nou și încearcă restaurarea la eșec. Invocările stone sunt serializate, iar cooldown-ul este persistat sincron după succes. Dismiss/switch ignoră boții temporari RDF/raid și elimină creaturile de companion generate după detașare, fără a șterge spawns native. |
| C2 | Ledger per personaj/item; livrările parțiale se reiau, iar obiectele vândute nu sunt acordate din nou. Inventarul, ledger-ul și markerul final sunt salvate în aceeași tranzacție InnoDB. Markerii vechi sunt păstrați. Distribuția de proficiențe rogue/priest/mage/warlock/druid a fost corectată. |
| D1 | Timerul DK scade cu timpul efectiv dintre scanări; fallback-ul de spawn nu depășește raza de despawn. Valorile float sunt finite și limitate. |
| I1 | Installer cu manifeste distincte world/characters/auth, SHA-256, dependențe, backup gzip verificat înainte de scriere, ledger și lock DB. Nu repetă migrațiile aplicate și oprește operația la checksum/stare incompletă. DDL-ul custom este în migrația characters, fără CREATE TABLE în constructorul scriptului. |
| T1 | Contractul GM și harness-urile city raid au fost actualizate; teste suplimentare pentru healer policy/targets, quest loot, heirloom delivery și manifeste. |
| G1 | Generator de release manifest și integrare CMake opțională ccache/PCH; cache persistent 10G. Au fost activate cerințele PCH `pch_defines,time_macros`; o probă PCH a demonstrat un cache hit real. |

## Configurație și comportament

Setările noi se află la sfârșitul `worldserver.conf.dist`, împreună cu valorile implicite:

- Top-off 95%, mana minimă 25% pentru completarea peste 85%; urgențele nu sunt blocate de acest buget OOC.
- Scanare OOC 750 ms, combat 250 ms, retry 750 ms, stagger după GUID. Urgențele de cast au cale separată.
- Player aid 40 yd, fără intervenție ambient în duel și fără ruperea stealth-ului casterului pentru ajutorul altora. Resurecția ambient caută o rută în loc să teleporteze botul prin obstacole.
- Ghost multiplier 3x, citit din configurația validată; vitezele fantomelor existente sunt recalculate la reload.
- Bones 1800 s și revive wanderer 300000 ms configurabile; evenimentele WG/city păstrează propriile valuri.
- AoE loot 120 yd și maximum 60 cadavre, cu validare signed/finite și maximum administrativ 300.
- Companion stone 18000 s, prima invocare cu bypass existent; heirlooms pot fi dezactivate.
- City defenders au leash explicit de 65 yd, inclusiv pentru focus/victima bossului; bossul are boundary de 90 yd în timpul evenimentului.
- Wintergrasp: `PlayerMax=40` și `BotWar.TargetPerTeam=40`, cu prioritatea umană existentă.

AutoLearn numără tranzițiile reale ale spell-urilor învățate, inclusiv rank-uri/reactivări și trainer wrappers; nu mai raportează încercările eșuate drept abilități învățate. Limita de 15 treceri rămâne și produce avertizare când este atinsă cu progres.

Implementarea existentă `LoadBotCreatureFromDB` acceptă numai hărți cu instanceId 0. Stone returnează acum eroare controlată dacă este cerut un companion nou dintr-o instanță; companionul se creează înainte de intrare și folosește teleportarea nativă. Nu se mai ajunge la ASSERT. GM poate atașa fără limita personală, dar contoarele de 8 biți și capacitatea grupurilor rămân limite tehnice.

Aliasul istoric `master` pentru free bots nu a fost eliminat din întregul motor; fluxul modificat nu îl folosește ca Player. Nu există cache global de Unit* și nu au fost mutate citiri Unit/Group pe thread-uri auxiliare.

## Verificare reproductibilă

```sh
python3 contrib/tools/run_custom_audit.py --jobs 2 --output-dir /tmp/trinity-custom-audit-tests
python3 contrib/tests/aoe_quest_transfer.py --sanitize
python3 contrib/tests/heirloom_delivery.py --sanitize
python3 contrib/tests/healer_targets.py --sanitize
cmake --build /home/trinity/build/npcbots --target worldserver authserver --parallel 4
```

Rularea extinsă inițială: 50/50 trecute. Rularea finală cu runner-ul de mai sus: **53/53 trecute**. Cele trei harness-uri noi și politica healer au trecut ASan/UBSan; LeakSanitizer este dezactivat în mediul ptrace. Aceste teste de policy/harness nu certifică gameplay-ul sau duratele MSPT pe un realm încărcat.

Migrația și installerul au fost executate de două ori pe o bază MariaDB temporară: prima aplicare a creat schema și ledger-ul, a doua nu a repetat SQL-ul. Baza temporară a fost eliminată. Migrația characters a fost apoi aplicată pe instalare după backup complet verificat, fără modificarea numărului de personaje ori a markerilor anteriori. Tabelele inventory/item_instance/characters/marker existente sunt InnoDB.

Build-urile și testele produc loguri în `/tmp/trinity-implementation-tests/` și `/tmp/trinity-*-build.log`. Pentru identitatea livrării:

```sh
python3 contrib/tools/trinity_release_manifest.py \
  --build /home/trinity/build/npcbots --install /home/trinity/server \
  --output /home/trinity/source-archives/RELEASE_DIRECTORY/release.json
```

## Livrare și limite

Backupul instalării este în `/home/trinity/source-archives/audit-implementation-20261117T173230Z/`. Conține dump-ul characters, configurația anterioară și configurația ccache anterioară; binarele precedente și manifestul sunt adăugate la instalare. Parolele/configurațiile locale și dump-urile private nu sunt incluse în Git.

Upstream-ul public verificat are SHA `a824f510b72012de45b27c7ccf574dc424a25483`. Istoria fork-ului local începe cu un import squashed și nu are merge-base cu acel upstream. Nu se face merge automat cu `--allow-unrelated-histories`: este necesar import/range-diff pe o bază de proveniență explicită. Verificarea nu schimbă codul remote ori branch-ul upstream.

Nu au fost pornite serviciile pentru un test cu clienți. Sunt încă necesare scenariile live de raid, pathing, duel/stealth, logout/teleport și profilare MSPT descrise în audit. Scriptul city raid continuă să acopere Stormwind și Orgrimmar; acest audit nu adaugă capitale/encounter-uri noi.
