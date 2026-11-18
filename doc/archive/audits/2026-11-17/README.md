# Audit TrinityCore / NPCBots — 17 noiembrie 2026

**Raport istoric, anterior remedierilor.** Implementarea solicitată ulterior este descrisă în [IMPLEMENTATION.md](../../../audit_2026_11_17/IMPLEMENTATION.md). Patch-ul minim este păstrat doar ca referință; codul integrat conține corecții mai ample.

Sursă verificată: `/root/TrinityCore-3.3.5-with-NPCBots`, `master`, HEAD `ddc4f1521af7`.
Instalare inspectată: `/home/trinity/server`; build: `/home/trinity/build/npcbots`.

Auditul urmărește schimbările recente A–G, fluxurile apelate de ele și testele disponibile. Nu certifică întregul motor și nu înlocuiește testarea cu clienți conectați. Constatările sunt statice dacă nu este indicat explicit un test. Nu am aplicat modificări în codul serverului, configurație ori baze de date și nu am pornit serviciile. Patch-ul alăturat este o propunere, verificată cu `git apply --check`, nu o corecție instalată.

## Rezultat și priorități

| ID | Prioritate | Constatare / localizare la HEAD | Acțiune |
|---|---|---|---|
| H1 | P1 | `bot_ai.cpp:18700`: acces `Player::GetGroup()` prin aliasul `master` al unui Creature liber | Verificare `!IAmFree()` înaintea oricărui acces specific Player |
| H2 | P1 | `bot_ai.cpp:18723`: anulare heal >90% contrazice top-off <95% în cele patru clase | Aceeași politică pentru pornire și anulare; patch minim inclus |
| L1 | P1 | `LootHandler.cpp:370–438,724–752`: transfer quest loot ignoră starea per jucător și compatibilitatea rândurilor | Oprire transfer FFA/condiționat/de grup până la păstrarea drepturilor și contoarelor |
| H3 | P2 | `BuffAndHealGroup`: filtre diferite pentru master, player, bot, pet, vehicul, icon target | Predicat comun înainte de scoring și reverificare înainte de cast |
| H4 | P2 | Scorul recompensează masterul sănătos/tankul sănătos înaintea unor ținte rănite | Nivel de urgență separat de preferința proprietarului; deduplicare |
| H5 | P2 | Wanderer încearcă o singură țintă; comentariul „players first” nu corespunde selecției | Ordonare explicită și fallback la următoarea țintă eligibilă |
| H6 | P2 | `bot_druid_ai.cpp:1271`: Regrowth poate fi reînnoit deși HoT există | Respectare HoT/incoming healing în top-off, limitare încercări eșuate |
| C1 | P2 | Companion: owner persistent scris înainte de spawn/attach, fără rollback pe erori | Rezervare cu rollback, commit la succes |
| C2 | P2 | Heirlooms: marcaj „received” după distribuție potențial parțială; comenzi repetabile | Ledger per item și livrare atomică/pending/mail; separare grant inițial/GM |
| D1 | P2 | `botdynamiczone.cpp`: fallback de spawn poate depăși raza de despawn; timer DK scade doar cu diff-ul ultimului tick | Raze coerente, timer pe timp efectiv scurs |
| I1 | P2 | Installer SQL reaplică toate fișierele alfabetic, fără ledger/checksum | Migrații versionate și ordine explicită a dependențelor |
| T1 | P2 | Trei teste executabile nu mai trec | Actualizare separată a testelor/stub-urilor și a contractului GM |
| H7 | P3 | Prioritate recalculată în comparator; `GetHPSTaken` rescanează grup/casteri pentru fiecare țintă | Snapshot per decizie, apoi optimizarea agregării incoming heals |
| G1 | P3 | Metadata build anterioară indica `c658aee4535a+`, nu HEAD curat | Manifest SHA/config/migrații și build după commit |

P1 = risc de crash, pierdere/incorectitudine loot sau blocarea funcției de bază. P2 = corecție de comportament, consistență ori verificare necesară. Nu am demonstrat un use-after-free prin ASan pe serverul complet.

## 1. Defectele healer-ilor și reproducerea lor

### H1 — `master` nu este întotdeauna un Player

`bot_ai::ResetBotAI`, linia 473, face `master = reinterpret_cast<Player*>(me)` când resetează proprietarul. Verificarea `master != nullptr` nu dovedește că obiectul este un Player. `Player::GetGroup()` citește câmpul `m_group`, inexistent în acel Creature.

