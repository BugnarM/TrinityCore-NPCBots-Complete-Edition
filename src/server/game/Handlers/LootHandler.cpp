/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "WorldSession.h"
#include "Common.h"
#include "Corpse.h"
#include "Creature.h"
#include "GameObject.h"
#include "Group.h"
#include "Item.h"
#include "Log.h"
#include "LootItemStorage.h"
#include "LootMgr.h"
#include "LootPackets.h"
#include "Map.h"
#include "Object.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "WorldPacket.h"

//npcbot
#include "botconfig.h"
#include "botmgr.h"
#include "World.h"
//end npcbot
#include "Config.h"
#include "ObjectMgr.h"
#include "QuestDef.h"
#include <algorithm>
#include <limits>

void WorldSession::HandleAutostoreLootItemOpcode(WorldPackets::Loot::LootItem& packet)
{
    TC_LOG_DEBUG("network", "WORLD: CMSG_AUTOSTORE_LOOT_ITEM");
    Player* player = GetPlayer();
    ObjectGuid lguid = player->GetLootGUID();
    Loot* loot = nullptr;

    if (lguid.IsGameObject())
    {
        GameObject* go = player->GetMap()->GetGameObject(lguid);

        // not check distance for GO in case owned GO (fishing bobber case, for example) or Fishing hole GO
        if (!go || ((go->GetOwnerGUID() != _player->GetGUID() && go->GetGoType() != GAMEOBJECT_TYPE_FISHINGHOLE) && !go->IsWithinDistInMap(_player)))
        {
            player->SendLootRelease(lguid);
            return;
        }

        loot = &go->loot;
    }
    else if (lguid.IsItem())
    {
        Item* pItem = player->GetItemByGuid(lguid);

        if (!pItem)
        {
            player->SendLootRelease(lguid);
            return;
        }

        loot = &pItem->loot;
    }
    else if (lguid.IsCorpse())
    {
        Corpse* bones = ObjectAccessor::GetCorpse(*player, lguid);
        if (!bones)
        {
            player->SendLootRelease(lguid);
            return;
        }

        loot = &bones->loot;
    }
    else
    {
        Creature* creature = GetPlayer()->GetMap()->GetCreature(lguid);

        bool lootAllowed = creature && creature->IsAlive() == (player->GetClass() == CLASS_ROGUE && creature->loot.loot_type == LOOT_PICKPOCKETING);
        if (!lootAllowed || !creature->IsWithinDistInMap(_player, INTERACTION_DISTANCE))
        {
            player->SendLootError(lguid, lootAllowed ? LOOT_ERROR_TOO_FAR : LOOT_ERROR_DIDNT_KILL);
            return;
        }

        loot = &creature->loot;
    }

    player->StoreLootItem(packet.LootListID, loot);

    // If player is removing the last LootItem, delete the empty container.
    if (loot->isLooted() && lguid.IsItem())
        player->GetSession()->DoLootRelease(lguid);
}

