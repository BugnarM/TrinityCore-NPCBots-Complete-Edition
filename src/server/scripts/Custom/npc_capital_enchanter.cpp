/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "AuctionHouseMgr.h"
#include "Config.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "DBCStores.h"
#include "DisableMgr.h"
#include "Duration.h"
#include "GameTime.h"
#include "GossipDef.h"
#include "Item.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "StringFormat.h"
#include "WorldSession.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
constexpr uint32 EnchanterText = 70903;
constexpr uint32 GossipSender = GOSSIP_SENDER_MAIN;
constexpr std::size_t EnchantsPerPage = 12;
constexpr auto MenuLifetime = Minutes(2);

enum Action : uint32
{
    ShowGear = 1,
    ToggleMaterials,
    PreviousPage,
    NextPage,
    BackToEnchants,
    BuyEnchant,
    NoAction,
    SelectSlot = 100,
    SelectOffer = 200
};

struct Enchant
{
    SpellInfo const* spell;
    SpellItemEnchantmentEntry const* effect;
    ItemTemplate const* upgrade;
    EnchantmentSlot slot;
    uint32 recipeSkill;
    uint32 skillToUse;
    uint32 skillRankToUse;
    std::map<uint32, uint32> materials;
};

using UnitPrices = std::unordered_map<uint32, uint32>;

struct Catalog
{
    std::vector<Enchant> enchants;
    std::unordered_set<uint32> pricedItems;
    UnitPrices vendors;
};

struct AuctionPrices
{
    UnitPrices alliance;
    UnitPrices horde;
};

Catalog Enchants;
std::atomic<std::shared_ptr<AuctionPrices const>> MarketPrices{ std::make_shared<AuctionPrices>() };

bool IsEnchantingProfession(uint32 skill)
{
    switch (skill)
    {
        case SKILL_ENCHANTING:
        case SKILL_BLACKSMITHING:
        case SKILL_ENGINEERING:
        case SKILL_INSCRIPTION:
        case SKILL_LEATHERWORKING:
        case SKILL_TAILORING:
            return true;
        default:
            return false;
    }
}

uint32 RecipeDifficulty(SkillLineAbilityEntry const& recipe)
{
    return std::clamp(std::max(recipe.MinSkillLineRank, recipe.TrivialSkillLineRankLow), 1u, 450u);
}

void AddEnchant(SpellInfo const& spell, ItemTemplate const* upgrade, SkillLineAbilityEntry const* recipe,
    std::unordered_set<uint32>& directEnchants)
{
    for (SpellEffectInfo const& effect : spell.GetEffects())
    {
        if (!effect.IsEffect(SPELL_EFFECT_ENCHANT_ITEM) && !effect.IsEffect(SPELL_EFFECT_ENCHANT_ITEM_PRISMATIC))
            continue;

        SpellItemEnchantmentEntry const* entry = sSpellItemEnchantmentStore.LookupEntry(effect.MiscValue);
        if (!entry || entry->ConditionID || !entry->Name[LOCALE_enUS] || !*entry->Name[LOCALE_enUS])
            continue;

        bool const prismatic = effect.IsEffect(SPELL_EFFECT_ENCHANT_ITEM_PRISMATIC);
        if (prismatic && std::ranges::find(entry->Effect, ITEM_ENCHANTMENT_TYPE_PRISMATIC_SOCKET) == std::end(entry->Effect))
            continue;

        if (upgrade && directEnchants.contains(entry->ID))
            continue;
        if (!upgrade)
            directEnchants.insert(entry->ID);

        Enchant enchant{ &spell, entry, upgrade, prismatic ? PRISMATIC_ENCHANTMENT_SLOT : PERM_ENCHANTMENT_SLOT,
            recipe ? RecipeDifficulty(*recipe) : uint32(std::clamp(upgrade->GetRequiredLevel() * 5, 1, 450)),
            entry->RequiredSkillID, entry->RequiredSkillRank, {} };

        // Direct recipes from other professions are character-bound modifications.
        if (!upgrade && recipe && !enchant.skillToUse &&
            (recipe->SkillLine != SKILL_ENCHANTING || (spell.EquippedItemInventoryTypeMask & (1 << INVTYPE_FINGER))))
        {
            enchant.skillToUse = recipe->SkillLine;
            enchant.skillRankToUse = recipe->MinSkillLineRank ? recipe->MinSkillLineRank : enchant.recipeSkill;
        }

        if (upgrade)
            enchant.materials.emplace(upgrade->GetId(), 1);
        else
            for (std::size_t i = 0; i < spell.Reagent.size(); ++i)
                if (spell.Reagent[i] > 0 && spell.ReagentCount[i] && sObjectMgr->GetItemTemplate(uint32(spell.Reagent[i])))
                    enchant.materials[uint32(spell.Reagent[i])] += spell.ReagentCount[i];

        for (auto const& [itemId, count] : enchant.materials)
            Enchants.pricedItems.insert(itemId);

        Enchants.enchants.push_back(std::move(enchant));
    }
}