Scenariu: wanderer/liber cu rol HEAL, cast negativ cu CastTimeEntry și țintă validă, niciuna dintre condițiile anterioare de anulare adevărată, bot cu cel puțin 60% HP. Prima condiție de protejare a masterului nu se activează; ramura următoare apelează `master->GetGroup()`. Comportament nedefinit, cu posibil crash. Sub 60%, poate întrerupe ofensiva pentru propria viață sub identitatea falsă de „master”.

Corecție imediată în `GlobalUpdate`: adăugare `!IAmFree()` înaintea accesului la master ca Player. Pe termen lung, înlocuire aliasului cu `Unit* GetAnchor()` și `Player* GetOwnerPlayer()`; eliminarea reinterpret_cast necesită auditul tuturor apelanților, nu înlocuirea globală a lui master cu nullptr.

### H2 — bucla heal/anulare este reală în fluxul de cod

Exemplu: player la 94% HP, ambele unități OOC, healer staționar la 15 yd, prag personal 95%, heal direct disponibil.

1. `HealTarget` permite cast-ul prin noua ramură `hp < 95`.
2. `GlobalUpdate` verifică heal-urile directe și întrerupe la `GetHealthPCT(target) > 90`.
3. Excepția existentă protejează doar masterul la distanță < `INTERACTION_DISTANCE`, nu ceilalți playeri și nici masterul aflat mai departe.
4. Întreruperea execută `GC_Timer = 0`. La următoarea decizie poate porni aceeași vrajă.

Există roll-ul de 75%, verificări CastTimeEntry și excepții AoE/tank/icon/order; deci nu este o buclă infinită deterministă pentru orice vrajă. Este un ciclu posibil și repetabil din condițiile contradictorii. Sub ipoteza tick-urilor independente, supraviețuirea a k verificări fără anulare este `0.25^k`. Nu este o măsurătoare de gameplay.

Patch-ul minim păstrează heal-urile OOC pe playeri până la 95% și exclude masterul fals din urgențe. Nu modifică GCD global, mana, costuri, HoT-uri ori politica de combat. Nu rezolvă toate H3–H7. Cerința de top-off trebuie aplicată consecvent și în cele patru `HealTarget`, folosind **atât** `!me->IsInCombat()` cât și `!target->IsInCombat()`.

### H3 — eligibilitate, hărți și pointeri

`std::list::sort` mută nodurile fără invalidarea iteratorilor/referințelor la elemente. Listele sunt locale și nu sunt modificate în timpul iterării de sortare. Nu rezultă de aici un defect de iterator. Pointerii din noduri sunt împrumutați: lista nu păstrează unitățile în viață.

Problemele concrete:

- Ramura solo adaugă masterul fără `IsInWorld`, identitate Map și fază; la pets/vehicule lipsesc unele verificări world/map/alive/phase. Vehiculele pot fi adăugate de mai multe ori prin pasageri diferiți.
- În bucla boților, respingerea botului nu oprește accesul ulterior la pet/vehicul. Pentru un bot aflat în teleport nu ar trebui inspectate subobiectele pentru scoring local.
- Cele patru `HealTarget` verifică target/null/alive/form/distanță, dar nu au un contract uniform pentru world/map/phase/isolated/assist. `GetDistance()` nu este un test de apartenență la hartă.
- Țintele icon pot suprapune ținte deja prezente. Folosiți GUID pentru deduplicare înainte de scoring.
- `UNIT_STATE_ISOLATED` nu este un indicator de durată de viață. CC/silence/GCD ale casterului și imunitățile țintei trebuie tratate de politica de cast, nu prin respingerea tuturor unităților CC.

Un guard minimal pentru **ținte vii**, în execuția sincronă pe harta casterului:

```cpp
// Membră bot_ai propusă. Nu se folosește pentru Corpse/resurrection.
bool bot_ai::CanEvaluateHealTarget(Unit const* target) const
{
    return target && target->IsInWorld() && me->IsInWorld() &&
        target->FindMap() == me->FindMap() && target->IsAlive() &&
        target->InSamePhase(me) &&
        !target->HasUnitState(UNIT_STATE_ISOLATED) &&
        target->GetShapeshiftForm() != FORM_SPIRITOFREDEMPTION &&
        me->IsWithinDistInMap(target, 40.0f) &&
        me->CanSeeOrDetect(target) && me->IsValidAssistTarget(target);
}
```

Un guard nu poate valida un pointer deja dangling: și `IsInWorld()` îl dereferențiază. Premisa este că registrul/map thread garantează existența obiectului pe durata apelului. Pentru cache între tick-uri, task-uri sau callback-uri se stochează GUID și Map/instance context, apoi se rezolvă pe thread-ul hărții prin `ObjectAccessor::GetUnit(*me, guid)` și se verifică din nou. GUID-ul nu autorizează accesul la obiecte din alte fire.

