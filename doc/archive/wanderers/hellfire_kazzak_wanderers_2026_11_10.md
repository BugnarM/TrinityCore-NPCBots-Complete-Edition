# Wandereri de nivel 70 la tronul lui Doom Lord Kazzak

SQL: `sql/custom/world/npcbot_2026_11_10_01_hellfire_kazzak_wanderers.sql`, ID-uri 61000–61023. Sunt 24 de noduri pe platoul Throne of Kil'jaeden, hartă 530, zonă Hellfire Peninsula 3483, arie 3547. Toate au nivel minim/maxim 70.

Cele șapte puncte de spawn sunt la peste 90 m de boss. Patru sunt pentru Alliance și trei pentru Horde, în sectoare separate. Flagul MOVEMENT_IGNORES_FACTION permite ambelor echipe să parcurgă întreaga rețea după spawn. Astfel, generarea nu mai plasează ambele facțiuni în aceeași tabără. Celelalte 17 noduri sunt de tranzit și au NOT_A_START_POINT: botii se deplasează pe jos spre tron, fără spawn sau teleportare pe boss.

Rețeaua are 30 de legături bidirecționale și șapte circuite conectate, cu ponderi crescute spre tron. Nodul 61021 se află la poziția bossului (943.49, 2269.77), proiectată pe suprafața navigabilă, Z 307.651. Nodurile 61020 și 61022 sunt puncte de acces pe platformă, cu vizibilitate directă spre Kazzak verificată pe vmaps. Nodul 61023 leagă coborârea de patrulele de pe platou. Nu există legături spre vechile drumuri Hellfire de nivel 60–63 de sub platou.

Wandererii păstrează numele, clasele, echipamentul și AI-ul nativ NPCBots. Kazzak păstrează puterea, vrăjile, respawn-ul și AI-ul său. Drumul până pe platformă permite contactul și aggro-ul; deplasarea bossului după țintă, kiting-ul și revenirea la poziția inițială rămân native. Nu este forțată teleportarea bossului sau o victorie a botilor.

Este folosită populația DynamicZone existentă: până la 100 de boți pentru zona ocupată, rază de spawn 250 m, buget global 25 per scanare de două secunde și limită globală 2.000. Populația se activează când un jucător este prezent. Numărul efectiv și facțiunile depind de punctele disponibile, jucători și pool-ul liber.

Validare: `python3 contrib/tests/hellfire_kazzak_navigation.py` verifică graful SQL, toate cele 60 de segmente direcționate pe mmaps instalate, accesul pe platforma bossului și vizibilitatea pe vmaps. Toate verificările au trecut. Datele sunt activate prin repornirea controlată a executabilului existent.

Preferință confirmată: PvP-ul între facțiuni și patrularea normală rămân active. Nu este introdus un mod PvE sau o prioritate forțată pentru Kazzak. Lupta poate întrerupe traseul spre tron.

Activare verificată: proces worldserver 198194, același executabil, 5.331 noduri încărcate. Logul confirmă generarea a 100 de wandereri la nivelul 70 pe noile puncte.
