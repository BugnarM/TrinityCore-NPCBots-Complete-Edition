# Wintergrasp: recuperare, PvP, asediu și statistici — 2026-11-08

Continuă corecțiile din `wintergrasp_aggression_2026_11_08.md` și `wintergrasp_realism_2026_11_08.md`.

## Vehicule recuperate

Moartea unui soldat folosește acum o cale de retragere din roster separată de reducerea populației. Vehiculul său viu rămâne pe hartă, înregistrat în capacitatea nativă. Un membru supraviețuitor al grupei poate ocupa volanul, iar liderul revenit după respawn își poate recupera vehiculul deplasându-se până la acesta. Nu este necesară reconstruirea unui vehicul încă funcțional.

La reducerea rosterului sunt păstrate vehiculele vii cu pasager uman sau șofer bot. Vehiculele rămase în registrul de recuperare sunt oferite unor membri de echipaj disponibili la maximum 250 m, excluzând gărzile turnurilor intacte și constructorii ocupați. Vehiculele fără șofer rămân oprite. Curățarea nativă de la sfârșitul bătăliei se păstrează. Distrugerea vehiculului și înlocuirea șoferului sunt contoare distincte. Construcțiile și distrugerile sunt numărate în registrul nativ, inclusiv pentru vehicule construite sau conduse de jucători. Despawn-ul unui vehicul viu la curățare nu este înregistrat ca distrugere în luptă.

## Infanterie și blocaje

Infanteria folosește același navigator cu prefixe verificate, coliziune dinamică, detururi laterale și retragere folosit pentru vehicule. Dimensiunile diferă: soldații folosesc raza proprie, de minimum 0,25 m, iar vehiculele păstrează minimum 0,75 m. Un traseu disponibil doar pentru infanterie nu este declarat automat disponibil și unui blindat.

Navigatorul verifică progresul la aproximativ o secundă, detectează o staționare de minimum patru secunde și încearcă alt traseu. Reîncercările costisitoare sunt limitate la trei secunde. Nu există fallback prin ziduri sau teleportare. Root/stun și oprirea impusă de cast nu sunt numărate ca blocaje de traseu. Contoarele disting episoadele de blocare de recuperarea efectivă prin deplasare. Călătoria la inginer și la vehicul folosește și ea navigație verificată.

## Luptă coordonată

Scanarea țintelor vizitează grila o singură dată. Scorul ține cont de distanță, viață, healer, cast în curs, amenințarea asupra unui healer aliat, vehiculele de asediu și ținta recentă a aliaților din același culoar. O marjă de 25 de puncte evită schimbarea continuă între două ținte aproape echivalente. Țintele de dincolo de o barieră intactă sunt excluse.

Pummel, Counterspell, Wind Shear, Kick și Silencing Shot au timere proprii. O întrerupere lansată cu succes rezervă ținta timp de 1,5 secunde; controlul o rezervă trei secunde. O lansare eșuată nu ocupă rezervarea. Registrele expiră și sunt resetate la schimbarea bătăliei. Țintele aflate sub fear/confuse nu primesc noi atacuri directe, iar atacurile AoE relevante și controlul AoE evită adversarii aflați deja sub control întrerupt de daune.

Aceste reguli coordonează deciziile boților; rezistențele, imunitățile, GCD-ul și efectele spell-urilor rămân native. Contoarele `interrupt_casts` și `control_casts` măsoară lansări acceptate de motor, nu garantează că un efect a trecut de imunitate sau rezistență.

## Opt profiluri, aceeași populație

Au fost adăugate Holy Paladin, Restoration Shaman, Rogue și Warlock la Warrior, Frost Mage, Holy Priest și Marksman. Fiecare grupă păstrează doi melee, un healer și doi ranged. Repartizarea este deterministă și identică pentru ambele facțiuni.

Se refolosesc cele opt template-uri existente ca surse de statistici, prin alegerea profilului în C++. Nu sunt necesare template-uri sau migrări SQL noi. Paladinul și shamanul au vindecări proprii; Rogue folosește energie, două daggere și Kick/Dismantle; Warlock folosește Shadow Bolt, Corruption și Death Coil. Mana și energia au regenerarea nativă activată. Profilele rămân rotații NPC simplificate, fără întregul sistem de talente, combo points, pet-uri și echipament al unui jucător.