void WorldSession::HandleLootMoneyOpcode(WorldPackets::Loot::LootMoney& /*packet*/)
{
    TC_LOG_DEBUG("network", "WORLD: CMSG_LOOT_MONEY");

    Player* player = GetPlayer();
    ObjectGuid guid = player->GetLootGUID();
    if (!guid)
        return;

    Loot* loot = nullptr;
    bool shareMoney = true;

    switch (guid.GetHigh())
    {
        case HighGuid::GameObject:
        {
            GameObject* go = GetPlayer()->GetMap()->GetGameObject(guid);

            // do not check distance for GO if player is the owner of it (ex. fishing bobber)
            if (go && ((go->GetOwnerGUID() == player->GetGUID() || go->IsWithinDistInMap(player))))
                loot = &go->loot;

            break;
        }
        case HighGuid::Corpse:                               // remove insignia ONLY in BG
        {
            Corpse* bones = ObjectAccessor::GetCorpse(*player, guid);

            if (bones && bones->IsWithinDistInMap(player, INTERACTION_DISTANCE))
            {
                loot = &bones->loot;
                shareMoney = false;
            }

            break;
        }
        case HighGuid::Item:
        {
            if (Item* item = player->GetItemByGuid(guid))
            {
                loot = &item->loot;
                shareMoney = false;
            }
            break;
        }
        case HighGuid::Unit:
        case HighGuid::Vehicle:
        {
            Creature* creature = player->GetMap()->GetCreature(guid);
            bool lootAllowed = creature && creature->IsAlive() == (player->GetClass() == CLASS_ROGUE && creature->loot.loot_type == LOOT_PICKPOCKETING);
            if (lootAllowed && creature->IsWithinDistInMap(player, INTERACTION_DISTANCE))
            {
                loot = &creature->loot;
                if (creature->IsAlive())
                    shareMoney = false;
            }
            else
                player->SendLootError(guid, lootAllowed ? LOOT_ERROR_TOO_FAR : LOOT_ERROR_DIDNT_KILL);
            break;
        }
        default:
            return;                                         // unlootable type
    }

    if (loot)
    {
        loot->NotifyMoneyRemoved();
        //npcbot
        if (shareMoney && BotCfg::GetNpcBotMoneyShareEnabled())
        {
            const float reward_dist_sq = std::pow(sWorld->getFloatConfig(CONFIG_GROUP_XP_DISTANCE), 2.f);
            std::vector<Player*> playersAll;
            Group* group = player->GetGroup();
            if (group)
            {
                playersAll.reserve(group->GetMembersCount());
                for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
                {
                    if (Player* member = itr->GetSource())
                        if (member->IsInMap(player))
                            playersAll.push_back(member);
                }
            }
            else
                playersAll.push_back(player);
            std::vector<Player*> playersNear;
            uint32 bots_count = 0;
            for (Player* member : playersAll)
            {
                if (player->IsAtGroupRewardDistance(member))
                    playersNear.push_back(member);

                if (!member->HaveBot())
                    continue;

                BotMap const* botMap = member->GetBotMgr()->GetBotMap();
                for (auto const& kv : *botMap)
                {
                    Creature const* bot = kv.second;
                    if (bot && bot->IsAlive() && bot->IsInMap(player) && (!BotCfg::GetNpcBotMoneyShareGroupOnly() || (group && group->IsMember(kv.first))) &&
                        (member->GetMap()->IsDungeon() || player->GetExactDistSq(bot) <= reward_dist_sq))
                        ++bots_count;
                }
            }

            uint32 sharers_count = uint32(playersNear.size()) + bots_count;
            uint32 goldPerPlayer = uint32(loot->gold / sharers_count);

            for (std::vector<Player*>::const_iterator i = playersNear.begin(); i != playersNear.end(); ++i)
            {
                (*i)->ModifyMoney(goldPerPlayer);
                (*i)->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_MONEY, goldPerPlayer);

                WorldPacket data(SMSG_LOOT_MONEY_NOTIFY, 4 + 1);
                data << uint32(goldPerPlayer);
                data << uint8(sharers_count <= 1); // Controls the text displayed in chat. 0 is "Your share is..." and 1 is "You loot..."
                (*i)->SendDirectMessage(&data);
            }
        }
        else
        //end npcbot
        if (shareMoney && player->GetGroup())      //item, pickpocket and players can be looted only single player
        {
            Group* group = player->GetGroup();

            std::vector<Player*> playersNear;
            for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
            {
                Player* member = itr->GetSource();
                if (!member)
                    continue;

                if (player->IsAtGroupRewardDistance(member))
                    playersNear.push_back(member);
            }

            uint32 goldPerPlayer = uint32((loot->gold) / (playersNear.size()));

            for (std::vector<Player*>::const_iterator i = playersNear.begin(); i != playersNear.end(); ++i)
            {
                (*i)->ModifyMoney(goldPerPlayer);
                (*i)->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_MONEY, goldPerPlayer);

                WorldPackets::Loot::LootMoneyNotify packet;
                packet.Money = goldPerPlayer;
                packet.SoleLooter = playersNear.size() <= 1;
                (*i)->SendDirectMessage(packet.Write());
            }
        }
        else
        {
            player->ModifyMoney(loot->gold);
            player->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_MONEY, loot->gold);

            WorldPackets::Loot::LootMoneyNotify packet;
            packet.Money = loot->gold;
            packet.SoleLooter = true; // "You loot..."
            SendPacket(packet.Write());
        }

        loot->gold = 0;

        // Delete the money loot record from the DB
        if (loot->containerID > 0)
            sLootItemStorage->RemoveStoredMoneyForContainer(loot->containerID);

        // Delete container if empty
        if (loot->isLooted() && guid.IsItem())
            player->GetSession()->DoLootRelease(guid);
    }
}

namespace
{
    constexpr size_t MAX_AOE_LOOT_ITEMS = 16;
    constexpr uint32 AOE_LOOT_STACK_LIMIT = 200;