void LoadCatalog()
{
    std::map<uint32, SkillLineAbilityEntry const*> recipes;
    std::map<uint32, SkillLineAbilityEntry const*> craftedItems;
    std::unordered_set<uint32> directEnchants;

    for (SkillLineAbilityEntry const* recipe : sSkillLineAbilityStore)
        if (IsEnchantingProfession(recipe->SkillLine))
            recipes.try_emplace(recipe->Spell, recipe);

    for (auto const& [spellId, recipe] : recipes)
    {
        SpellInfo const* spell = sSpellMgr->GetSpellInfo(spellId);
        if (!spell)
            continue;

        AddEnchant(*spell, nullptr, recipe, directEnchants);
        for (SpellEffectInfo const& effect : spell->GetEffects())
            if ((effect.IsEffect(SPELL_EFFECT_CREATE_ITEM) || effect.IsEffect(SPELL_EFFECT_CREATE_ITEM_2)) && effect.ItemType)
                craftedItems.try_emplace(effect.ItemType, recipe);
    }

    for (auto const& [id, item] : sObjectMgr->GetItemTemplateStore())
    {
        auto craft = craftedItems.find(id);
        bool const craftedUpgrade = craft != craftedItems.end() &&
            (item.GetClass() == ITEM_CLASS_CONSUMABLE || item.GetClass() == ITEM_CLASS_TRADE_GOODS);
        if (!craftedUpgrade && !item.GetRequiredReputationFaction())
            continue;

        for (ItemEffect const& effect : item.Effects)
            if (effect.TriggerType == ITEM_SPELLTRIGGER_ON_USE && effect.SpellID > 0 && effect.Charges < 0)
                if (SpellInfo const* spell = sSpellMgr->GetSpellInfo(effect.SpellID))
                    AddEnchant(*spell, &item, craft != craftedItems.end() ? craft->second : nullptr, directEnchants);
    }

    std::ranges::sort(Enchants.enchants, [](Enchant const& left, Enchant const& right)
    {
        if (left.recipeSkill != right.recipeSkill)
            return left.recipeSkill > right.recipeSkill;
        if (left.effect->ID != right.effect->ID)
            return left.effect->ID > right.effect->ID;
        return (left.upgrade ? left.upgrade->GetId() : 0) < (right.upgrade ? right.upgrade->GetId() : 0);
    });

    for (auto const& [entry, creature] : sObjectMgr->GetCreatureTemplates())
        if (VendorItemData const* vendor = sObjectMgr->GetNpcVendorItemList(entry))
            for (VendorItem const& offer : vendor->m_items)
                if (!offer.maxcount && !offer.ExtendedCost && Enchants.pricedItems.contains(offer.item))
                    if (ItemTemplate const* item = sObjectMgr->GetItemTemplate(offer.item); item && item->GetBuyPrice())
                        Enchants.vendors.try_emplace(offer.item, std::max(1u, item->GetBuyPrice() / std::max(1u, item->GetBuyCount())));

    TC_LOG_INFO("server.loading", "Capital Enchanter: loaded {} permanent enhancements and {} priced materials",
        Enchants.enchants.size(), Enchants.pricedItems.size());
}