`BotMgr::Update` execută boții angajați în update-ul proprietarului, cu `_update_lock` și removals amânate; nu este mutex între fire. `MapManager::Update` așteaptă workerii înainte de DelayedUpdate. Logout-ul este rezervat `World::UpdateSessions`, iar `WorldSession::LogoutPlayer` scoate boții înaintea distrugerii playerului. Teleport-ul întrerupe cast-urile în `BotMgr::OnTeleportFar`. `Spell::update` folosește `UpdatePointers`, care actualizează țintele. Aceste mecanisme reduc riscul presupus de „logout în mijlocul aceleiași funcții”; nu justifică introducerea de locks în comparator și nici cache de Unit* global.

Nu am identificat o dovadă de ștergere concurentă în listele locale examinate. Un audit complet al callback-urilor, proprietății globale BotDataMgr și al configurațiilor cu mai mulți map workers rămâne separat.

### H4–H5 — scor și wanderers

Scorurile master/player/tank/pet sunt într-un lanț `if/else if`, nu bonusuri cumulative precum descrierea cerinței. Un player tank nu primește +50 dacă a primit +80; masterul nu primește +80 pentru player. Urgența HP este și ea pe benzi exclusive.

Contraexemplu verificabil aritmetic: master la 95% => 5+100=105; bot DPS la 86% =>14. Un master tank la 100%, eligibil prin IsTank, poate avea scor 100 înaintea unui bot la 86%. Un heal nereușit permite fallback, însă un shield/heal reușit pe ținta prioritară poate întârzia rănitul. Prioritatea masterului poate fi intenționată; efectul trebuie decis explicit.

Pentru urgențe folosiți cheie lexicografică `(emergencyTier, missingHealth, ownerPreference)` și bonusuri independente doar în aceeași bandă. Dacă designul cere exact formula din prompt, transformați player/tank/pet în `if` independente și documentați că asta schimbă balansul.

Wanderer-ul selectează strict HP minim, nu „players first”: un bot la 30% bate un player la 90%. Mai grav, dacă ținta minimă nu poate primi heal (LoS/imunitate/prag personal), nu încearcă ceilalți răniți; continuă cu self/buffs. `BuffTargetExclude` elimină summon-uri înainte de filtrul wanderer, deci verificați dacă populația generată folosește această categorie.

Căutarea existentă include alive, isolated, phase, distanță, `IsInBotParty`, detectare și assist; pentru boții liberi `IsInBotParty` acceptă deja playeri mutual friendly. Nu este corectă afirmația că scanarea tuturor playerilor ar fi blocată automat de lipsa grupului. Fallback-ul de rez a fost extins separat pentru wanderers.

## 2. MSPT, complexitate și cache

La H healeri și N ținte, sortarea este `O(H*N log N)` per rundă de decizie, nu obligatoriu per server tick. `BuffAndHealGroup` se oprește la GCD activ, mounted non-wanderer, cast sau feasting; `GlobalUpdate` are propriile timers și gating. N include pets/vehicule/boți, nu doar cei 25–40 playeri.

Pentru N=80 sunt aproximativ N log2 N ≈506 comparații, fiecare evaluând două scoruri. `IsTank(player)` poate consulta roluri/specializare/echipare; `IsPointedHealTarget` parcurge iconurile. Ambele sunt repetate inutil în comparator. `std::list` adaugă alocări și acces dispersat în memorie.

Costul ascuns mai important: fiecare încercare `HealTarget` apelează `GetHPS` → `BotMgr::GetHPSTaken`, care rescanează membri/boți/cast-uri și HoT-uri. Dacă multe ținte sunt necastabile, costul poate ajunge aproximativ `O(H*N*(P+B+A))`, cu A costul auras relevante. Nu există profilare care să dovedească numărul de milisecunde sau că sortarea este bottleneck-ul dominant.

Refactorizare propusă, după filtrare/deduplicare:

```cpp
struct HealCandidate
{
    ObjectGuid guid;
    int32 score;
};
std::vector<HealCandidate> ranked;
ranked.reserve(targets.size());
for (Unit* target : targets)
    if (CanEvaluateHealTarget(target))
        ranked.push_back({target->GetGUID(), GetHealPriority(target)});

std::ranges::sort(ranked, [](auto const& a, auto const& b)
{
    return a.score != b.score ? a.score > b.score : a.guid < b.guid;
});
for (auto const& candidate : ranked)
    if (Unit* target = ObjectAccessor::GetUnit(*me, candidate.guid);
        CanEvaluateHealTarget(target) && HealTarget(target, diff))
        return;
```

