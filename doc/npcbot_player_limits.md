# Companioni personali pentru jucători și grupuri Finder

Profil instalat la 26 octombrie 2026:

```ini
NpcBot.MaxBots.NonGM = 1
NpcBot.DungeonBots.Enable = 1
NpcBot.DungeonBots.Queue.WaitTime = 1
NpcBot.DungeonBots.Queue.RandomOnly = 0
```

Conturile cu securitate sub `SEC_GAMEMASTER` pot recruta și controla cel mult un
companion personal. Limita este verificată la recrutare, în meniul recrutorului,
la preluarea unui bot și la reatașarea unui bot deja deținut sau partajat. Conturile
GM păstrează limita pe nivel din `NpcBot.MaxBots`, indiferent de activarea modului
`.gm on/off`. Restricțiile și costul obișnuit de recrutare rămân aplicabile.

Boții generați temporar pentru Dungeon Finder nu consumă slotul de companion
personal. Grupul normal sau eroic rămâne de cinci membri, cu un tank, un healer
și trei DPS: un jucător singur primește patru boți; un jucător cu un companion
personal inclus în coadă primește încă trei boți, dacă rolurile sunt compatibile.
Mai mulți jucători înseamnă proporțional mai puține locuri completate cu boți.

Companionul personal se recrutează prin serviciul NPC existent; această schimbare
nu alege automat o clasă și nu oferă automat un bot la autentificare. Proprietatea
și echipamentul boților vechi nu sunt șterse. Dacă un cont obișnuit avea deja mai
mulți companioni, doar unul poate fi reatașat simultan. Boții generați pentru
instanță rămân în instanță când jucătorul se teleportează afară; curățarea lor la
încheierea instanței/ieșirea din grup folosește mecanismul Finder existent.

Verificări:

```sh
c++ -std=c++20 -O2 -Wall -Wextra -Werror contrib/tests/npcbot_player_limits.cpp -o /tmp/npcbot-player-limits
/tmp/npcbot-player-limits
c++ -std=c++20 -O2 -Wall -Wextra -Werror contrib/tests/dungeon_finder_bots.cpp -o /tmp/npcbot-rdf-limits
/tmp/npcbot-rdf-limits
```

Testul politicii folosește funcțiile de producție pentru limita personală și
compoziția Finder: primul și al doilea companion, boți deținuți anterior,
exceptarea GM și a boților generați, compoziție de patru/trei completări și roluri
incompatibile. Compilarea completă verifică integrarea în server. Autentificarea
unui cont normal, recrutarea și teleportarea într-un dungeon trebuie validate
și în client; testele offline nu simulează acești pași.

Rezultat instalare: cele două suite au trecut, iar compilarea completă
`worldserver` a reușit fără avertismente. Log: `/tmp/npcbot-player-limit-build.log`.
Binarul actualizat este instalat în `/home/trinity/server/bin/worldserver`.
Serverul era oprit și a fost păstrat oprit; politica nouă intră în funcțiune
la următoarea pornire.