UnitPrices ReadAuctionPrices(uint8 houseId)
{
    UnitPrices prices;
    AuctionHouseObject* house = sAuctionMgr->GetAuctionsMapByHouseId(houseId);
    time_t const now = GameTime::GetGameTime();
    for (auto auction = house->GetAuctionsBegin(); auction != house->GetAuctionsEnd(); ++auction)
    {
        AuctionEntry const& entry = *auction->second;
        if (!entry.buyout || !entry.itemCount || entry.expire_time <= now || !Enchants.pricedItems.contains(entry.itemEntry))
            continue;

        uint32 const unitPrice = uint32((uint64(entry.buyout) + entry.itemCount - 1) / entry.itemCount);
        auto [price, inserted] = prices.try_emplace(entry.itemEntry, unitPrice);
        if (!inserted)
            price->second = std::min(price->second, unitPrice);
    }

    return prices;
}

class capital_enchanter_prices final : public WorldScript
{
public:
    capital_enchanter_prices() : WorldScript("capital_enchanter_prices") { }

    void OnStartup() override
    {
        LoadCatalog();
        RefreshPrices();
    }

    void OnUpdate(uint32 diff) override
    {
        if (_untilRefresh > diff)
            _untilRefresh -= diff;
        else
            RefreshPrices();
    }

private:
    void RefreshPrices()
    {
        auto snapshot = std::make_shared<AuctionPrices>();
        snapshot->alliance = ReadAuctionPrices(AUCTIONHOUSE_ALLIANCE);
        snapshot->horde = ReadAuctionPrices(AUCTIONHOUSE_HORDE);
        MarketPrices.store(std::move(snapshot));
        _untilRefresh = std::clamp(sConfigMgr->GetIntDefault("CapitalEnchanter.PriceRefreshSeconds", 300), 30, 3600) * IN_MILLISECONDS;
    }

    uint32 _untilRefresh = 0;
};

struct Quote
{
    uint32 total = 0;
    uint32 labor = 0;
    std::vector<std::pair<uint32, uint32>> consumes;
    bool operator==(Quote const&) const = default;
};

uint32 MaterialPrice(uint32 itemId, UnitPrices const& market)
{
    ItemTemplate const* item = sObjectMgr->GetItemTemplate(itemId);
    uint32 const sellValue = item ? item->GetSellPrice() : 0;

    if (auto price = market.find(itemId); price != market.end())
        return std::max(price->second, sellValue);
    if (auto price = Enchants.vendors.find(itemId); price != Enchants.vendors.end())
        return price->second;
    if (item && item->GetBuyPrice())
        return std::max(1u, item->GetBuyPrice() / std::max(1u, item->GetBuyCount()));
    if (sellValue)
        return uint32(std::min(uint64(sellValue) * 4, uint64(MAX_MONEY_AMOUNT)));

    return std::clamp(sConfigMgr->GetIntDefault("CapitalEnchanter.MaterialFallbackCopper", 10000), 1, 10000000);
}

Quote MakeQuote(Player const& player, Enchant const& enchant, bool useOwnMaterials, UnitPrices const& market)
{
    Quote quote;
    uint32 const laborAtMax = std::clamp(sConfigMgr->GetIntDefault("CapitalEnchanter.LaborAt450Copper", 50000), 1, 10000000);
    quote.labor = std::max(1u, uint32(uint64(laborAtMax) * enchant.recipeSkill * enchant.recipeSkill / (450u * 450u)));
    uint64 total = quote.labor;
    for (auto const& [itemId, amount] : enchant.materials)
    {
        uint32 const present = useOwnMaterials ? std::min(amount, player.GetItemCount(itemId)) : 0;
        if (present)
            quote.consumes.emplace_back(itemId, present);
        total += uint64(amount - present) * MaterialPrice(itemId, market);
    }

    quote.total = uint32(std::min(total, uint64(MAX_MONEY_AMOUNT)));
    return quote;
}

