# Arene cu NPCBots

Implementarea completează echipele 2v2, 3v3 și 5v5 în Nagrand (559), Blade's Edge
(562), Ruins of Lordaeron (572), Dalaran Sewers (617) și Ring of Valor (618).
Sunt acceptate intrarea solo și grupurile incomplete. Dimensiunea cerută de coadă
rămâne obligatorie; boții nu măresc echipa peste limita arenei. Grupurile rămase în
coadă sunt reverificate la cinci secunde, cu anularea automată după invitație sau
ieșirea din coadă.

## Configurare

În `worldserver.conf`:

```ini
NpcBot.Enable = 1
NpcBot.Enable.Arena = 1
NpcBot.WanderingBots.Arena.Enable = 1
NpcBot.WanderingBots.Arena.TargetTeamSize.2v2 = 2
NpcBot.WanderingBots.Arena.TargetTeamSize.3v3 = 3
NpcBot.WanderingBots.Arena.TargetTeamSize.5v5 = 5
```

Valoarea `0` dezactivează completarea unui bracket. Orice altă valoare este
normalizată la 2, 3 sau 5, după bracket. Nu este necesară activarea generării
boților pentru continente sau battleground-uri. Clasele de boți trebuie să aibă
șabloane disponibile în rezerva generatorului. Dacă nu există suficienți boți
pentru ambele echipe, generarea este abandonată fără a introduce o echipă cotată
incompletă în matchmaking.

Coada cotată păstrează verificarea apartenenței jucătorilor la echipa de arenă.
Când există participanți generați, meciul este de antrenament: nu modifică rating,
MMR sau realizări cotate și refuzarea invitației nu penalizează ratingul.
Adversarul sintetic are `ArenaTeamId = 0`; identitatea echipei reale nu este copiată
pe boți. Meciurile între echipe umane complete păstrează comportamentul existent.

## Noduri și deplasare

Migrarea `sql/custom/world/2026_10_24_00_arena_bot_mesh.sql` este tranzacțională și
idempotentă: 11 noduri pe hartă, 55 în total. Șterge exclusiv ID-urile definite în
migrare. Fiecare graf are două puncte de spawn, legături reciproce și noduri de
adăpost marcate prin `BOTWP_FLAG_ARENA_COVER`.

Boții folosesc poziția de start a echipei atribuite de matchmaking. După teleport,
graful de deplasare este selectat pe harta instanței reale, chiar dacă generatorul
a folosit altă hartă pentru pregătirea boților. Steagurile Gold/Green și relațiile
aliat/inamic folosesc echipa meciului, inclusiv pentru pet-uri și meciuri între
jucători din aceeași facțiune. În Ring of Valor, boții aflați încă la baza
ascensoarelor sunt mutați pe podeaua arenei la finalul animației platformelor,
folosind timerul existent al hărții. Nu se bazează pe mișcarea trimisă de un client.

AI-ul selectează o țintă comună pe baza vieții și defensivelor, evită țintele aflate
sub control fără atacatori și încearcă Polymorph, Blind, Fear sau Hammer of Justice
pe healerul advers care nu este ținta principală. Se păstrează verificările normale
ale vrăjilor: cost, cooldown, rază, imunități și vizibilitate. Defensivele disponibile
sunt prioritizate sub 30% viață; Shield Wall necesită scut și stance-ul permis de
vrăjile clasei.

Healerii generați caută adăpost numai când un caster îi atacă. Destinația trebuie
să rupă vizibilitatea adversarului, să păstreze vizibilitatea pacientului și să fie
accesibilă prin navmesh. Vindecarea unui aliat sub 40% viață are prioritate.
Vizibilitatea și obstacolele mobile sunt verificate la rulare; graful SQL singur
nu demonstrează că fiecare poziție este un adăpost valid în fiecare moment.

Un bot eliminat nu se reînvie și nu este teleportat la cimitir. Moartea prin
cădere, pet-uri și mediu declanșează verificarea victoriei după creditarea kill-ului.
Eliminarea simultană a ambelor echipe produce remiză. Verificarea nu încheie arena
în perioada de pregătire.

## Verificare fără pornirea serverului

Din rădăcina repository-ului:

```sh
g++ -std=c++20 -O2 -Wall -Wextra -Werror "$PWD/contrib/tests/arena_bots.cpp" -o /tmp/arena-test
/tmp/arena-test
cmake --build build --target worldserver --parallel 4
```

Testul folosește aceleași funcții de calcul ca serverul pentru completare,
normalizarea configurării, dimensiunile grupurilor, steaguri și victorie.
Validează și grafurile SQL, inclusiv drumurile permise fiecărei facțiuni și
respingerea muchiilor lipsă, izolate sau între hărți. Nu necesită un realm activ.

Verificarea bazei active:

```sql
SELECT mapid, COUNT(*), SUM((flags & 1048576) != 0) AS cover_nodes
FROM creature_template_npcbot_wander_nodes
WHERE id BETWEEN 5400 AND 5490
GROUP BY mapid;
```

Rezultatul așteptat: `559: 11/4`, `562: 11/2`, `572: 11/4`, `617: 11/2`, `618: 11/4`
(noduri totale / adăposturi).

## Validare în joc, numai după autorizarea pornirii

Pentru fiecare hartă și fiecare bracket, verificați solo și grup complet, apoi un
grup incomplet pentru 3v3/5v5. Confirmați numărul de participanți, pozițiile inițiale,
deschiderea porților, steagurile și ostilitatea. Repetați cu echipe umane de aceeași
facțiune și cu boți în echipa câștigătoare după moartea jucătorului. Verificați
refuzarea invitației, ieșirea din coadă, deconectarea, moartea prin cădere, lipsa
reînvierii și absența modificărilor de rating în meciurile de antrenament.

Testarea vizuală a deplasării pe pod, prin țevi și pe ascensoare rămâne necesară.
Serverul trebuie să rămână oprit până la comanda explicită a utilizatorului.
