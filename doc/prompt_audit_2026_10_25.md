# Audit și completări pentru prompt.md — 25 octombrie 2026

Raportul din `/root/prompt.md` descria unele funcții drept complet funcționale,
dar codul conținea erori de flux pe care testele inițiale nu le exercitau.
Acest audit verifică cele nouă subsisteme personalizate și corectează problemele
identificate. Nu reprezintă o certificare a întregului TrinityCore sau o
validare de gameplay cu un client conectat.

## Constatări și corecții

| Modul | Verificare / rezultat |
| --- | --- |
| 1. Arene | Revizuite coada, limitele 2/3/5, rezultatul meciului, flag-urile și grafurile pentru cele cinci hărți. Testul de regresie trece; în world2 există exact 11 noduri pe fiecare hartă. Lifturile, flăcările și cascada necesită și testare în joc. |
| 2. SA/IC | Revizuite rutarea Dijkstra și filtrarea muchiilor după porți. Testate combinațiile de porți. Datele active au plafoane 15/40 și grafuri de 27/23 noduri. |
| 3. Vehicule | Corectată căutarea turelelor: scaunul părinte este ocupat chiar de turelă. Adăugată abandonarea vehiculului sub 15% viață în SA/IC și WG; prioritatea jucătorilor umani este păstrată. |
| 4. Wintergrasp | Revizuite distribuția pe trasee, limitele și prioritatea umană. Configurația activă este 120/echipă și există toate cele opt șabloane de soldați. Regresia trece; scenariile de asediu rămân de jucat. |
| 5. RDF | Păstrate rolurile 1/1/3 și rezervările existente. Rezervările de raid au fost separate de cele RDF. Eșecul spawn-ului eliberează echipa parțială și șabloanele neconsumate, în loc de ABORT/ASSERT. Corectat și un pointer rămas după Group::Disband în OnMapChanged. |
| 6. Hazard avoidance | Scanarea rămâne la 250 ms, dar ieșirea este verificată din cache la fiecare tick. La primul tick în afara perimetrului, botul oprește deplasarea de urgență și reia heal/DPS; nu mai așteaptă destinația sau cele 8 secunde. Cache-ul reutilizează memoria pentru scanări. |
| 7. Autoequip | Preflight complet pentru planurile fixe și organice, sintaxă Tail pentru `.npcbot autoequip organic t10`, filtrare explicită a ItemSet-urilor pentru cele 19 profile standard. Piesele pentru priest/shaman/druid healer nu mai pot fi înlocuite cu setul DPS doar datorită statisticilor similare. |
| 8. Organic gear | Corectat slotul ignorat: cămașa este 6, capul este 3. Exact 35% 4p / 35% 2p / 15% 5p / 15% 3p când catalogul permite; off-set-ul nu mai poate completa accidental setul selectat. Implementată efectiv șansa de 10% PvP pe sloturile secundare. Ordine deterministă, identitate stabilă bazată pe șablon/spec/rol, restricții de unicitate și 2H/offhand. |
| 9. Raid Finder | Înlocuit fluxul care forma câte un raid pentru fiecare solicitant și teleporta doar liderul pe continent. Matchmaking real între solicitări, grupuri indivizibile și facțiuni separate; prioritate numărului de jucători umani, apoi locurilor ocupate. Rezervare înainte de reorganizarea grupurilor; dificultate Normal 10/25; teleportarea tuturor oamenilor în instanță și spawn numai după sosire. |
| 10. Dynamic Zone Wanderers | Corectate alegerea nivelului pe nod/clasă, facțiunea, izolarea hartă/zonă, patrularea, limitele și curățarea. Detalii în [dynamic_zone_population.md](dynamic_zone_population.md). Teste offline; integrarea în joc nu este încă validată. |

## Echipare și proprietatea obiectelor

Planificarea echipamentului este separată de aplicare. `InitEquips()` construiește
obiectele și lasă rutina nativă să aplice bonusurile o singură dată; nu mai
apelează recursiv `AutoEquip()`/`SetStats()` în timpul inițializării.

