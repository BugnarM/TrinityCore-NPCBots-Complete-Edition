#include "botraidfindermgr.h"
#include "Battlefield.h"
#include "BattlefieldMgr.h"
#include "bot_ai.h"
#include "botconfig.h"
#include "botdatamgr.h"
#include "botmgr.h"
#include "Chat.h"
#include "CommonHelpers.h"
#include "DBCStores.h"
#include "GameTime.h"
#include "Group.h"
#include "GroupMgr.h"
#include "Item.h"
#include "LFGMgr.h"
#include "Map.h"
#include "MapManager.h"
#include "ObjectAccessor.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "WorldSession.h"
#include <numeric>

namespace NPCBots::RaidFinder
{
RaidFinderMgr* RaidFinderMgr::Instance() { static RaidFinderMgr instance; return &instance; }

uint8_t RaidFinderMgr::PlayerRole(Player const* player)
{
    if (!player) return 0;
    uint8 spec = Bcore::Helpers::Entity::GetPlayerSpecialization(player);
    switch (player->GetClass())
    {
        case CLASS_WARRIOR: return spec == SPEC_WARRIOR_PROTECTION ? Tank : Damage;
        case CLASS_PALADIN: return spec == SPEC_PALADIN_PROTECTION ? Tank : spec == SPEC_PALADIN_HOLY ? Healer : Damage;
        case CLASS_PRIEST: return spec == SPEC_PRIEST_SHADOW ? Damage : Healer;
        case CLASS_SHAMAN: return spec == SPEC_SHAMAN_RESTORATION ? Healer : Damage;
        case CLASS_DRUID:
            return spec == SPEC_DRUID_RESTORATION ? Healer :
                (player->GetShapeshiftForm() == FORM_BEAR || player->GetShapeshiftForm() == FORM_DIREBEAR) ? Tank : Damage;
        case CLASS_DEATH_KNIGHT: return player->HasAura(48263) ? Tank : Damage;
        default: return Damage;
    }
}

bool RaidFinderMgr::Validate(Player* player, RaidDefinition const& raid, uint8_t size, bool report) const
{
    auto fail = [&](char const* message) {
        if (report && player && player->GetSession()) ChatHandler(player->GetSession()).PSendSysMessage("[Raid Finder] %s", message);
        return false;
    };
    if (!player || !player->GetSession() || !player->IsInWorld()) return false;
    if (!BotCfg::IsNpcBotRaidFinderEnabled()) return fail("Raid Finder is disabled in the server configuration.");
    if (!ValidSize(size) || player->GetLevel() < raid.minLevel) return fail("Select a 10/25-player raid; level 80 is required.");
    if (!player->IsAlive() || player->IsInCombat() || player->IsBeingTeleported() || player->IsInFlight() ||
        player->GetVehicle() || player->GetTransport() || player->InBattleground() || player->InBattlegroundQueue())
        return fail("Leave combat, transport, battleground queues and vehicles before queueing.");
    if (player->GetMap()->IsDungeon()) return fail("Leave your current instance before queueing for another raid.");
    if (Group* group = player->GetGroup(); group && (group->isLFGGroup() || group->isBGGroup() || group->isBFGroup()))
        return fail("Leave your dungeon/battleground group before queueing for a raid.");
    auto state = sLFGMgr->GetState(player->GetGUID());
    if (state != lfg::LFG_STATE_NONE && state != lfg::LFG_STATE_FINISHED_DUNGEON)
        return fail("Leave Dungeon Finder before queueing for a raid.");
    Difficulty difficulty = size == 25 ? RAID_DIFFICULTY_25MAN_NORMAL : RAID_DIFFICULTY_10MAN_NORMAL;
    if (!GetMapDifficultyData(raid.mapId, difficulty) || !BotCfg::IsMapAllowedForBots(sMapMgr->CreateBaseMap(raid.mapId)))
        return fail("The selected raid/difficulty is not enabled for bots.");
    if (raid.mapId == 624)
    {
        Battlefield* wintergrasp = sBattlefieldMgr->GetBattlefieldByBattleId(BATTLEFIELD_BATTLEID_WG);
        if (!wintergrasp || !wintergrasp->IsEnabled() || wintergrasp->IsWarTime() || wintergrasp->GetDefenderTeam() != player->GetTeamId())
            return fail("Vault of Archavon requires control of Wintergrasp outside battle time.");
    }
    auto const* entrance = sObjectMgr->GetMapEntranceTrigger(raid.mapId);
    if (!entrance || entrance->target_mapId != raid.mapId) return fail("The instance entrance is missing from the world database.");
    if (!player->Satisfy(sObjectMgr->GetAccessRequirement(raid.mapId, difficulty), raid.mapId, report)) return false;
    if (auto const* bind = player->GetBoundInstance(raid.mapId, difficulty); bind && bind->perm)
        return fail("This character already has a permanent save for this raid/difficulty.");
    if (player->GetAverageItemLevel() < float(raid.minItemLevel) * BotCfg::GetRaidFinderItemLevelRatio())
        return fail("Your average item level is below the minimum shown by .raid list.");
    return true;
}

bool RaidFinderMgr::Queue(Player* player, uint32_t raidId, uint8_t size, uint8_t roles)
{
    auto const* raid = FindRaidById(raidId);
    if (!raid || !Validate(player, *raid, size, true)) return false;
    if (player->GetGroup() && player->GetGroup()->GetLeaderGUID() != player->GetGUID())
    {
        ChatHandler(player->GetSession()).SendSysMessage("[Raid Finder] Only the group leader may queue a premade group.");
        return false;
    }
    QueueEntry entry{player->GetGUID(), player->GetGroup() ? player->GetGroup()->GetGUID() : ObjectGuid::Empty,
        raidId, player->GetTeam(), size, GameTime::GetGameTime(), {}};
    auto addHuman = [&](Player* member, uint8_t mask) {
        if (!member || !Validate(member, *raid, size, member == player) || member->GetTeam() != player->GetTeam() || IsQueued(member)) return false;
        uint8_t viable = BotDataMgr::BotToLFGRoles(BotDataMgr::GetViableRolesForClass(member->GetClass()), false);
        mask &= viable & (Tank | Healer | Damage);
        if (!mask) return false;
        entry.members.push_back({member->GetGUID(), mask});
        return true;
    };
    if (Group* group = player->GetGroup())
    {
        for (auto const& slot : group->GetMemberSlots())
        {
            if (!slot.guid.IsPlayer()) continue;
            Player* member = ObjectAccessor::FindConnectedPlayer(slot.guid);
            uint8_t selected = slot.guid == player->GetGUID() ? roles : slot.roles;
            if (!(selected & (Tank | Healer | Damage))) selected = PlayerRole(member);
            if (!addHuman(member, selected)) return false;
        }
        for (auto* ref = group->GetFirstBotMember(); ref; ref = ref->next())
        {
            Creature* bot = ref->GetSource();
            if (!bot || !bot->IsAlive() || !bot->GetBotAI() || bot->IsSummon() || bot->IsInCombat()) return false;
            entry.members.push_back({bot->GetGUID(), BotDataMgr::BotToLFGRoles(bot->GetBotRoles())});
        }
        if (entry.members.size() != group->GetMembersCount()) return false;
    }
    else if (!addHuman(player, roles)) return false;
    std::vector<uint8_t> masks;
    for (auto const& member : entry.members) masks.push_back(member.roles);
    if (!AssignRoles(masks, size))
    {
        ChatHandler(player->GetSession()).SendSysMessage("[Raid Finder] Group roles exceed this raid's capacity. Specify tank, heal or dps after the raid size.");
        return false;
    }
    _queue.emplace(entry.leader, entry);
    for (auto const& member : entry.members)
        if (Player* human = ObjectAccessor::FindConnectedPlayer(member.guid))
        {
            human->GetSession()->SendLfgLfrList(true);
            ChatHandler(human->GetSession()).PSendSysMessage("|cff00ff00[Raid Finder]|r Queued for %s (%u-player). Bot fill after %u seconds.",
                raid->name.c_str(), uint32_t(size), BotCfg::GetRaidFinderWaitSeconds());
        }
    return true;
}

bool RaidFinderMgr::IsQueued(Player const* player) const
{
    if (!player) return false;
    for (auto const& [_, entry] : _queue)
        for (auto const& member : entry.members) if (member.guid == player->GetGUID()) return true;
    for (auto const& [_, pending] : _pending)
        if (std::find(pending.humans.begin(), pending.humans.end(), player->GetGUID()) != pending.humans.end()) return true;
    return false;
}

bool RaidFinderMgr::GetStatus(Player const* player, std::string& name, uint8_t& size, uint32_t& elapsed) const
{
    if (!player) return false;
    for (auto const& [_, entry] : _queue)
        for (auto const& member : entry.members)
            if (member.guid == player->GetGUID())
            {
                auto const* raid = FindRaidById(entry.raidId);
                name = raid ? raid->name : "unknown";
                size = entry.size;
                elapsed = uint32_t(std::max<int64_t>(0, GameTime::GetGameTime() - entry.joined));
                return true;
            }
    return false;
}

bool RaidFinderMgr::Leave(Player* player)
{
    if (!IsQueued(player)) return false;
    OnLogout(player->GetGUID());
    return true;
}

void RaidFinderMgr::CancelPending(ObjectGuid group)
{
    auto it = _pending.find(group);
    if (it == _pending.end()) return;
    BotDataMgr::ReleaseRaidBots(it->second.leader);
    _pending.erase(it);
}

void RaidFinderMgr::OnLogout(ObjectGuid guid)
{
    for (auto it = _queue.begin(); it != _queue.end();)
    {
        bool found = std::any_of(it->second.members.begin(), it->second.members.end(), [guid](Member const& m) { return m.guid == guid; });
        if (!found) { ++it; continue; }
        auto entry = std::move(it->second);
        it = _queue.erase(it);
        for (auto const& member : entry.members)
            if (Player* human = ObjectAccessor::FindConnectedPlayer(member.guid))
            {
                human->GetSession()->SendLfgLfrList(false);
                ChatHandler(human->GetSession()).SendSysMessage("[Raid Finder] Queue cancelled because a member left or the group changed.");
            }
    }
    std::vector<ObjectGuid> cancel;
    for (auto const& [group, pending] : _pending)
        if (std::find(pending.humans.begin(), pending.humans.end(), guid) != pending.humans.end()) cancel.push_back(group);
    for (ObjectGuid group : cancel) CancelPending(group);
}

void RaidFinderMgr::OnGroupChanged(ObjectGuid group)
{
    std::vector<ObjectGuid> leaders;
    for (auto const& [leader, entry] : _queue) if (entry.group == group) leaders.push_back(leader);
    for (ObjectGuid leader : leaders) OnLogout(leader);
    CancelPending(group);
}

bool RaidFinderMgr::SnapshotValid(QueueEntry const& entry) const
{
    Player* leader = ObjectAccessor::FindConnectedPlayer(entry.leader);
    auto const* raid = FindRaidById(entry.raidId);
    if (!leader || !raid) return false;
    Group* group = leader->GetGroup();
    if ((group ? group->GetGUID() : ObjectGuid::Empty) != entry.group ||
        (group && (group->GetLeaderGUID() != entry.leader || group->GetMembersCount() != entry.members.size()))) return false;
    for (auto const& member : entry.members)
    {
        if (member.guid.IsPlayer())
        {
            Player* player = ObjectAccessor::FindConnectedPlayer(member.guid);
            if (!Validate(player, *raid, entry.size, false) || player->GetGroup() != group) return false;
        }
        else
        {
            Creature const* bot = BotDataMgr::FindBot(member.guid.GetEntry());
            if (!bot || !bot->IsAlive() || bot->IsInCombat() || bot->GetBotGroup() != group) return false;
        }
    }
    return true;
}

bool RaidFinderMgr::FormRaid(std::vector<QueueEntry> const& entries, std::vector<uint8_t> const& roles)
{
    if (entries.empty()) return false;
    auto const& seed = entries.front();
    auto const* raid = FindRaidById(seed.raidId);
    auto missing = MissingRoles(roles, seed.size);
    if (!raid || !missing) return false;
    for (auto const& entry : entries) if (!SnapshotValid(entry)) return false;
    Player* leader = ObjectAccessor::FindConnectedPlayer(seed.leader);
    uint32_t count = std::accumulate(missing->begin(), missing->end(), 0u);
    if (count && !BotDataMgr::ReserveRaidBots(leader, *missing, seed.size)) return false;
    Group* group = leader->GetGroup();
    if (!group)
    {
        group = new Group;
        if (!group->Create(leader)) { delete group; BotDataMgr::ReleaseRaidBots(seed.leader); return false; }
        sGroupMgr->AddGroup(group);
    }
    // All preflight checks and the complete template reservation succeeded.
    // Remove queue records before native group callbacks run while merging premades.
    for (auto const& entry : entries) _queue.erase(entry.leader);
    if (!group->isRaidGroup()) group->ConvertToRaid();
    Difficulty difficulty = seed.size == 25 ? RAID_DIFFICULTY_25MAN_NORMAL : RAID_DIFFICULTY_10MAN_NORMAL;
    group->SetRaidDifficultyID(difficulty);
    std::vector<Member> members;
    for (auto const& entry : entries) members.insert(members.end(), entry.members.begin(), entry.members.end());
    std::vector<Creature*> bots;
    for (auto const& member : members)
        if (!member.guid.IsPlayer())
            if (Creature const* found = BotDataMgr::FindBot(member.guid.GetEntry())) bots.push_back(const_cast<Creature*>(found));
    for (Creature* bot : bots)
        if (bot->GetBotGroup() && bot->GetBotGroup() != group) bot->GetBotOwner()->GetBotMgr()->RemoveBotFromGroup(bot);
    for (auto const& member : members)
    {
        if (!member.guid.IsPlayer()) continue;
        Player* player = ObjectAccessor::FindConnectedPlayer(member.guid);
        if (player->GetGroup() != group)
        {
            if (player->GetGroup()) player->RemoveFromGroup();
            if (!group->AddMember(player)) { BotDataMgr::ReleaseRaidBots(seed.leader); return false; }
        }
        player->SetRaidDifficultyID(difficulty);
        player->SendRaidDifficulty(false, true);
        player->GetSession()->SendLfgLfrList(false);
    }
    for (Creature* bot : bots)
        if (bot->GetBotGroup() != group && !bot->GetBotOwner()->GetBotMgr()->AddBotToGroup(bot))
        { BotDataMgr::ReleaseRaidBots(seed.leader); return false; }
    PendingRaid pending{seed.leader, group->GetGUID(), seed.raidId, seed.size, GameTime::GetGameTime() + 120, {}};
    for (size_t i = 0; i < members.size(); ++i)
    {
        group->SetLfgRoles(members[i].guid, roles[i]);
        if (members[i].guid.IsPlayer())
        {
            Player* player = ObjectAccessor::FindConnectedPlayer(members[i].guid);
            if (sMapMgr->PlayerCannotEnter(raid->mapId, player) != Map::CAN_ENTER)
            {
                BotDataMgr::ReleaseRaidBots(seed.leader);
                ChatHandler(player->GetSession()).SendSysMessage("[Raid Finder] Native instance access checks rejected entry. No bots were spawned.");
                return false;
            }
            pending.humans.push_back(members[i].guid);
        }
    }
    _pending.emplace(group->GetGUID(), pending);
    auto const* entrance = sObjectMgr->GetMapEntranceTrigger(raid->mapId);
    for (ObjectGuid guid : pending.humans)
    {
        Player* player = ObjectAccessor::FindConnectedPlayer(guid);
        if (!player->TeleportTo(entrance->target_mapId, entrance->target_X, entrance->target_Y, entrance->target_Z, entrance->target_Orientation))
        { CancelPending(group->GetGUID()); return false; }
        ChatHandler(player->GetSession()).PSendSysMessage("|cff00ff00[Raid Finder]|r %s (%u-player) formed. Entering the instance; bots arrive after the group.", raid->name.c_str(), uint32_t(seed.size));
    }
    return true;
}

void RaidFinderMgr::Update(uint32_t diff)
{
    if (_updateTimer > diff) { _updateTimer -= diff; return; }
    _updateTimer = 1000;
    int64_t now = GameTime::GetGameTime();
    for (auto it = _active.begin(); it != _active.end();)
    {
        Player* owner = ObjectAccessor::FindConnectedPlayer(it->second.leader);
        if (owner && owner->IsBeingTeleported()) { ++it; continue; }
        if (!owner || !owner->GetGroup() || owner->GetGroup()->GetGUID() != it->first ||
            !owner->IsInWorld() || owner->GetMapId() != it->second.map || !BotCfg::IsNpcBotRaidFinderEnabled())
        {
            auto record = std::move(it->second);
            it = _active.erase(it);
            if (owner)
                for (ObjectGuid bot : record.bots)
                    if (owner->GetBotMgr()->GetBot(bot)) owner->GetBotMgr()->RemoveBot(bot, BOT_REMOVE_UNSUMMON);
        }
        else ++it;
    }
    for (auto it = _pending.begin(); it != _pending.end();)
    {
        PendingRaid pending = it->second;
        Player* leader = ObjectAccessor::FindConnectedPlayer(pending.leader);
        auto const* raid = FindRaidById(pending.raidId);
        bool invalid = !BotCfg::IsNpcBotRaidFinderEnabled() || !leader || !leader->GetGroup() || leader->GetGroup()->GetGUID() != pending.group || now >= pending.expires;
        bool arrived = !invalid && raid && leader->IsInWorld() && !leader->IsBeingTeleported() && leader->GetMapId() == raid->mapId;
        for (ObjectGuid guid : pending.humans)
        {
            Player* player = ObjectAccessor::FindConnectedPlayer(guid);
            if (!player || !leader || player->GetGroup() != leader->GetGroup()) { invalid = true; arrived = false; break; }
            arrived = arrived && player->IsInWorld() && !player->IsBeingTeleported() && player->IsAlive() && player->GetMap() == leader->GetMap();
        }
        if (invalid)
        {
            BotDataMgr::ReleaseRaidBots(pending.leader);
            it = _pending.erase(it);
        }
        else if (arrived)
        {
            // Erase first: native bot additions call group scripts.
            it = _pending.erase(it);
            if (BotDataMgr::GenerateRaidBots(leader, leader->GetGroup(), raid->tier, pending.size))
            {
                ActiveRaid active{pending.leader, raid->mapId, {}};
                for (auto* ref = leader->GetGroup()->GetFirstBotMember(); ref; ref = ref->next())
                    if (Creature* bot = ref->GetSource(); bot && bot->IsSummon() && bot->GetBotOwner() == leader)
                        active.bots.push_back(bot->GetGUID());
                _active.emplace(pending.group, std::move(active));
            }
            else
                for (ObjectGuid guid : pending.humans)
                    if (Player* human = ObjectAccessor::FindConnectedPlayer(guid))
                        ChatHandler(human->GetSession()).SendSysMessage("[Raid Finder] Bot creation failed. The reservation was released; no partial generated team was kept.");
        }
        else ++it;
    }
    std::vector<ObjectGuid> invalid;
    for (auto const& [leader, entry] : _queue) if (!SnapshotValid(entry)) invalid.push_back(leader);
    for (ObjectGuid leader : invalid) OnLogout(leader);
    std::vector<QueueEntry> queue;
    for (auto const& [_, entry] : _queue) queue.push_back(entry);
    std::stable_sort(queue.begin(), queue.end(), [](QueueEntry const& a, QueueEntry const& b) { return a.joined < b.joined; });
    // Bound matchmaking work on large queues; each succeeding tick sees the remainder.
    for (size_t seed = 0; seed < std::min<size_t>(queue.size(), 32); ++seed)
    {
        if (!_queue.contains(queue[seed].leader)) continue;
        std::vector<QueueEntry> compatible;
        std::vector<QueueUnit> units;
        size_t localSeed = 0;
        for (size_t i = 0; i < queue.size() && compatible.size() < 64; ++i)
            if (_queue.contains(queue[i].leader) && queue[i].raidId == queue[seed].raidId && queue[i].size == queue[seed].size)
            {
                if (i == seed) localSeed = compatible.size();
                QueueUnit unit{queue[i].team, {}};
                for (auto const& member : queue[i].members)
                {
                    unit.roles.push_back(member.roles);
                    if (member.guid.IsPlayer()) ++unit.humans;
                }
                units.push_back(std::move(unit));
                compatible.push_back(queue[i]);
            }
        auto batch = SelectBatch(units, localSeed, queue[seed].size);
        if (!batch || !Ready(now, queue[seed].joined, BotCfg::GetRaidFinderWaitSeconds(), batch->roles.size(), queue[seed].size)) continue;
        std::vector<QueueEntry> selected;
        for (size_t index : batch->units) selected.push_back(compatible[index]);
        FormRaid(selected, batch->roles);
    }
}
}