    bool CanStackRegularLoot(LootItem const& existingItem, LootItem const& incomingItem)
    {
        return !existingItem.is_looted &&
            !incomingItem.is_looted &&
            !existingItem.freeforall &&
            !incomingItem.freeforall &&
            !existingItem.needs_quest &&
            !incomingItem.needs_quest &&
            !existingItem.is_blocked &&
            !incomingItem.is_blocked &&
            !existingItem.follow_loot_rules &&
            !incomingItem.follow_loot_rules &&
            existingItem.conditions.empty() &&
            incomingItem.conditions.empty() &&
            existingItem.rollWinnerGUID.IsEmpty() &&
            incomingItem.rollWinnerGUID.IsEmpty() &&
            existingItem.itemid == incomingItem.itemid &&
            existingItem.randomPropertySeed == incomingItem.randomPropertySeed &&
            existingItem.randomPropertyId == incomingItem.randomPropertyId &&
            existingItem.is_underthreshold == incomingItem.is_underthreshold &&
            existingItem.allowedGUIDs == incomingItem.allowedGUIDs;
    }

    uint32 AddOrStackRegularLoot(Loot* mainLoot, LootItem const& incomingItem, size_t reservedQuestRows)
    {
        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(incomingItem.itemid);
        uint32 stackLimit = itemTemplate ? std::min<uint32>(itemTemplate->GetMaxStackSize(), AOE_LOOT_STACK_LIMIT) : 1;

        bool canStack = stackLimit > 1 &&
            !incomingItem.freeforall &&
            !incomingItem.needs_quest &&
            !incomingItem.is_blocked &&
            !incomingItem.follow_loot_rules &&
            incomingItem.conditions.empty() &&
            incomingItem.rollWinnerGUID.IsEmpty();

        uint32 remaining = incomingItem.count;
        uint32 transferred = 0;

        if (canStack)
        {
            for (LootItem& existingItem : mainLoot->items)
            {
                if (!CanStackRegularLoot(existingItem, incomingItem) || existingItem.count >= stackLimit)
                    continue;

                uint32 amountToAdd = std::min<uint32>(remaining, stackLimit - existingItem.count);
                existingItem.count = static_cast<uint8>(existingItem.count + amountToAdd);
                remaining -= amountToAdd;
                transferred += amountToAdd;

                if (remaining == 0)
                    return transferred;
            }
        }

        while (remaining > 0)
        {
            if (mainLoot->items.size() + reservedQuestRows >= MAX_AOE_LOOT_ITEMS)
                return transferred;

            LootItem newItem = incomingItem;
            uint32 newStackCount = canStack ? std::min<uint32>(remaining, stackLimit) : remaining;

            newItem.count = static_cast<uint8>(newStackCount);
            newItem.itemIndex = static_cast<uint32>(mainLoot->items.size());
            newItem.is_looted = false;
            newItem.is_counted = false;

            mainLoot->items.push_back(newItem);
            transferred += newStackCount;

            if (!newItem.freeforall && newItem.conditions.empty() && !newItem.needs_quest)
                ++mainLoot->unlootedCount;

            remaining -= newStackCount;

            if (!canStack)
                break;
        }

        return transferred;
    }

    uint32 AddOrStackQuestLoot(Loot* mainLoot, LootItem const& incomingItem, Player* player)
    {
        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(incomingItem.itemid);
        uint32 stackLimit = itemTemplate ? std::min<uint32>(itemTemplate->GetMaxStackSize(), AOE_LOOT_STACK_LIMIT) : 1;

        uint32 remaining = incomingItem.count;
        uint32 transferred = 0;

        // 1. Try to stack into an existing unlooted quest item in mainLoot
        if (stackLimit > 1)
        {
            for (LootItem& existingItem : mainLoot->quest_items)
            {
                if (!existingItem.is_looted && existingItem.itemid == incomingItem.itemid && existingItem.count < stackLimit)
                {
                    uint32 amountToAdd = std::min<uint32>(remaining, stackLimit - existingItem.count);
                    existingItem.count = static_cast<uint8>(existingItem.count + amountToAdd);
                    remaining -= amountToAdd;
                    transferred += amountToAdd;

                    if (remaining == 0)
                        return transferred;
                }
            }
        }

        // 2. Add as a new quest item slot in mainLoot
        while (remaining > 0)
        {
            NotNormalLootItemList* qlist = nullptr;
            NotNormalLootItemMap& qmap = mainLoot->GetPlayerQuestItems();
            auto itr = qmap.find(player->GetGUID());
            if (itr != qmap.end())
                qlist = itr->second;
            else
            {
                qlist = new NotNormalLootItemList();
                qmap[player->GetGUID()] = qlist;
            }

            size_t totalSlots = mainLoot->items.size() + (qlist ? qlist->size() : 0);
            if (totalSlots >= MAX_AOE_LOOT_ITEMS || mainLoot->quest_items.size() >= MAX_NR_QUEST_ITEMS)
                return transferred;

            uint32 newStackCount = (stackLimit > 1) ? std::min<uint32>(remaining, stackLimit) : 1;
            uint32 newIndex = static_cast<uint32>(mainLoot->quest_items.size());

            LootItem newItem = incomingItem;
            newItem.count = static_cast<uint8>(newStackCount);
            newItem.itemIndex = newIndex;
            newItem.is_looted = false;
            newItem.is_counted = false;

            if (!player->GetGroup() || (player->GetGroup()->GetLootMethod() != GROUP_LOOT && player->GetGroup()->GetLootMethod() != ROUND_ROBIN))
                newItem.is_blocked = true;

            mainLoot->quest_items.push_back(newItem);
            qlist->push_back(NotNormalLootItem(static_cast<uint8>(newIndex)));
            ++mainLoot->unlootedCount;

            transferred += newStackCount;
            remaining -= newStackCount;

            if (stackLimit <= 1)
                break;
        }

        return transferred;
    }