bool IsCompatible(Player const& player, Item const& item, Enchant const& enchant)
{
    if (item.GetOwnerGUID() != player.GetGUID() || !item.IsEquipped() ||
        item.GetEnchantmentId(enchant.slot) == enchant.effect->ID ||
        player.GetLevel() < enchant.effect->MinLevel ||
        (enchant.skillToUse && (!player.HasSkill(enchant.skillToUse) ||
            player.GetSkillValue(enchant.skillToUse) < enchant.skillRankToUse)) ||
        !item.IsFitToSpellRequirements(enchant.spell) ||
        DisableMgr::IsDisabledFor(DISABLE_TYPE_SPELL, enchant.spell->Id, &player))
        return false;

    if (enchant.upgrade &&
        (player.CanUseItem(enchant.upgrade) != EQUIP_ERR_OK ||
            (enchant.upgrade->GetRequiredReputationFaction() &&
                uint32(player.GetReputationRank(enchant.upgrade->GetRequiredReputationFaction())) < enchant.upgrade->GetRequiredReputationRank())))
        return false;

    ItemTemplate const* proto = item.GetTemplate();
    uint32 requiredLevel = proto->GetRequiredLevel() ? proto->GetRequiredLevel() : proto->GetBaseItemLevel();
    if (!enchant.spell->HasAttribute(SPELL_ATTR2_ALLOW_LOW_LEVEL_BUFF) && requiredLevel < enchant.spell->BaseLevel)
        return false;
    if (enchant.upgrade && enchant.spell->MaxLevel && proto->GetBaseItemLevel() > enchant.spell->MaxLevel)
        return false;

    for (uint32 effect : enchant.effect->Effect)
    {
        if (effect == ITEM_ENCHANTMENT_TYPE_USE_SPELL &&
            std::ranges::any_of(proto->Effects, [](ItemEffect const& itemEffect)
            {
                return itemEffect.TriggerType == ITEM_SPELLTRIGGER_ON_USE && itemEffect.SpellID > 0;
            }))
            return false;

        if (effect == ITEM_ENCHANTMENT_TYPE_PRISMATIC_SOCKET)
        {
            if (item.GetEnchantmentId(PRISMATIC_ENCHANTMENT_SLOT))
                return false;

            uint32 sockets = 0;
            for (uint32 i = 0; i < MAX_ITEM_PROTO_SOCKETS; ++i)
                if (proto->GetSocketColor(i))
                    ++sockets;
            if (sockets == MAX_ITEM_PROTO_SOCKETS)
                return false;
        }
    }

    return true;
}

std::string MoneyText(uint32 copper)
{
    return Trinity::StringFormat("{}g {}s {}c", copper / GOLD, (copper % GOLD) / SILVER, copper % SILVER);
}

std::string EnchantName(Player const& player, SpellItemEnchantmentEntry const& entry)
{
    char const* name = entry.Name[player.GetSession()->GetSessionDbcLocale()];
    return name && *name ? name : entry.Name[LOCALE_enUS];
}

struct ShoppingSession
{
    uint32 menuId = 0;
    std::chrono::steady_clock::time_point expires;
    uint8 slot = EQUIPMENT_SLOT_END;
    ObjectGuid itemGuid;
    bool useOwnMaterials = true;
    std::size_t page = 0;
    std::vector<Enchant const*> visibleOffers;
    Enchant const* pending = nullptr;
    uint32 oldEnchant = 0;
    Quote quote;
};

void Notice(Player* player, char const* message)
{
    player->GetSession()->SendNotification("%s", message);
}

struct npc_capital_enchanter final : public ScriptedAI
{
    explicit npc_capital_enchanter(Creature* creature) : ScriptedAI(creature) { }

