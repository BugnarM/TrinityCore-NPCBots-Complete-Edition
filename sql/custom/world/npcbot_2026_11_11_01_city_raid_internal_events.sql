-- City raids are controlled by their script and explicit GM start/stop commands.
-- Requires GameEventMgr::Update to skip GAMEEVENT_INTERNAL (state 5).
START TRANSACTION;
UPDATE game_event SET world_event=5 WHERE eventEntry IN (86,87);
COMMIT;