Catalogul de spell-uri primare, de vindecare și suport a fost verificat și în `/home/trinity/server/data/dbc/Spell.dbc`; daggerul 40386 a fost verificat în `Item.dbc`.

## Asediu între culoare

La fiecare decizie tactică sunt comparate poarta centrală și zidurile vest/est. Scorul folosește starea de distrugere, procentul real de viață și apărătorii din apropiere. O breșă deja deschisă este preferată; atelierele nordice trebuie controlate pentru trimiterea unui convoi pe flanc. O marjă de 25 de puncte stabilizează alegerea culoarului.

Pot fi concentrate cel mult trei convoaie de demolishere/Siege Engine cu escortele lor. Catapultele, vehiculele ocupate de oameni, grupele desemnate capturării și vehiculele deja în interiorul cetății sunt excluse. Redirecționarea nu resetează armata la staging: alege următorul checkpoint relevant și verifică fiecare atelier și barieră. Un zid neîncărcat rămâne blocant. Infanteria aflată în luptă aplică ordinul după terminarea luptei.

## Statistici persistente

Evenimentele provin din callback-urile native pentru începutul/sfârșitul bătăliei, schimbarea atelierului și distrugerea clădirilor. Astfel sunt înregistrate și capturile sau victoriile realizate de jucători.

- `event=progress`: situație periodică pe facțiune, vehicule construite/pierdute/recuperate, capturi, blocaje, recuperări și lansări de întrerupere/control.
- `event=capture`: facțiune, atelier și momentul capturării.
- `event=breach`: clădire și momentul distrugerii.
- `event=battle_end`: atacator, câștigătorul nativ, durată, momentul activării armatelor, prima captură/breșă și contoarele finale.

Bătăliile în care armatele nu au fost activate de un participant real sunt excluse din ratele victoriilor. Totalurile în memorie se păstrează între bătălii în același proces. Fișierul dedicat `WintergraspBattle.log` este deschis în modul append, pentru analiză și după reporniri. Logger: `bg.battlefield.botwar`.

Configurația instalată are `LogsDir = ""`; cu scriptul curent de pornire, fișierul este `/home/trinity/server/bin/WintergraspBattle.log`. Va fi creat la pornirea noii versiuni. Export:

```bash
python3 /root/TrinityCore-3.3.5-with-NPCBots/contrib/tools/wintergrasp_battle_report.py \
  /home/trinity/server/bin/WintergraspBattle.log \
  --json /tmp/wintergrasp-report.json --csv /tmp/wintergrasp-battles.csv
```

Raportul separă rezultatele după facțiunea atacatoare, calculează rata victoriilor și durata medie și exportă înregistrările complete. Lipsa bătăliilor produce o rată `null`, nu o victorie inventată. Ratele reflectă rezultatele native; bătăliile închise manual de GM trebuie analizate separat de meciurile normale.

## Verificare și limite

Build complet `worldserver` în `/home/trinity/build/npcbots`; logul final este `/tmp/wg-upgrades-20261108/build-release-final.log`.

Cincisprezece suite automate sunt înregistrate în `/tmp/wg-upgrades-20261108/tests.log`: cele nouă suite existente, plus profiluri/politici tactice, catalogul DBC, rezervările PvP între mai mulți boți, redirecționarea efectivă a traseelor de asediu, exportul rapoartelor și contabilizarea nativă a vehiculelor. Testele de logistică au fost extinse cu păstrarea vehiculului după moarte, resursele claselor și coordonarea convoaielor. Navigatorul este testat și pentru dimensiuni de infanterie și spații înguste.

Nu a fost rulată o bătălie în client. Testele cu dependințe simulate verifică mecanismele, nu toate rampele/porțile din datele reale și nici performanța observată la 240 de participanți. Nu există o rată a victoriilor măsurată pentru noua versiune. Pașii următori în joc sunt recuperarea unui vehicul după uciderea șoferului, ocolirea unui blocaj, observarea fiecărui profil, traversarea unei breșe alternative și compararea unor bătălii cu facțiunile inversate.