Apelul intern fără receiver este permis numai pentru boți generați temporar.
Înlocuirea trece prin `_unequip()`, care eliberează obiectul anterior. `_updateEquips()`
nu scrie echipamentul summon-urilor sau wanderer-ilor în tabelele de echipament
persistent. Pentru un bot angajat, comanda returnează obiectele prin mecanismul
normal de inventar/poștă. Eșecul planului sau al alocării obiectelor survine înainte
de mutarea obiectelor vechi.

Summon-urile primesc profilul organic în harta destinație: `organic blue` pentru
dungeon Normal, `organic t7` pentru Heroic și profilul corespunzător raidului.
Limita de ilvl din configurația generatorului este respectată. Clasele personalizate
folosesc piese generale, fără bonusurile de tier ale altor clase.

## Raid Finder

```text
.raid queue icc 25 heal
.raid queue naxx 10 tank
.raid queue ulduar 10 dps
.raid status
.raid leave
.raid list
```

Aliasuri: `.raid find`, `.rf ...`, `toc`/`totc`, `ony`/`onyxia`. Fără rol explicit,
rolul este dedus din specializare/prezență/formă. Liderul poate înscrie un grup
existent; membrii trebuie să fie conectați și eligibili. Grupurile se păstrează
întregi la matching, inclusiv la combinarea premade-urilor. Nu sunt transformate
rolurile excedentare în DPS.

Configurație:

```ini
NpcBot.Enable.Raid = 1
NpcBot.RaidFinder.Enable = 1
NpcBot.RaidFinder.WaitTime = 30
NpcBot.RaidFinder.MinItemLevel.Ratio = 1.0
```

În configurația distribuită, Raid Finder este implicit dezactivat. Disponibilitatea
depinde și de activarea NPCBots și a raidurilor. Pragurile de **ilvl mediu** sunt
160 pentru T7, 187 pentru T8, 200 pentru T9 și 219 pentru T10; raportul le poate
ajusta sau dezactiva (0). Acesta nu este scorul addon-ului GearScore.

Sunt reverificate nivelul, echipamentul, starea de combat/teleport/vehicul,
cozile BG/RDF, apartenența grupului, cerințele native de acces, dificultatea și
save-urile permanente. Raid Finder formează raiduri proaspete; nu înscrie un
personaj deja salvat permanent în aceeași dificultate. VoA necesită controlul
Wintergrasp în afara bătăliei. Nu sunt adăugate dificultăți Heroic.

Intrările sunt citite din `areatrigger_teleport` prin `GetMapEntranceTrigger()`;
nu se folosesc coordonate aproximative în afara instanței. Toate cele nouă hărți
au intrări și cerințe pentru 10/25 în world2.

O echipă umană completă poate porni imediat. Altfel, după termen se rezervă doar
complementul lipsă de roluri. Fără șabloane suficiente, solicitarea rămâne în
coadă fără modificarea grupurilor. Schimbarea grupului, ieșirea și logout-ul
anulează solicitarea/rezerva; sosirea are termen de 120 s. Boții sunt generați
după sosirea tuturor oamenilor în aceeași instanță. La eșec nu rămâne o echipă
generată parțial. Boții generați sunt urmăriți și eliminați când proprietarul
părăsește raidul/harta sau când funcția este dezactivată.

Integrarea cu Raid Browser utilizează opcodurile existente, dificultatea din
LFGDungeonEntry și notificarea stării de căutare. Clientul 3.3.5 nu oferă fereastra
LFR din versiunile ulterioare; feedbackul suplimentar este prin chat și pachetele
native de grup. Nu se pretinde implementarea unui astfel de client/UI nou.

## Dynamic Zone Wanderers (Player-Proximity Dynamic World Population)