    uint8 GetLootSortPriority(LootItem const& item)
    {
        ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(item.itemid);
        if (!itemTemplate)
            return 5;

        switch (itemTemplate->Quality)
        {
            case ITEM_QUALITY_HEIRLOOM:
            case ITEM_QUALITY_ARTIFACT:
            case ITEM_QUALITY_LEGENDARY:
                return 0;
            case ITEM_QUALITY_EPIC:
                return 1;
            case ITEM_QUALITY_RARE:
                return 2;
            case ITEM_QUALITY_UNCOMMON:
                return 3;
            case ITEM_QUALITY_POOR:
                return 6;
            case ITEM_QUALITY_NORMAL:
            default:
                break;
        }

        if (itemTemplate->Class == ITEM_CLASS_TRADE_GOODS ||
            itemTemplate->Class == ITEM_CLASS_REAGENT ||
            itemTemplate->Class == ITEM_CLASS_GEM)
            return 4;

        if (itemTemplate->Class == ITEM_CLASS_CONSUMABLE &&
            itemTemplate->SubClass == ITEM_SUBCLASS_FOOD_DRINK)
            return 7;

        if (itemTemplate->Class == ITEM_CLASS_RECIPE)
            return 8;

        return 5;
    }

    bool HasSimpleRegularLootRules(LootItem const& item)
    {
        return !item.freeforall &&
            !item.needs_quest &&
            !item.is_blocked &&
            !item.follow_loot_rules &&
            item.conditions.empty() &&
            item.rollWinnerGUID.IsEmpty();
    }

    bool CanSafelySortLootItem(LootItem const& item)
    {
        return !item.is_looted && HasSimpleRegularLootRules(item);
    }

    bool HasPendingRoll(Loot const& loot)
    {
        auto isPending = [](LootItem const& item)
        {
            return !item.is_looted && (item.is_blocked || !item.rollWinnerGUID.IsEmpty());
        };

        return std::any_of(loot.items.begin(), loot.items.end(), isPending) ||
            std::any_of(loot.quest_items.begin(), loot.quest_items.end(), isPending);
    }

    bool IsReservedByGroupLootRules(Player const* player, Loot const& loot, LootItem const& item)
    {
        Group const* group = player->GetGroup();
        if (!group || group->GetLootMethod() == FREE_FOR_ALL)
            return false;

        // Check if there are other real players in the group
        bool hasOtherRealPlayers = false;
        for (GroupReference const* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player const* member = itr->GetSource();
            if (member && member != player)
            {
                hasOtherRealPlayers = true;
                break;
            }
        }
        if (!hasOtherRealPlayers)
            return false;

        if (group->GetLootMethod() != ROUND_ROBIN && !item.is_underthreshold)
            return true;

        if (loot.roundRobinPlayer.IsEmpty() || loot.roundRobinPlayer == player->GetGUID())
            return false;

        return group->GetLootMethod() == ROUND_ROBIN || item.is_underthreshold;
    }

    void CompactTransferredRegularLoot(Loot* loot)
    {
        if (!loot || !std::all_of(loot->items.begin(), loot->items.end(), HasSimpleRegularLootRules))
            return;

        loot->items.erase(
            std::remove_if(loot->items.begin(), loot->items.end(),
                [](LootItem const& item) { return item.is_looted; }),
            loot->items.end());

        for (size_t i = 0; i < loot->items.size(); ++i)
            loot->items[i].itemIndex = static_cast<uint32>(i);
    }

    void SortRegularLoot(Loot* loot)
    {
        if (!loot || !std::all_of(loot->items.begin(), loot->items.end(), CanSafelySortLootItem))
            return;

        std::stable_sort(loot->items.begin(), loot->items.end(),
            [](LootItem const& left, LootItem const& right)
            {
                return GetLootSortPriority(left) < GetLootSortPriority(right);
            });

        for (size_t i = 0; i < loot->items.size(); ++i)
            loot->items[i].itemIndex = static_cast<uint32>(i);
    }