    bool OnGossipHello(Player* player) override
    {
        if (!CanShop(player))
            return true;

        ShoppingSession& session = _sessions[player->GetGUID()];
        session = ShoppingSession{};
        ShowEquipment(player, session);
        return true;
    }

    bool OnGossipSelect(Player* player, uint32 menuId, uint32 listId) override
    {
        if (!CanShop(player))
            return true;

        auto it = _sessions.find(player->GetGUID());
        if (it == _sessions.end() || it->second.menuId != menuId ||
            it->second.expires <= std::chrono::steady_clock::now())
        {
            CloseGossipMenuFor(player);
            return true;
        }

        ShoppingSession& session = it->second;
        uint32 const action = GetGossipActionFor(player, listId);
        if (action >= SelectSlot && action < SelectSlot + uint32(EQUIPMENT_SLOT_END))
        {
            session.slot = uint8(action - SelectSlot);
            Item const* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, session.slot);
            session.itemGuid = item ? item->GetGUID() : ObjectGuid::Empty;
            session.page = 0;
            ShowEnchants(player, session);
        }
        else if (action >= SelectOffer && action < SelectOffer + session.visibleOffers.size() && !session.pending)
            ShowQuote(player, session, *session.visibleOffers[action - SelectOffer]);
        else
            switch (action)
            {
                case ShowGear:
                    ShowEquipment(player, session);
                    break;
                case ToggleMaterials:
                    session.useOwnMaterials = !session.useOwnMaterials;
                    if (session.slot < EQUIPMENT_SLOT_END)
                        ShowEnchants(player, session);
                    else
                        ShowEquipment(player, session);
                    break;
                case PreviousPage:
                    if (session.page)
                        --session.page;
                    ShowEnchants(player, session);
                    break;
                case NextPage:
                    ++session.page;
                    ShowEnchants(player, session);
                    break;
                case BackToEnchants:
                    ShowEnchants(player, session);
                    break;
                case BuyEnchant:
                    Purchase(player, session);
                    break;
                default:
                    break;
            }

        return true;
    }

    void UpdateAI(uint32 diff) override
    {
        if (_cleanupTimer > diff)
        {
            _cleanupTimer -= diff;
            return;
        }

        _cleanupTimer = 30000;
        auto now = std::chrono::steady_clock::now();
        std::erase_if(_sessions, [now](auto const& entry) { return entry.second.expires <= now; });
    }