`GetHealPriority` trebuie extras ca membră; tipurile din snippet sunt deja ale proiectului. Fragmentul este model de integrare, nu cod compilat în build-ul acestui audit. Dacă trebuie doar o țintă și nu există fallback, `max_element` este O(N). În implementarea actuală fallback-ul există și trebuie păstrat; înlocuirea simplă cu max_element ar produce starvation.

Ordine recomandată de optimizare:

1. Calcul HP/rol/icon o singură dată per candidat per decizie; vector reutilizat per AI, fără pointeri persistenți; GUID tie-break stabil.
2. Scan wanderer OOC la 500–1000 ms, combat 200–250 ms, stagger după GUID; urgențele cu cale separată. Acestea sunt valori propuse, nu măsurate.
3. Agregare incoming heals per hartă/grup într-un snapshot construit o singură dată, cu rezervări pentru cast-urile tocmai începute. Cache-ul pe termen scurt trebuie invalidat la schimbarea grupului/hărții și nu poate întârzia urgențele.
4. Expirare rezervări la spell-go, interrupt, failed cast, moarte/despawn/teleport; nu rețineți rezervarea doar până la timeout.
5. Instrumentați durată p50/p95/p99/max, număr scanări/candidați/comparații/cast fails/cancel reasons, separat de MSPT total. Comparați 1/10/25/40 playeri, cu pets și toți healerii sincronizați. Logging agregat, fără mesaj per tick.

Nu mutați citirea Unit/Group în job-uri paralele pentru optimizare; numai snapshot-uri de valori pot fi procesate în afara map thread-ului.

## 3. OOC, HoT, mana, duel și stealth

- Pragul personal există deja: `_healHpPctThreshold{95u}` în `bot_ai.h`, configurabil prin bot. O valoare personală sub 95 blochează top-off înaintea noii reguli. Limita efectivă trebuie definită ca minimul pragului personal și celui global.
- „Spirit tick prediction” din descriere nu corespunde codului: `GetHPS` este incoming healing calculat din cast-uri și HoT-uri minus DPS urmărit; pentru free bots returnează zero. Nu există în funcția examinată calcul de spirit regeneration. Bypass-ul OOC poate ignora o vindecare reală care urmează să aterizeze.
- `hppctps` întreg pierde valori sub 1%/s, iar proiecțiile folosesc mix unsigned/signed/float și int32. Pentru refactor utilizați `double`/int64 la intermediare, maxHealth verificat și clamp înainte de conversia în tip mic; nu `abs` pe expresii unsigned.
- Pragul `<95` singur nu generează o buclă infinită: heal-ul eficient trece de prag, GCD/cost/cooldown se aplică. Un target cu healing reduction, HP negativ periodic sau cast-uri anulate poate rămâne la 94%, producând retry repetat. Mana disponibilă nu garantează progres.
- Nu există în schimbarea recentă un buget OOC explicit de mana. `CheckBotCast` verifică power cost, `IsSpellReady`/`GC_Timer` și timers limitează cadența; `Feasting` poate amâna heal-ul. GCD nu trebuie eliminat ca să se repare top-off. Alegeți heal eficient, păstrați mana minimă configurabilă și backoff 500–1000 ms la erori persistente de țintă/spell.
- Druid Regrowth are noua alternativă pentru player/master care ocolește și verificarea propriului HoT. Nu suprapune aura la infinit, dar poate reînnoi inutil durata. Renew/Riptide/Rejuvenation au verificări de aură în fluxurile examinate; acestea nu rezolvă competiția între mai mulți healeri.
- Bypass-ul formei ferale pentru rol HEAL este util. `if (IsTank() && xppct > 25) return false` rămâne înainte de el: un bot cu roluri TANK+HEAL poate rămâne blocat. Decideți dacă rolurile sunt exclusive; validați la schimbarea rolurilor.
- Urgența actuală este în roll-ul 75%, după verificarea CastTimeEntry, țintă și alte ramuri; nu este „imediată” pentru toate spell-urile. Este `InterruptSpell(slot)`, nu `InterruptNonMeleeSpells(false)` în acest bloc. Protejează și membri de grup sub 50%, nu numai masterul sub 60%.
- `WorldObject::IsValidAssistTarget` are reguli duel/PvP, dar unele depind de `UNIT_FLAG_PLAYER_CONTROLLED` și `GetAffectingPlayer`. Un wanderer liber poate fi friendly cu ambii dueliști și fără owner; nu presupuneți că verificarea nativă exclude toate cazurile. Funcția bot `InDuel` și unele apeluri sunt comentate.
- Pentru ambient aid, interziceți explicit ținte cu `GetAffectingPlayer()->duel` dacă politica este „nu interveni în dueluri”. Pentru companions decideți separat ajutorul propriului duelist și păstrați restricțiile native. Nu relaxați `IsValidAssistTarget` ca să funcționeze o excepție GM.
- `Spell::prepare` elimină auras cu interrupt-on-cast când spell-ul rupe stealth. `GlobalUpdate` are comentat guard-ul general stealth. Filtrați explicit caster stealth/invisibility pentru ajutor ambient și preferința proprietarului; heal-urile AoE și buff-urile trebuie verificate separat. Nu rezultă că fiecare heal rupe automat stealth-ul țintei.