    void ProcessAoELoot(Player* player, Creature* mainCreature)
    {
        if (!sConfigMgr->GetBoolDefault("AOELoot.Enable", true))
            return;

        if (!mainCreature || !mainCreature->HasDynamicFlag(UNIT_DYNFLAG_LOOTABLE))
            return;

        if (mainCreature->loot.loot_type == LOOT_SKINNING || mainCreature->loot.loot_type == LOOT_PICKPOCKETING)
            return;

        if (HasPendingRoll(mainCreature->loot))
            return;

        float range = sConfigMgr->GetFloatDefault("AOELoot.Range", 120.0f);
        if (range < 5.0f) range = 5.0f;
        if (range > 300.0f) range = 300.0f;

        uint32 maxCorpses = sConfigMgr->GetIntDefault("AOELoot.MaxCorpses", 60);
        if (maxCorpses < 1) maxCorpses = 1;

        std::list<Creature*> nearbyCorpses;
        mainCreature->GetCreatureListWithEntryInGrid(nearbyCorpses, 0, range);

        nearbyCorpses.remove_if([&](Creature* c)
        {
            return !c ||
                c == mainCreature ||
                !c->isDead() ||
                !c->HasDynamicFlag(UNIT_DYNFLAG_LOOTABLE) ||
                c->loot.loot_type == LOOT_SKINNING ||
                c->loot.loot_type == LOOT_PICKPOCKETING ||
                HasPendingRoll(c->loot) ||
                !player->isAllowedToLoot(c);
        });

        if (nearbyCorpses.empty())
            return;

        if (nearbyCorpses.size() > maxCorpses)
            nearbyCorpses.resize(maxCorpses);

        Loot* mainLoot = &mainCreature->loot;
        uint32 totalGold = mainLoot->gold;

        struct RegularLootCandidate
        {
            Creature* sourceCreature;
            size_t sourceIndex;
            LootItem item;
        };

        struct QuestLootCandidate
        {
            Creature* sourceCreature;
            size_t sourceIndex;
            LootItem item;
        };

        std::vector<Creature*> processedCreatures;
        std::vector<RegularLootCandidate> regularCandidates;
        std::vector<QuestLootCandidate> questCandidates;

        for (Creature* creature : nearbyCorpses)
        {
            if (!creature)
                continue;

            Loot* loot = &creature->loot;
            if (loot->isLooted())
                continue;

            processedCreatures.push_back(creature);

            // Merge gold
            if (loot->gold > 0)
            {
                if (loot->gold <= std::numeric_limits<uint32>::max() - totalGold)
                {
                    totalGold += loot->gold;
                    loot->gold = 0;
                }
            }

            // Collect regular item candidates
            for (size_t i = 0; i < loot->items.size(); ++i)
            {
                LootItem const& item = loot->items[i];
                if (!CanSafelySortLootItem(item))
                    continue;

                if (!item.AllowedForPlayer(player, mainLoot->lootOwnerGUID))
                    continue;

                if (IsReservedByGroupLootRules(player, *loot, item) ||
                    IsReservedByGroupLootRules(player, *mainLoot, item))
                    continue;

                regularCandidates.push_back({ creature, i, item });
            }

            // Collect quest items needed by player
            for (size_t i = 0; i < loot->quest_items.size(); ++i)
            {
                LootItem const& questItem = loot->quest_items[i];
                if (questItem.is_looted)
                    continue;

                if (!player->HasQuestForItem(questItem.itemid))
                    continue;

                if (!questItem.AllowedForPlayer(player, loot->lootOwnerGUID))
                    continue;

                if (!questItem.rollWinnerGUID.IsEmpty() && questItem.rollWinnerGUID != player->GetGUID())
                    continue;

                uint32 maxNeeded = 0;
                for (uint8 slot = 0; slot < MAX_QUEST_LOG_SIZE; ++slot)
                {
                    uint32 questId = player->GetQuestSlotQuestId(slot);
                    if (!questId)
                        continue;

                    Quest const* quest = sObjectMgr->GetQuestTemplate(questId);
                    if (!quest)
                        continue;

                    for (uint8 j = 0; j < QUEST_ITEM_OBJECTIVES_COUNT; ++j)
                    {
                        if (quest->RequiredItemId[j] == questItem.itemid && quest->RequiredItemCount[j] > maxNeeded)
                            maxNeeded = quest->RequiredItemCount[j];
                    }
                }

                uint32 ownedCount = player->GetItemCount(questItem.itemid, true);
                for (QuestLootCandidate const& pending : questCandidates)
                {
                    if (pending.item.itemid == questItem.itemid)
                        ownedCount += pending.item.count;
                }

                for (LootItem const& mainQuestItem : mainLoot->quest_items)
                {
                    if (mainQuestItem.itemid == questItem.itemid && !mainQuestItem.is_looted)
                        ownedCount += mainQuestItem.count;
                }

                if (maxNeeded > 0 && ownedCount >= maxNeeded)
                    continue;

                uint32 stillNeeded = (maxNeeded > 0 && maxNeeded > ownedCount) ? (maxNeeded - ownedCount) : static_cast<uint32>(questItem.count);
                LootItem cappedItem = questItem;
                cappedItem.count = static_cast<uint8>(std::min(static_cast<uint32>(questItem.count), stillNeeded));
                if (cappedItem.count > 0)
                    questCandidates.push_back({ creature, i, cappedItem });
            }
        }

        // 1. Transfer quest items into mainLoot (consolidating and stacking into corpse loot window)
        for (QuestLootCandidate const& candidate : questCandidates)
        {
            Loot* sourceLoot = &candidate.sourceCreature->loot;
            if (candidate.sourceIndex >= sourceLoot->quest_items.size())
                continue;

            LootItem& sourceItem = sourceLoot->quest_items[candidate.sourceIndex];
            if (sourceItem.is_looted)
                continue;

            uint32 sourceCount = sourceItem.count;
            uint32 transferred = AddOrStackQuestLoot(mainLoot, candidate.item, player);

            if (transferred == 0)
                continue;

            if (transferred >= sourceCount)
            {
                sourceItem.is_looted = true;
                if (sourceLoot->unlootedCount > 0)
                    --sourceLoot->unlootedCount;
            }
            else
            {
                sourceItem.count = static_cast<uint8>(sourceCount - transferred);
            }
        }

        // 2. Sort regular candidates by priority (high quality first)
        std::stable_sort(regularCandidates.begin(), regularCandidates.end(),
            [](RegularLootCandidate const& left, RegularLootCandidate const& right)
            {
                return GetLootSortPriority(left.item) < GetLootSortPriority(right.item);
            });

        size_t reservedQuestRows = mainLoot->quest_items.size();

        // 3. Transfer regular items into mainLoot
        for (RegularLootCandidate const& candidate : regularCandidates)
        {
            Loot* sourceLoot = &candidate.sourceCreature->loot;
            if (candidate.sourceIndex >= sourceLoot->items.size())
                continue;

            LootItem& sourceItem = sourceLoot->items[candidate.sourceIndex];
            if (sourceItem.is_looted)
                continue;

            uint32 sourceCount = sourceItem.count;
            uint32 transferred = AddOrStackRegularLoot(mainLoot, sourceItem, reservedQuestRows);

            if (transferred == 0)
                continue;

            if (transferred >= sourceCount)
            {
                sourceItem.is_looted = true;
                if (sourceLoot->unlootedCount > 0)
                    --sourceLoot->unlootedCount;
            }
            else
            {
                sourceItem.count = static_cast<uint8>(sourceCount - transferred);
            }
        }

        // Set merged gold
        mainLoot->gold = totalGold;

        // Clean up emptied source corpses
        for (Creature* creature : processedCreatures)
        {
            Loot* loot = &creature->loot;
            CompactTransferredRegularLoot(loot);

            if (!loot->isLooted())
                continue;

            creature->RemoveDynamicFlag(UNIT_DYNFLAG_LOOTABLE);
            if (!creature->IsAlive())
                creature->AllLootRemovedFromCorpse();
            loot->clear();
        }

        // Sort regular loot on main creature
        SortRegularLoot(mainLoot);
    }
}

