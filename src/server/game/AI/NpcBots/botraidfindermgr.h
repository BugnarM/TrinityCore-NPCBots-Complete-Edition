#ifndef NPCBOT_RAID_FINDER_MGR_H
#define NPCBOT_RAID_FINDER_MGR_H
#include "botraidfinder.h"
#include "ObjectGuid.h"
#include <map>
class Player;
class Group;
namespace NPCBots::RaidFinder
{
struct Member { ObjectGuid guid; uint8_t roles; };
struct QueueEntry
{
    ObjectGuid leader, group;
    uint32_t raidId, team;
    uint8_t size;
    int64_t joined;
    std::vector<Member> members;
};
struct PendingRaid
{
    ObjectGuid leader, group;
    uint32_t raidId;
    uint8_t size;
    int64_t expires;
    std::vector<ObjectGuid> humans;
};
struct ActiveRaid { ObjectGuid leader; uint32_t map; std::vector<ObjectGuid> bots; };
class RaidFinderMgr
{
public:
    static RaidFinderMgr* Instance();
    bool Queue(Player* player, uint32_t raidId, uint8_t size, uint8_t roles);
    bool Leave(Player* player);
    bool IsQueued(Player const* player) const;
    bool GetStatus(Player const* player, std::string& name, uint8_t& size, uint32_t& elapsed) const;
    static uint8_t PlayerRole(Player const* player);
    void OnLogout(ObjectGuid guid);
    void OnGroupChanged(ObjectGuid group);
    void Update(uint32_t diff);
private:
    bool Validate(Player* player, RaidDefinition const& raid, uint8_t size, bool report) const;
    bool SnapshotValid(QueueEntry const& entry) const;
    bool FormRaid(std::vector<QueueEntry> const& entries, std::vector<uint8_t> const& roles);
    void CancelPending(ObjectGuid group);
    std::map<ObjectGuid, QueueEntry> _queue;
    std::map<ObjectGuid, PendingRaid> _pending;
    std::map<ObjectGuid, ActiveRaid> _active;
    uint32_t _updateTimer{};
};
}
#endif