## 4. AoE loot: drepturi și contabilizare

Localizare reală: `Handlers/LootHandler.cpp`, cu structuri în `Loot.h`/`Loot.cpp`, nu modificarea declarată în `LootMgr.cpp`/Player.cpp. `AOELoot.Range=120`, Enable și MaxCorpses există deja. Raza este căutată în jurul cadavrului principal; ea nu reprezintă automat distanța maximă de la player. Creșterea 40→120 triplează raza și mărește aria de aproximativ 9 ori la densitate uniformă, nu timpul CPU garantat de 9 ori.

Filtrele pentru regular loot sunt conservatoare: nu transferă freeforall, condiții, blocked, follow_loot_rules, roll winner; verifică template stack size, seed, random property și allowedGUIDs la stacking. Acesta este un lucru bun. Limitele de 16 rânduri client și 200 bucăți pentru count uint8 trebuie păstrate/justificate; nu configurați sloturi client arbitrare.

Quest path nu păstrează aceleași garanții:

1. Selectarea verifică global `questItem.is_looted`, nu `NotNormalLootItem::is_looted` pentru player. Pentru FFA, `Loot::LootItemInSlot` și `FillQuestLoot` folosesc stări per jucător.
2. `AddOrStackQuestLoot` unește numai după itemid/count/is_looted; nu compară conditions, freeforall, follow_loot_rules, allowedGUIDs, proprietăți sau eligibilitatea din `PlayerQuestItems`.
3. Stacking într-un rând existent nu garantează că playerul solicitant are rândul în qlist ori că nu l-a consumat deja. Cantitatea poate deveni invizibilă solicitantului după ce a fost scăzută din sursă.
4. După transfer complet, sursa este marcată global `is_looted` și unlootedCount scade cu unu. Pentru FFA rândul poate fi datorat mai multor playeri și contorizat de mai multe ori. Asta poate pierde accesul altor playeri și poate lăsa contoare/flag-uri inconsistente.
5. `HasPendingRoll` tratează orice `is_blocked` ca pending; quest loot poate avea blocked fără un roll real, conform `FillQuestLoot`. Poate dezactiva consolidarea pentru unele cadavre.
6. Funcția nu normalizează toate rândurile duplicate ale cadavrului principal: doar transferă de pe alte cadavre. Cu lista nearby goală revine imediat. Afirmația „duplicate dintr-un singur creature consolidate” nu este demonstrată de această implementare.

Corecție inițială prudentă: transfer quest numai solo, rânduri ne-FFA, necondiționate, fără rules/roll ownership și cu qlist eligibil neconsumat; altfel păstrați sursa. Corecția completă trebuie să păstreze proveniența și consumul per jucător, cu API în Loot pentru mutarea drepturilor; nu doar decrementări manuale de count.

Tests necesare: doi playeri pe aceeași quest și un item multi-drop; unul a lootat deja sursa; qlist existent/absent/consumat pe main; item condiționat identic ca entry; 16 sloturi; transfer parțial; inventar plin; quest need 1 vs count 20; master/group/round-robin; sursa și destinația cu owner diferit. Verificați conservarea cantității și a drepturilor separat. Flag-ul LOOTABLE se elimină doar când contabilizarea nativă spune că loot-ul este epuizat pentru participanții îndreptățiți.

`AOELoot.MaxCorpses` este citit într-un uint32 din config semnat: -1 poate deveni o limită foarte mare, anulând intenția cap-ului. Validați signed înainte de conversie. Raza float trebuie validată cu `isfinite`; comparațiile <5 și >300 nu elimină NaN. Cache configurație la load/reload.

## 5. Living World, city raids și Wintergrasp

**Dynamic zone:** managerul real este `botdynamiczone.cpp`, generatorul în `botdatamgr.cpp`; `botmgr.cpp` gestionează în principal companions. Limita 100 este pentru acest manager, nu totalul global incluzând city raids/BG/WG. La țintă totală pară, ambele facțiuni disponibile și populație completă, cap-urile permit 50/50. În timpul spawn-ului/despawn-ului, la targets impare sau noduri restrictive, nu există garanție permanentă de egalitate exactă. Se preferă numeric facțiunea deficitară, fără tranzacție pereche. Testul dynamic_zone verifică policy header, nu întreaga nouă buclă de facțiuni.

