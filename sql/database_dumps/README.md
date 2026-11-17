# Baze de Date Complete TrinityCore + NPCBots (Fresh Install)

Acest director conține arhivele complete ale bazelor de date pentru o instalare curată (**fresh install**), fără date personale (fără conturi de jucători, parole, e-mailuri sau caractere personale).

## Conținut

1. **`auth.sql.gz`**
   - Structura completă a bazei de date `auth`.
   - Tabelele RBAC (roluri și permisiuni standard TrinityCore).
   - `build_info` pentru versiunea de client 3.3.5a (build 12340).
   - `realmlist` cu realm-ul implicit (Realm 1: "Trinity", 127.0.0.1:8085).
   - Fără conturi înregistrate (este pregătit pentru `account create <user> <pass>` din consola serverului).

2. **`characters.sql.gz`**
   - Structura completă a tabelelor `characters`.
   - Tabelele de sistem și worldstates (inclusiv starea inițială `BOT_GIVER_ENTRY`).
   - `characters_npcbot` cu toate template-urile de boți preconfigurate (`roles` cu flag party activat, `owner = 0` / liberi).
   - Fără caractere de jucători (0 caractere, inventare sau bresle).

3. **`world.sql.gz`**
   - Baza de date completă de world (echivalentul `world2`), gata de importat ca `world`.
   - Conține toate creaturile, quest-urile, scripturile, vrăjile și obiectele personalizate (Companion Hearthstone `70901`, boși custom, waypoint-uri de wanderers, NPCBots extras).

---

## Instrucțiuni de Import (Fresh Install)

Pe serverul de destinație cu MySQL/MariaDB:

```bash
# 1. Creează bazele de date cu charset utf8mb4
mysql -u root -p -e "
CREATE DATABASE IF NOT EXISTS auth DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE DATABASE IF NOT EXISTS characters DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE DATABASE IF NOT EXISTS world DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
"

# 2. Importă arhivele comprimate direct în bazele de date
zcat auth.sql.gz | mysql -u root -p auth
zcat characters.sql.gz | mysql -u root -p characters
zcat world.sql.gz | mysql -u root -p world
```

Serverul este acum gata de pornire cu `authserver` și `worldserver`.