void WorldSession::HandleLootOpcode(WorldPackets::Loot::LootUnit& packet)
{
    TC_LOG_DEBUG("network", "WORLD: CMSG_LOOT");

    // Check possible cheat
    if (!GetPlayer()->IsAlive() || !packet.Unit.IsCreatureOrVehicle())
        return;

    // interrupt cast
    if (GetPlayer()->IsNonMeleeSpellCast(false))
        GetPlayer()->InterruptNonMeleeSpells(false);

    GetPlayer()->RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_LOOTING);

    if (Creature* mainCreature = GetPlayer()->GetMap()->GetCreature(packet.Unit))
        ProcessAoELoot(GetPlayer(), mainCreature);

    GetPlayer()->SendLoot(packet.Unit, LOOT_CORPSE);
}

void WorldSession::HandleLootReleaseOpcode(WorldPackets::Loot::LootRelease& packet)
{
    TC_LOG_DEBUG("network", "WORLD: CMSG_LOOT_RELEASE");

    // cheaters can modify lguid to prevent correct apply loot release code and re-loot
    // use internal stored guid
    ObjectGuid lguid = GetPlayer()->GetLootGUID();
    if (!lguid.IsEmpty())
        if (lguid == packet.Unit)
            DoLootRelease(lguid);
}