Fallback-ul „closest waypoint” poate selecta noduri până la distanță <50000 chiar când `proximityRadius=400`. La `despawnRadius=900`, un nod ales la 1500 poate genera spawn/despawn la fiecare scanare. Limitați fallback-ul la despawn radius sau nu populați până există noduri utile. `darkKnightRespawnTimer` este decrementat numai după gating-ul scanării, cu diff-ul unui tick, nu cu timpul acumulat: la ticks de 50 ms și scanare la 2s, 300s pot dura aproximativ 40 ori mai mult. Acumulați elapsed scan time ori folosiți deadline monotonic.

**Bones/respawn:** non-player corpse expiră la 30 min în `Corpse::IsExpired`; `_reviveTimer` wanderer este 300000 ms în `JustDied`. Acesta nu este identic cu un SQL respawn timer. Există excepții BG/arena/city waves; `SpawnBotBones` nu generează bones în dungeon. La 100 wanderers care mor la fiecare 5 min, retenția 30 min poate însemna ordinul a 600 skeletons plus alte sisteme; testați curățarea, fără a presupune leak.

**City raid:** roster-ul chiar are 25 defenders cu 5/5/15 pe checkpoints. Nu am găsit în configurațiile acestui script toate capitalele enumerate în prompt: sunt Stormwind și Orgrimmar, nu Cairne/Thunder Bluff. Graveyard-urile sunt poziții configurate în script, nu căutare dinamică a celui mai apropiat graveyard. Defenders sunt readuși pe valuri și `Reinforce()` îi trimite către checkpoint-ul tronului. Testul navmesh real a verificat 32 poziții și 62 trasee pentru cele două orașe.

`CanAIAttack` are early-return pentru victima bossului și focus înainte de raza de 65 yd; acea rază nu este o garanție absolută de leash. O regulă de retreat pentru eveniment nu dovedește un leash al bossului însuși. Testați boss tras din tron, target care fuge, tank schimbat și întoarcere de la graveyard în combat. Leash-ul trebuie definit separat pentru boss și defenders, cu anchor valid și histerezis, fără resetarea encounter-ului la orice pas din formație.

**Wintergrasp:** implementarea și testele acoperă squads, vehicule, navigație, logistical orders, native soldiers și lifecycle; nu este doar un placeholder. Configurația instalată are `Wintergrasp.BotWar.TargetPerTeam=120`, nu 40. Pentru cerința 40v40 ajustați target-ul și limita player/bot în mod coerent; oamenii trebuie să ocupe locuri înaintea boților. Nu am schimbat configurația. Testele offline nu demonstrează un meci complet 120v120 ori 40v40, pathing sub foc, deconectarea șoferului, ownership-ul scaunelor după distrugere ori victorie reală cu clienți.

## 6. QoL, persistență și infrastructură

**Ghost:** `Rate.Ghost.MoveSpeed=3.0` există în dist și instalare. Multiplică viteza deja calculată pentru RUN/SWIM/FLIGHT; cu bonusuri de spirit deja active viteza finală poate fi >3x viteza de bază. „3x” înseamnă +200%, nu +300% (care ar fi 4x). Nu multiplică valoarea salvată la fiecare apel, fiind recalculată în UpdateSpeed. Adăugați clamp finit pozitiv, cache și recalcularea jucătorilor existenți la reload. `BuildPlayerRepop`, rez și aura ghost actualizează vitezele; testați Night Elf, apă, BG și rez după reload.

**AutoLearn:** folosește class trainers și `CanTeachSpell`, respectând cerințe nivel/race/prerequisite; nu învață automat orice spell din DBC. Rulează și la login, nu doar level-up. Are maximum 15 treceri, deci nu există loop nebounded. `hadNew=true` și contorul cresc chiar dacă un cast didactic eșuează; numărul anunțat poate număra încercări, iar lanțuri mai lungi pot rămâne incomplete. Verificați progres real și logați atingerea limitei. Testați downgrade nivel/GM, DK, spell wrappers cu learn effects și lanțuri de rank.

**Heirlooms:** distribuția reală este la login; acordă și personajelor existente fără marker. Dacă inventarul e plin sau template-ul invalid, funcția nu raportează succes complet, dar caller-ul scrie markerul. Inserarea markerului și salvarea inventarului nu formează aceeași tranzacție. Folosiți ledger pending/delivered per item și tranzacție cu save/mail. `.heirlooms` și `.companion heirlooms` pot reface inventarul după vânzare deoarece verifică deținerea curentă, iar ambele folosesc permisiunea NPCBOT_INFO; primul acceptă target. Decideți explicit dacă regrant este GM-only și testați RBAC real; nu am verificat permisiunile din DB live.

