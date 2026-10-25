> Actualizare 25 octombrie 2026: consultați [auditul curent](prompt_audit_2026_10_25.md) pentru echiparea organică, bonusurile de set pe spec și instalarea actuală. Starea din 24 octombrie de mai jos este istorică.

# Raid: evitarea efectelor de sol și echipare rapidă

## Evitarea pericolelor

`bot_ai::CheckHazardAvoidance()` rulează înaintea rotațiilor de clasă și a
mișcărilor obișnuite din `GlobalUpdate()`. Verifică la 250 ms efectele din jurul
botului, inclusiv când proprietarul se află departe de el. Folosește obiectele
dinamice ostile și trigger-ele de creatură pentru Defile, Slime Puddle,
Choking Gas Bomb, Shadow Trap și Legion Flame. Raza Defile/Slime urmează scala
folosită în scripturile encounter-urilor; Legion Flame folosește spell-ul de
daune 66877 (3 m), nu aura periodică 66201 (care nu are rază în DBC).

Evitarea preferă un pas lateral de 5 m. Dacă balta este prea mare sau crește,
destinația este extinsă până în afara perimetrului, cu marjă de siguranță și
limită de 40 m. Sunt evaluate 32 de direcții, cu prioritate pentru ieșirile
scurte. Destinația trebuie să evite **toate** pericolele din apropiere.

Sunt verificate vizibilitatea, diferența de nivel, traseul mmap și fiecare
segment al traseului. Botul poate ieși din balta în care se află, dar nu poate
alege un segment care intră într-o altă baltă sau care îl duce mai adânc în
cea inițială. Nu există fallback care ignoră obstacolele. Dacă există o ieșire
validă, castul este întrerupt, botul aleargă, iar mișcarea are prioritate până
la ieșire. Dacă nu există o cale sigură, rutina nu blochează vindecările și
restul AI-ului.

Rutina este activă în dungeon-uri și raiduri, inclusiv pentru boții RDF. Nu
mută pasagerii/vehiculele de encounter (Abomination are o mecanică proprie cu
slime), boții controlați sau aflați în teleport/săritură. `FULLSTOP` și
`INACTION` opresc evitarea; `STAY` permite deplasarea de urgență. Efectele de pe
alte etaje, cu diferență Z mai mare de 4 m, sunt ignorate.

Aceasta este evitare reactivă a efectelor existente, nu o implementare completă
a tuturor mecanicilor raidurilor sau predicția viitoarelor casturi.

## Comanda de echipare

Selectează un NPCBot propriu, angajat și activ, apoi folosește:

```text
.npcbot autoequip
.npcbot autoequip blue
.npcbot autoequip t7
.npcbot autoequip t8
.npcbot autoequip t9
.npcbot autoequip t10
.npcbot autoequip wrathful
.npcbot autoequip 264
```

Comanda generează obiecte și folosește permisiunea administrativă deja existentă
`RBAC_PERM_COMMAND_NPCBOT_CREATENEW` (70036). Nu cere activarea modului GM,
dar contul trebuie să aibă permisiunea. Chiar și un GM trebuie să aibă botul
angajat activ în propriul BotMgr; comanda nu modifică boți liberi, boții altor
proprietari sau summon-uri temporare. Proprietarul și botul trebuie să fie în afara luptei, iar botul să fie în
viață și să nu fie într-un vehicul sau teleport.

| Preset | Interval principal | Piesele de set |
| --- | --- | --- |
| `blue`, `dungeon`, `preraid` | 187–200 | Rare; relicvele potrivite de ilvl 200 pot fi epice |
| `t7`, `naxx` | 200–213 | T7, cinci piese din același ItemSet |
| `t8`, `ulduar` | 219–232 | T8 (armura originală are ilvl 219/226) |
| `t9`, `toc` | 232–258 | T9, respectând facțiunea proprietarului |
| `t10`, `icc` | 251–277 | T10, cinci piese compatibile |
| `wrathful`, `pvp` | 251–284 | Wrathful; include armele elite, dacă sunt compatibile |
| Număr `1`–`284` | cap minus 15 până la cap | Fără obligația unui ItemSet; capul este strict |

Presetările sunt insensibile la majuscule. Fără parametru (`auto`/`default`),
la nivelul 80 se folosește `blue`, iar sub 80 o limită adaptată nivelului.
Presetările numite Wrath necesită nivelul 80. Clasele personalizate pot folosi
`blue`, selecția după nivel sau capul numeric; nu au preseturi T7–T10/Wrathful.