Implementarea inițială a fost revizuită. Nivelurile sunt alese pentru fiecare punct și clasă, regulile de facțiune sunt obligatorii, iar deplasarea boților dinamici rămâne în zona de origine. S-au adăugat limite globale, spawn gradual și setări validate. Modul manual își păstrează lista de hărți.

Testul vechi folosea o copie simulată a managerului și nu demonstra integrarea reală. A fost înlocuit cu verificări ale politicii de producție. Raportul [dynamic_zone_population.md](dynamic_zone_population.md) descrie configurația, datele observate, testele și limitele verificării offline.

## Verificare

Suita folosește headerele de producție, fără mock-uri ale serverului:

- Arene: cinci grafuri, echipe și stări de victorie.
- SA/IC: toate combinațiile porților și accesul la obiective.
- WG: limite, prioritate umană, trasee și obiective tactice.
- RDF: 12.000 de comparații cu o căutare exhaustivă independentă.
- Hazard: 181.704 trasee în 8.000 de configurații, verificate prin eșantionare.
- Echipare organică: 10.000 de identități, distribuții 35/35/15/15 și 10% PvP,
  stabilitate la reordonarea catalogului, cap/cămașă, 2H și unicitate.
- Raid: 12.000 de comparații exhaustive pentru roluri, termene, dimensiuni
  invalide, aliasuri și un caz în care selecția greedy ar lăsa oameni în coadă.

Comenzi reproductibile:

```sh
for test in arena_bots siege_battlegrounds wintergrasp_bot_war dungeon_finder_bots hazard_avoidance_and_autoequip_test raid_finder_test dynamic_zone_test; do
  g++ -std=c++20 -O2 -Wall -Wextra -Werror contrib/tests/$test.cpp -o /tmp/$test
  /tmp/$test || exit 1
done
```

Testele de politică pot rula cu AddressSanitizer/UndefinedBehaviorSanitizer.
Ele nu validează toate apelurile native de Map/Group/Item sau pachetele clientului;
acestea au fost verificate prin citirea fluxului și compilare, iar gameplay-ul
rămâne de testat după autorizarea pornirii.

## Validare manuală necesară

Testați raidurile 10/25 solo și cu mai multe grupuri, roluri flexibile,
insuficiența șabloanelor, logout/schimbarea liderului în timpul teleportării,
lockout-uri și VoA cu facțiunea greșită. Verificați apartenența în grup, numărul
exact de participanți, despawn-ul, heal/DPS după hazard, echiparea fiecărei clase,
bonusurile efective și inventarul plin. Pentru vehicule verificați turelele și
abandonarea la 15%, fără să întrerupeți un șofer uman.

`worldserver` și `authserver` rămân oprite. Nu au fost făcute încercări live.

## Starea verificată a instalării

- Compilarea completă `worldserver`: reușită.
- Toate cele șase suite standalone au trecut, cu `-Wall -Wextra -Werror`.
- Politicile raid/gear/hazard au trecut și ASan/UBSan (LeakSanitizer dezactivat
  deoarece mediul utilizează ptrace).
- Binarul instalat: `/home/trinity/server/bin/worldserver`; checksum verificat:
  `2e88718c8cb7cbf3353d7167d1b4fb18e96474c477597e257d2c49ab393224f2`.
- Backupul binarului, configurației și ajutorului comenzilor:
  `/home/trinity/source-archives/prompt-audit-20261025-r4td4pad`.
- `NpcBot.RaidFinder.Enable = 1`, timp 30 s și raport ilvl 1.0 în configurația
  activă; ajutorul comenzilor este actualizat în `world2`.
- `worldserver` și `authserver` au rămas oprite; nu există validare în joc.

Pentru o compilare ulterioară fără artefacte în directorul sursă:

```sh
cmake -S /root/TrinityCore-3.3.5-with-NPCBots -B /home/trinity/build/npcbots -DCMAKE_INSTALL_PREFIX=/home/trinity/server -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build /home/trinity/build/npcbots --target worldserver --parallel 4
```