void WorldSession::DoLootRelease(ObjectGuid lguid)
{
    Player  *player = GetPlayer();
    Loot    *loot;

    player->SetLootGUID(ObjectGuid::Empty);
    player->SendLootRelease(lguid);

    player->RemoveUnitFlag(UNIT_FLAG_LOOTING);

    if (!player->IsInWorld())
        return;

    if (lguid.IsGameObject())
    {
        GameObject* go = GetPlayer()->GetMap()->GetGameObject(lguid);

        // not check distance for GO in case owned GO (fishing bobber case, for example) or Fishing hole GO
        if (!go || ((go->GetOwnerGUID() != _player->GetGUID() && go->GetGoType() != GAMEOBJECT_TYPE_FISHINGHOLE) && !go->IsWithinDistInMap(_player)))
            return;

        loot = &go->loot;

        if (go->GetGoType() == GAMEOBJECT_TYPE_DOOR)
        {
            // locked doors are opened with spelleffect openlock, prevent remove its as looted
            go->UseDoorOrButton();
        }
        else if (loot->isLooted() || go->GetGoType() == GAMEOBJECT_TYPE_FISHINGNODE)
        {
            if (go->GetGoType() == GAMEOBJECT_TYPE_FISHINGHOLE)
            {
                bool allOpensConsumed = false;
                if (go->GetGOValue()->FishingHole.MaxOpens)
                {
                    // The fishing hole used once more
                    go->AddUse();

                    // If the max usage is reached, will be despawned in next tick
                    allOpensConsumed = go->GetUseCount() >= go->GetGOValue()->FishingHole.MaxOpens;
                }

                go->SetLootState(allOpensConsumed ? GO_JUST_DEACTIVATED : GO_READY);
            }
            else
                go->SetLootState(GO_JUST_DEACTIVATED);

            loot->clear();
        }
        else
        {
            // not fully looted object
            go->SetLootState(GO_ACTIVATED, player);

            // if the round robin player release, reset it.
            if (player->GetGUID() == loot->roundRobinPlayer)
                loot->roundRobinPlayer.Clear();
        }
    }
    else if (lguid.IsCorpse())        // ONLY remove insignia at BG
    {
        Corpse* corpse = ObjectAccessor::GetCorpse(*player, lguid);
        if (!corpse || !corpse->IsWithinDistInMap(_player, INTERACTION_DISTANCE))
            return;

        loot = &corpse->loot;

        if (loot->isLooted())
        {
            loot->clear();
            corpse->RemoveCorpseDynamicFlag(CORPSE_DYNFLAG_LOOTABLE);
        }
    }
    else if (lguid.IsItem())
    {
        Item* pItem = player->GetItemByGuid(lguid);
        if (!pItem)
            return;

        ItemTemplate const* proto = pItem->GetTemplate();

        // destroy only 5 items from stack in case prospecting and milling
        if (proto->HasFlag(ITEM_FLAG_IS_PROSPECTABLE) || proto->HasFlag(ITEM_FLAG_IS_MILLABLE))
        {
            pItem->m_lootGenerated = false;
            pItem->loot.clear();

            uint32 count = pItem->GetCount();

            // >=5 checked in spell code, but will work for cheating cases also with removing from another stacks.
            if (count > 5)
                count = 5;

            player->DestroyItemCount(pItem, count, true);
        }
        else
        {
            // Only delete item if no loot or money (unlooted loot is saved to db) or if it isn't an openable item
            if (pItem->loot.isLooted() || !proto->HasFlag(ITEM_FLAG_HAS_LOOT))
                player->DestroyItem(pItem->GetBagSlot(), pItem->GetSlot(), true);
        }
        return;                                             // item can be looted only single player
    }
    else
    {
        Creature* creature = GetPlayer()->GetMap()->GetCreature(lguid);

        bool lootAllowed = creature && creature->IsAlive() == (player->GetClass() == CLASS_ROGUE && creature->loot.loot_type == LOOT_PICKPOCKETING);
        if (!lootAllowed || !creature->IsWithinDistInMap(_player, INTERACTION_DISTANCE))
            return;

        loot = &creature->loot;
        if (loot->isLooted())
        {
            creature->RemoveDynamicFlag(UNIT_DYNFLAG_LOOTABLE);

            // skip pickpocketing loot for speed, skinning timer reduction is no-op in fact
            if (!creature->IsAlive())
                creature->AllLootRemovedFromCorpse();

            loot->clear();
        }
        else
        {
            // if the round robin player release, reset it.
            if (player->GetGUID() == loot->roundRobinPlayer)
            {
                loot->roundRobinPlayer.Clear();

                if (Group* group = player->GetGroup())
                    group->SendLooter(creature, nullptr);
            }
            // force dynflag update to update looter and lootable info
            creature->ForceValuesUpdateAtIndex(UNIT_DYNAMIC_FLAGS);
        }
    }

    //Player is not looking at loot list, he doesn't need to see updates on the loot list
    loot->RemoveLooter(player->GetGUID());
}