Selecția folosește catalogul existent, regulile native de echipare, nivelul,
clasa, specializarea și ponderile de statistici ale rolului. Include un
supliment separat pentru arme rare și relicve, respectând lista de obiecte
dezactivate. Relicvele Wrath sunt filtrate pe specializare/rol. Pentru PvP,
armura de set este efectiv Wrathful, celelalte obiecte sunt PvP (inclusiv
Medallion/Battlemaster), iar relicvele pot rămâne PvE dacă acesta este efectul potrivit pentru rol (de exemplu tank).

Sunt respectate arma cu două mâini/offhand, Titan's Grip, obiectele unice și
categoriile de unicitate pentru variantele normal/heroic. Accesoriile care nu
există în interval pot folosi un obiect inferior, fără depășirea capului;
mesajul comenzii raportează numărul acestor sloturi. De exemplu, catalogul
conține Medallion ilvl 264 și Battlemaster ilvl 245 pentru cele două trinketuri
PvP. Trinketurile nu trebuie să aibă intelect/agilitate dacă oferă alte
statistici sau efecte relevante. Nu se promite un set BiS.

Planul și obiectele noi sunt pregătite înainte de dezechipare. Lipsa unui set
complet compatibil produce eroare înainte să fie atinse obiectele existente.
Obiectele vechi sunt returnate prin `_unequip()` în inventar sau prin poștă
când inventarul este plin; cămașa este păstrată. Obiectele noi sunt soulbound,
primesc enchant-urile generatorului și sunt echipate prin `_equip()`, inclusiv
actualizarea blocării, dual wield, proc-urilor, statisticilor și persistenței
în CharacterDatabase. Repetarea comenzii păstrează aceeași selecție la aceleași
date de intrare. Angajarea/spawn-ul obișnuit nu înlocuiește echipamentul.

Ajutorul `.help npcbot autoequip` se poate instala prin migrarea
`sql/custom/world/npcbot_2026_10_24_00_autoequip.sql` în baza world activă.
În această instalare baza este `world2`.

## Verificări

```sh
g++ -std=c++20 -O2 -Wall -Wextra -Werror -pedantic contrib/tests/hazard_avoidance_and_autoequip_test.cpp -o /tmp/hazard-autoequip-test
/tmp/hazard-autoequip-test
cmake --build build --target worldserver --parallel 4
```

Auditul read-only al catalogului `world2` a confirmat cinci sloturi pentru
toate cele 111 ItemSet-uri: 19 T7, 19 T8, 38 T9, 19 T10 și 16 Wrathful,
cu respectarea listei de obiecte dezactivate.

Testul include direct headerele de producție `bothazard.h` și `botautoequip.h`.
Validează 181.704 trasee în 8.000 de configurații aleatoare, cu eșantionare
independentă a segmentelor, plus preseturi invalide, praguri de ilvl, identitatea
seturilor, selecția a cinci piese și conflicte de unicitate. Poate fi compilat
și cu AddressSanitizer/UndefinedBehaviorSanitizer. Aceste teste nu simulează
clientul, serverul de hartă, casturile sau salvarea itemelor în timp real.

Validarea în joc rămâne necesară: Defile în creștere, Legion Flame, slime
suprapus/lângă ziduri, revenirea la DPS/heal după ieșire, tank cu shield,
Fury cu două arme, caster cu staff/offhand, toate clasele și presetările,
inventar plin, repetarea comenzii și relog/restart pentru persistență.
Nu porni worldserver/authserver fără solicitarea explicită a utilizatorului.

## Starea instalării — 24 octombrie 2026

- `worldserver` a fost compilat integral, fără erori sau avertismente.
- Binarul instalat: `/home/trinity/server/bin/worldserver`.
- SHA256 verificat identic cu rezultatul compilării:
  `248e5765690d379fd2cf30210cc257c4aa02d2974954521097c6b72f6a25f02f`.
- Backup: `/root/npcbots-hazard-autoequip-backup-20261024/`; include binarul
  anterior, configurația, manifestul și logul compilării.
- Ajutorul comenzii a fost instalat în `world2.command`. Configurația activă
  nu a fost modificată.
- Testele noi, ASan/UBSan și regresiile RDF/arene/SA/IC/Wintergrasp au trecut.
- `worldserver` și `authserver` au rămas oprite; porturile 8085/3724 nu ascultă.
  Validarea encounter-urilor și a echipării cu un client WoW este în așteptare.