**Companion:** cooldown 5h și prima utilizare fără cooldown ulterior sunt implementate: prima înregistrează `first_use_done=1,last_use_time=0`, următoarea reușită pornește cele 5h. SQL queries sunt sincrone la utilizare/login; `ORDER BY RAND()` pe templates nu trebuie să intre în AI tick. Există bypass GM și roluri/gossip native. Owner-ul template-ului este actualizat înainte de LoadBotCreatureFromDB; eșecul load/attach nu are rollback în funcția custom. Păstrați starea anterioară și dezalocați prin API map-owned la eșec; nu faceți delete direct unui Creature adăugat în Map. Persistența cooldown-ului și a primei utilizări trebuie comisă după succes, cu stare în memorie care previne două invocări înaintea query-ului async. „Unlimited GM” are limite tehnice: helperul întoarce 255, există câmpuri de count uint8 și grupuri cu capacitate finită.

**Build:** C++20 cerut efectiv în `ConfigureBaseTargets.cmake`; nu este build C++17. ccache launcher C/C++ activ și max_size 10G confirmate. Limita este bugetul cache, nu memorie RAM. Codul de producție nu a fost modificat de audit. Build-ul sursei curente pentru worldserver și authserver a reușit; nu am instalat binarele rezultate.

**Dumps:** am parcurs complet cele trei gzip-uri, inclusiv verificarea CRC. Nu există INSERT-uri în tabelele account/account_access/characters/character_inventory/mail/guild/character_companion_stone/character_heirlooms_received din arhivele inspectate. Aceasta este verificare a tabelelor enumerate, nu garanție generală că fiecare câmp custom este lipsit de date private. `world` are 295167098 bytes decomprimați și 186 tabele populate. Nu a fost importat într-un MariaDB temporar. Datele binare din `warden_checks` nu trebuie recodate automat ca text UTF-8.

**SQL installer:** `apply_all_custom_sql.sh` există și este tracked, dar folosește `for ... $(ls ... | sort)` și reaplică toate fișierele fără registru. Prefixele `2026_...` și `npcbot_...` nu formează o singură ordine cronologică, iar scripturi de mall conțin DELETE pe intervale de entry/GUID. Nu îl rulați automat pe producție la fiecare deploy. Folosiți manifest cu dependențe, checksum, migrații distincte world/characters/auth, backup verificat și staging import. Creditele DB se transmit prin fișier defaults protejat, nu argument în proces/istoric. DDL în constructorul companion_playerscript trebuie mutat într-o migrație characters.