private:
    bool CanShop(Player* player) const
    {
        if (sConfigMgr->GetBoolDefault("CapitalEnchanter.Enable", true) && player->IsAlive() &&
            !player->IsInCombat() && !player->GetTradeData())
            return true;

        Notice(player, "Enchanting is unavailable while you are dead, trading or in combat, or when the service is closed.");
        CloseGossipMenuFor(player);
        return false;
    }

    static void MenuLine(Player* player, std::string const& text, uint32 action)
    {
        AddGossipItemFor(player, GOSSIP_ICON_TALK, text, GossipSender, action);
    }

    void StartMenu(Player* player, ShoppingSession& session)
    {
        ClearGossipMenuFor(player);
        session.menuId = ++_lastMenuId;
        session.expires = std::chrono::steady_clock::now() + MenuLifetime;
        player->PlayerTalkClass->GetGossipMenu().SetMenuId(session.menuId);
    }

    static Item* SelectedItem(Player* player, ShoppingSession const& session)
    {
        if (session.slot >= EQUIPMENT_SLOT_END)
            return nullptr;
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, session.slot);
        return item && item->GetGUID() == session.itemGuid ? item : nullptr;
    }

    static void MaterialsLine(Player* player, ShoppingSession const& session)
    {
        MenuLine(player, session.useOwnMaterials ? "Use my materials: ON" : "Use my materials: OFF", ToggleMaterials);
    }

    void ShowEquipment(Player* player, ShoppingSession& session)
    {
        constexpr std::array<char const*, EQUIPMENT_SLOT_END> SlotNames = {
            "Head", "Neck", "Shoulders", "Shirt", "Chest", "Waist", "Legs", "Feet", "Wrists",
            "Hands", "Ring 1", "Ring 2", "Trinket 1", "Trinket 2", "Back", "Main hand", "Off hand", "Ranged", "Tabard"
        };
        session.slot = EQUIPMENT_SLOT_END;
        session.pending = nullptr;
        session.visibleOffers.clear();
        StartMenu(player, session);
        MaterialsLine(player, session);
        bool hasOffers = false;
        for (uint8 slot = 0; slot < EQUIPMENT_SLOT_END; ++slot)
            if (Item const* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot))
                if (std::ranges::any_of(Enchants.enchants, [&](Enchant const& enchant) { return IsCompatible(*player, *item, enchant); }))
                {
                    MenuLine(player, Trinity::StringFormat("{} - {}", SlotNames[slot],
                        item->GetNameForLocaleIdx(player->GetSession()->GetSessionDbLocaleIndex())), SelectSlot + slot);
                    hasOffers = true;
                }

        if (!hasOffers)
            MenuLine(player, "No compatible enchants for your equipped items.", NoAction);
        SendGossipMenuFor(player, EnchanterText, me);
    }

    void ShowEnchants(Player* player, ShoppingSession& session)
    {
        Item const* item = SelectedItem(player, session);
        if (!item)
        {
            ShowEquipment(player, session);
            return;
        }

        auto prices = MarketPrices.load();
        UnitPrices const& market = player->GetTeam() == ALLIANCE ? prices->alliance : prices->horde;
        session.pending = nullptr;
        session.visibleOffers.clear();
        std::vector<Enchant const*> available;
        std::unordered_map<uint32, std::size_t> offerByEnchant;
        for (Enchant const& enchant : Enchants.enchants)
            if (IsCompatible(*player, *item, enchant))
            {
                auto [entry, first] = offerByEnchant.try_emplace(enchant.effect->ID, available.size());
                if (first)
                    available.push_back(&enchant);
                else
                {
                    // Alternate recipes or reputation rewards can apply the same effect.
                    Enchant const*& choice = available[entry->second];
                    if (MakeQuote(*player, enchant, session.useOwnMaterials, market).total <
                        MakeQuote(*player, *choice, session.useOwnMaterials, market).total)
                        choice = &enchant;
                }
            }

        std::size_t const lastPage = available.empty() ? 0 : (available.size() - 1) / EnchantsPerPage;
        session.page = std::min(session.page, lastPage);
        StartMenu(player, session);
        MenuLine(player, Trinity::StringFormat("Enchants for {}", item->GetNameForLocaleIdx(player->GetSession()->GetSessionDbLocaleIndex())), NoAction);
        MaterialsLine(player, session);
        for (std::size_t i = session.page * EnchantsPerPage; i < std::min(available.size(), (session.page + 1) * EnchantsPerPage); ++i)
        {
            Enchant const& enchant = *available[i];
            Quote price = MakeQuote(*player, enchant, session.useOwnMaterials, market);
            MenuLine(player, Trinity::StringFormat("{} - {}", EnchantName(*player, *enchant.effect), MoneyText(price.total)),
                SelectOffer + session.visibleOffers.size());
            session.visibleOffers.push_back(&enchant);
        }

        if (session.page)
            MenuLine(player, "Previous page", PreviousPage);
        if (session.page < lastPage)
            MenuLine(player, "Next page", NextPage);
        if (available.empty())
            MenuLine(player, "No compatible enchants for this item.", NoAction);
        MenuLine(player, "Back to equipment", ShowGear);
        SendGossipMenuFor(player, EnchanterText, me);
    }

    void ShowQuote(Player* player, ShoppingSession& session, Enchant const& enchant)
    {
        Item const* item = SelectedItem(player, session);
        if (!item || !IsCompatible(*player, *item, enchant))
        {
            ShowEnchants(player, session);
            return;
        }

        auto prices = MarketPrices.load();
        UnitPrices const& market = player->GetTeam() == ALLIANCE ? prices->alliance : prices->horde;
        session.pending = &enchant;
        session.oldEnchant = item->GetEnchantmentId(enchant.slot);
        session.quote = MakeQuote(*player, enchant, session.useOwnMaterials, market);
        StartMenu(player, session);
        std::string confirm = Trinity::StringFormat("Apply {} for {}?", EnchantName(*player, *enchant.effect), MoneyText(session.quote.total));
        if (session.oldEnchant)
        {
            if (SpellItemEnchantmentEntry const* old = sSpellItemEnchantmentStore.LookupEntry(session.oldEnchant))
                confirm += Trinity::StringFormat("\nReplaces {}.", EnchantName(*player, *old));
            else
                confirm += "\nReplaces the existing enchant.";
        }

        MenuLine(player, Trinity::StringFormat("Labor: {} | Materials supplied by the enchanter: {}",
            MoneyText(session.quote.labor), MoneyText(session.quote.total - session.quote.labor)), NoAction);
        for (auto const& [itemId, count] : session.quote.consumes)
            if (ItemTemplate const* material = sObjectMgr->GetItemTemplate(itemId))
                MenuLine(player, Trinity::StringFormat("From your bags: {} x {}", count, material->GetName(player->GetSession()->GetSessionDbLocaleIndex())), NoAction);
        AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, Trinity::StringFormat("Apply for {}", MoneyText(session.quote.total)),
            GossipSender, BuyEnchant, confirm, 0, false);
        MenuLine(player, "Back to enchants", BackToEnchants);
        SendGossipMenuFor(player, EnchanterText, me);
    }

    void Purchase(Player* player, ShoppingSession& session)
    {
        Enchant const* enchant = session.pending;
        Item* item = SelectedItem(player, session);
        if (!enchant || !item || !IsCompatible(*player, *item, *enchant) ||
            item->GetEnchantmentId(enchant->slot) != session.oldEnchant)
        {
            Notice(player, "Your equipment changed. Please select an enchant again.");
            ShowEnchants(player, session);
            return;
        }

        auto prices = MarketPrices.load();
        UnitPrices const& market = player->GetTeam() == ALLIANCE ? prices->alliance : prices->horde;
        Quote quote = MakeQuote(*player, *enchant, session.useOwnMaterials, market);
        if (quote != session.quote)
        {
            Notice(player, "The price or available materials changed. Review the new quote.");
            ShowQuote(player, session, *enchant);
            return;
        }

        if (!player->HasEnoughMoney(quote.total))
        {
            Notice(player, "You do not have enough gold for this enchant.");
            return;
        }

        // These inventory changes run together on the player's map thread.
        player->ModifyMoney(-int32(quote.total));
        for (auto const& [itemId, count] : quote.consumes)
            player->DestroyItemCount(itemId, count, true);

        player->ApplyEnchantment(item, enchant->slot, false);
        item->SetEnchantment(enchant->slot, enchant->effect->ID, 0, 0, me->GetGUID());
        player->ApplyEnchantment(item, enchant->slot, true);
        player->RemoveTradeableItem(item);
        item->ClearSoulboundTradeable(player);
        if (enchant->effect->Flags & ENCHANTMENT_CAN_SOULBOUND)
            item->SetBinding(true);

        CharacterDatabaseTransaction trans = CharacterDatabase.BeginTransaction();
        player->SaveInventoryAndGoldToDB(trans);
        CharacterDatabase.CommitTransaction(trans);
        Notice(player, "Your enchant has been applied.");
        ShowEnchants(player, session);
    }

    uint32 _lastMenuId = 0;
    uint32 _cleanupTimer = 30000;
    std::unordered_map<ObjectGuid, ShoppingSession> _sessions;
};
}

void AddCapitalEnchanterScripts()
{
    new capital_enchanter_prices();
    RegisterCreatureAI(npc_capital_enchanter);
}