void WorldSession::HandleLootMasterGiveOpcode(WorldPacket& recvData)
{
    uint8 slotid;
    ObjectGuid lootguid, target_playerguid;

    recvData >> lootguid >> slotid >> target_playerguid;

    if (!_player->GetGroup() || _player->GetGroup()->GetMasterLooterGuid() != _player->GetGUID() || _player->GetGroup()->GetLootMethod() != MASTER_LOOT)
    {
        _player->SendLootError(lootguid, LOOT_ERROR_DIDNT_KILL);
        return;
    }

    // player on other map
    Player* target = ObjectAccessor::GetPlayer(*_player, target_playerguid);
    if (!target)
    {
        _player->SendLootError(lootguid, LOOT_ERROR_PLAYER_NOT_FOUND);
        return;
    }

    TC_LOG_DEBUG("network", "WorldSession::HandleLootMasterGiveOpcode (CMSG_LOOT_MASTER_GIVE, 0x02A3) Target = [{}].", target->GetName());

    if (_player->GetLootGUID() != lootguid)
    {
        _player->SendLootError(lootguid, LOOT_ERROR_DIDNT_KILL);
        return;
    }

    if (!_player->IsInRaidWith(target) || !_player->IsInMap(target))
    {
        _player->SendLootError(lootguid, LOOT_ERROR_MASTER_OTHER);
        TC_LOG_INFO("entities.player.cheat", "MasterLootItem: Player {} tried to give an item to ineligible player {} !", GetPlayer()->GetName(), target->GetName());
        return;
    }

    Loot* loot = nullptr;

    if (GetPlayer()->GetLootGUID().IsCreatureOrVehicle())
    {
        Creature* creature = GetPlayer()->GetMap()->GetCreature(lootguid);
        if (!creature)
            return;

        loot = &creature->loot;
    }
    else if (GetPlayer()->GetLootGUID().IsGameObject())
    {
        GameObject* pGO = GetPlayer()->GetMap()->GetGameObject(lootguid);
        if (!pGO)
            return;

        loot = &pGO->loot;
    }

    if (!loot)
        return;

    if (slotid >= loot->items.size() + loot->quest_items.size())
    {
        TC_LOG_DEBUG("loot", "MasterLootItem: Player {} might be using a hack! (slot {}, size {})",
            GetPlayer()->GetName(), slotid, (unsigned long)loot->items.size());
        return;
    }

    LootItem& item = slotid >= loot->items.size() ? loot->quest_items[slotid - loot->items.size()] : loot->items[slotid];

    ItemPosCountVec dest;
    InventoryResult msg = target->CanStoreNewItem(NULL_BAG, NULL_SLOT, dest, item.itemid, item.count);
    if (!item.AllowedForPlayer(target, true))
        msg = EQUIP_ERR_CANT_EQUIP_EVER;
    if (msg != EQUIP_ERR_OK)
    {
        if (msg == EQUIP_ERR_ITEM_MAX_COUNT)
            _player->SendLootError(lootguid, LOOT_ERROR_MASTER_UNIQUE_ITEM);
        else if (msg == EQUIP_ERR_INV_FULL)
            _player->SendLootError(lootguid, LOOT_ERROR_MASTER_INV_FULL);
        else
            _player->SendLootError(lootguid, LOOT_ERROR_MASTER_OTHER);
        return;
    }

    // now move item from loot to target inventory
    Item* newitem = target->StoreNewItem(dest, item.itemid, true, item.randomPropertyId, item.GetAllowedLooters());
    target->SendNewItem(newitem, uint32(item.count), false, false, true);
    target->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_ITEM, item.itemid, item.count);
    target->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_TYPE, GetLootTypeForClient(loot->loot_type), item.count);
    target->UpdateAchievementCriteria(ACHIEVEMENT_CRITERIA_TYPE_LOOT_EPIC_ITEM, item.itemid, item.count);

    // mark as looted
    item.count = 0;
    item.is_looted = true;

    loot->NotifyItemRemoved(slotid);
    --loot->unlootedCount;
}