**Upstream:** origin este trickerer, remote github este fork-ul utilizatorului; branch local master. Upstream public are branch `npcbots_3.3.5`: [repository](https://github.com/trickerer/TrinityCore-3.3.5-with-NPCBots). Am confirmat identitatea proiectului, nu că fork-ul este sincron cu ultimul upstream. Nu am făcut fetch/pull/merge. Pentru sincronizare utilizați merge-base/range-diff pe SHA-uri fixate și rerulați testele custom, în special în Spell/Loot/Group. Patch-urile locale de engine pot avea conflicte semantice chiar fără conflict textual.

## 7. Configurație propusă

Cheile de mai jos marcate „nou” nu sunt implementate; simpla lor adăugare în conf nu schimbă serverul. Păstrați namespace-urile existente:

```ini
# Deja implementate
AOELoot.Enable = 1
AOELoot.Range = 120.0
AOELoot.MaxCorpses = 60
Rate.Ghost.MoveSpeed = 3.0
AutoLearnSpells.Enable = 1
AutoLearnSpells.Announce = 1
CityRaid.DefenderCount = 25
Wintergrasp.BotWar.TargetPerTeam = 40  # propunere pentru cerința 40v40; live este 120
NpcBot.WanderingBots.DynamicZone.MaxBots = 100

# Nou: trebuie implementate în BotCfg/config load, cu validare
NpcBot.Healing.TopOff.Enable = 1
NpcBot.Healing.TopOff.HealthPct = 95
NpcBot.Healing.TopOff.MinManaPct = 25
NpcBot.Healing.TopOff.ScanIntervalMs = 750
NpcBot.Healing.Combat.ScanIntervalMs = 250
NpcBot.Healing.Emergency.OwnerHealthPct = 60
NpcBot.Healing.Emergency.GroupHealthPct = 50
NpcBot.Healing.FailedTargetRetryMs = 750
NpcBot.Wanderer.PlayerAid.Enable = 1
NpcBot.Wanderer.PlayerAid.Range = 40.0
NpcBot.Wanderer.PlayerAid.AllowDuel = 0
NpcBot.Wanderer.PlayerAid.AllowWhileStealthed = 0
NpcBot.Wanderer.BonesLifetimeSeconds = 1800
NpcBot.Wanderer.ReviveDelayMs = 300000
Companion.Stone.CooldownSeconds = 18000
Companion.Heirlooms.Enable = 1
```

Validați signed înainte de cast, procente 0–100, intervale limitate, float finite, distanță ≤ range real spell. Separați pragul personal al botului de plafonul global. Publicați snapshot de configurație la o barieră world/map, fără citiri concurente dintr-o structură mutabilă. Nu citiți ConfigMgr și nu scrieți DB în comparator. Ponderile de scor pot rămâne constante documentate până există o nevoie administrativă reală; prea multe setări interdependente complică balansul.

## 8. Verificare și ordinea remedierilor

Loguri brute: `/tmp/trinity-audit-20261117/` (temporare; rezultatul este păstrat mai jos).

- 37 ținte de test executabile distincte: **34 trecute, 3 eșuate**.
- Cele 11 suite C++ standalone valide: arena_bots, siege_battlegrounds, wintergrasp_bot_war, dungeon_finder_bots, hazard_avoidance_and_autoequip_test, raid_finder_test, dynamic_zone_test, city_raid_tactics, wintergrasp_tactics, wintergrasp_vehicle_cleanup, npcbot_player_limits. Ultima eșuează; celelalte 10 trec. Compilate cu C++20/O2/Wall/Wextra/Werror.
- 26 scripturi Python city_raid_* și wintergrasp_*: 24 trec, 2 eșuează. Unele verifică text/extrag metode în harness-uri cu stub-uri, nu server real.
- `npcbot_player_limits`: assert PersonalLimit(39,1,true)==39; producția întoarce 255 pentru noul bypass GM. Test stale față de intenția actuală; ajustați și documentația conf.
- `city_raid_lifecycle.py`: harness nu are Bot::GetExactDist2d, Config::EventId/CityName, TC_LOG_INFO.
- `city_raid_route_combat.py`: harness nu reflectă CityRaidBotAI/reinforcing, BossEntry, Focus și metodele Unit. Nu este dovadă că producția nu compilează.
- `city_raid_navigation.cpp` trebuie lansat prin `city_raid_navigation.py`, care pregătește include/lib/navmesh. Încercarea inițială standalone fără aceste flags a eșuat cu MMapFactory.h lipsă și nu este numărată drept defect de proiect. Wrapper-ul a trecut.
- `git apply --check doc/archive/audits/2026-11-17/healer_minimal_proposal.patch`: trecut. Patch-ul nu este aplicat și nu a fost compilat ca parte a serverului. Am generat separat `/tmp/trinity-audit-20261117/bot_ai_proposal.cpp` și am verificat întreaga unitate de traducere cu `/usr/bin/c++ -fsyntax-only`, definițiile/include-urile/flag-urile din `game.dir/flags.make` și include-ul directorului NpcBots: **trecut, exit 0**. Log `proposal-syntax.log`; aceasta validează sintaxa/tipurile, nu runtime/link/gameplay.
- Verificare dump gzip: trecut pentru toate cele trei arhive; fără import SQL.
- Conectarea read-only la socket MariaDB a fost refuzată de mediu; schema/datele live și aplicarea migrațiilor nu sunt confirmate. Nu am încercat parole din conf și nu am modificat DB.
- Build `cmake --build /home/trinity/build/npcbots --target worldserver authserver --parallel 2`: **trecut** pentru sursa curentă. Log în `/tmp/trinity-audit-20261117/build.log`. Fără install/deploy.
- Nici worldserver, nici authserver nu erau pornite la inspecție. Fără client/gameplay/MSPT benchmark/sanitizer pe server complet.

Ordine de remediere: H1 și H2; apoi L1; contract comun de ținte și cazuri duel/phase/teleport; HoT și scoring; persistență companions/heirlooms; testele rămase în urmă; abia apoi profilare și cache. Adăugarea unei rezervări map-local de incoming heal și reason codes pentru retry/cancel ar avea valoare mai mare decât extinderea ponderilor magice.

Pentru acceptarea heal overhaul: fiecare clasă, player la 70/84/86/90/91/94/95/100%, master la 3 și 15 yd, alt player, țintă cu HoT/incoming cast, LoS blocat, teleport/logout, fază diferită, duel, mana mică, feral HEAL și HEAL+TANK; verificați cast finalizat, mana consumată și motivul oricărei anulări. Pentru pointer safety sunt necesare și rulari staging ASan/UBSan cu grid unload și map workers, nu doar teste de policy.
