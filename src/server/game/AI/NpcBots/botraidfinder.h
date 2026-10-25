#ifndef NPCBOT_RAID_FINDER_H
#define NPCBOT_RAID_FINDER_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace NPCBots::RaidFinder
{
inline constexpr uint8_t Tank = 2, Healer = 4, Damage = 8;
using RoleCounts = std::array<uint8_t, 3>;
inline constexpr std::array<uint8_t, 3> Roles{Tank, Healer, Damage};
inline constexpr RoleCounts Capacity10{2, 3, 5}, Capacity25{2, 6, 17};
constexpr bool ValidSize(uint8_t size) { return size == 10 || size == 25; }
constexpr RoleCounts GetCapacity(uint8_t size) { return size == 25 ? Capacity25 : size == 10 ? Capacity10 : RoleCounts{}; }
constexpr uint32_t NormalizeWait(int seconds) { return seconds < 0 ? 30u : std::min<uint32_t>(seconds, 3600u); }
constexpr bool Ready(int64_t now, int64_t joined, uint32_t wait, size_t members, uint8_t size)
{
    return ValidSize(size) && members > 0 && members <= size &&
        (members == size || (now >= joined && uint64_t(now) - uint64_t(joined) >= wait));
}
struct RaidDefinition
{
    uint32_t id;
    std::string key, name;
    uint32_t mapId;
    uint8_t tier, minLevel;
    uint16_t minItemLevel;
};
inline std::array<RaidDefinition, 9> const& GetRaids()
{
    static const std::array<RaidDefinition, 9> raids{{
        {1, "naxx", "Naxxramas", 533, 7, 80, 160},
        {2, "os", "The Obsidian Sanctum", 615, 7, 80, 160},
        {3, "eoe", "The Eye of Eternity", 616, 7, 80, 160},
        {4, "voa", "Vault of Archavon", 624, 8, 80, 187},
        {5, "ulduar", "Ulduar", 603, 8, 80, 187},
        {6, "totc", "Trial of the Crusader", 649, 9, 80, 200},
        {7, "ony", "Onyxia's Lair", 249, 9, 80, 200},
        {8, "icc", "Icecrown Citadel", 631, 10, 80, 219},
        {9, "rs", "The Ruby Sanctum", 724, 10, 80, 219}
    }};
    return raids;
}
inline RaidDefinition const* FindRaid(std::string_view key)
{
    std::string lower(key);
    for (char& c : lower) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
    if (lower.empty()) return nullptr;
    if (lower == "toc") lower = "totc";
    if (lower == "onyxia") lower = "ony";
    RaidDefinition const* found = nullptr;
    for (auto const& raid : GetRaids())
    {
        if (raid.key == lower) return &raid;
        std::string name = raid.name;
        for (char& c : name) if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        if (name.find(lower) != std::string::npos)
        {
            if (found) return nullptr; // Ambiguous names must never queue the wrong raid.
            found = &raid;
        }
    }
    return found;
}
inline RaidDefinition const* FindRaidByMap(uint32_t map)
{
    for (auto const& raid : GetRaids()) if (raid.mapId == map) return &raid;
    return nullptr;
}
inline RaidDefinition const* FindRaidById(uint32_t id)
{
    for (auto const& raid : GetRaids()) if (raid.id == id) return &raid;
    return nullptr;
}
inline std::optional<RoleCounts> MissingRoles(std::vector<uint8_t> const& assigned, uint8_t size)
{
    if (!ValidSize(size) || assigned.size() > size) return std::nullopt;
    RoleCounts missing = GetCapacity(size);
    for (uint8_t role : assigned)
    {
        role &= Tank | Healer | Damage;
        auto it = std::find(Roles.begin(), Roles.end(), role);
        if (it == Roles.end() || !missing[size_t(it - Roles.begin())]) return std::nullopt;
        --missing[size_t(it - Roles.begin())];
    }
    return missing;
}
// Assign every human/bot exactly one allowed role; never relabel surplus tanks as DPS.
inline std::optional<std::vector<uint8_t>> AssignRoles(std::vector<uint8_t> const& masks, uint8_t size)
{
    if (!ValidSize(size) || masks.size() > size) return std::nullopt;
    std::vector<uint8_t> slots;
    auto capacity = GetCapacity(size);
    for (size_t i = 0; i < Roles.size(); ++i) slots.insert(slots.end(), capacity[i], Roles[i]);
    std::vector<int> owners(slots.size(), -1);
    std::vector<bool> visited;
    std::function<bool(size_t)> assign = [&](size_t person) {
        for (size_t slot = 0; slot < slots.size(); ++slot)
            if (!visited[slot] && (masks[person] & slots[slot]))
            {
                visited[slot] = true;
                if (owners[slot] < 0 || assign(size_t(owners[slot]))) { owners[slot] = int(person); return true; }
            }
        return false;
    };
    for (size_t person = 0; person < masks.size(); ++person)
    {
        visited.assign(slots.size(), false);
        if (!assign(person)) return std::nullopt;
    }
    std::vector<uint8_t> result(masks.size());
    for (size_t slot = 0; slot < slots.size(); ++slot) if (owners[slot] >= 0) result[size_t(owners[slot])] = slots[slot];
    return result;
}
struct BotSlot { size_t candidate; uint8_t role; };
inline std::optional<std::vector<BotSlot>> PlanRaidBots(std::vector<uint8_t> const& candidates, RoleCounts const& needed)
{
    std::vector<uint8_t> slots;
    for (size_t i = 0; i < Roles.size(); ++i) slots.insert(slots.end(), needed[i], Roles[i]);
    if (slots.size() > 25 || slots.size() > candidates.size()) return std::nullopt;
    std::vector<int> assigned(candidates.size(), -1);
    std::vector<bool> visited;
    std::function<bool(size_t)> match = [&](size_t slot) {
        for (size_t bot = 0; bot < candidates.size(); ++bot)
            if (!visited[bot] && (candidates[bot] & slots[slot]))
            {
                visited[bot] = true;
                if (assigned[bot] < 0 || match(size_t(assigned[bot]))) { assigned[bot] = int(slot); return true; }
            }
        return false;
    };
    for (size_t slot = 0; slot < slots.size(); ++slot)
    {
        visited.assign(candidates.size(), false);
        if (!match(slot)) return std::nullopt;
    }
    std::vector<BotSlot> result;
    for (size_t bot = 0; bot < assigned.size(); ++bot) if (assigned[bot] >= 0) result.push_back({bot, slots[size_t(assigned[bot])]});
    return result;
}
struct QueueUnit { uint32_t team; std::vector<uint8_t> roles; size_t humans{}; };
struct Batch { std::vector<size_t> units; std::vector<uint8_t> roles; size_t humans{}; };
inline std::optional<Batch> SelectBatch(std::vector<QueueUnit> const& queue, size_t seed, uint8_t size)
{
    if (!ValidSize(size) || seed >= queue.size() || queue[seed].roles.empty()) return std::nullopt;
    RoleCounts capacity = GetCapacity(size);
    auto options = [&](std::vector<uint8_t> const& masks) {
        std::map<RoleCounts, std::vector<uint8_t>> choices{{RoleCounts{}, {}}};
        for (uint8_t mask : masks)
        {
            std::map<RoleCounts, std::vector<uint8_t>> next;
            for (auto const& [counts, assigned] : choices)
                for (size_t role = 0; role < Roles.size(); ++role)
                    if ((mask & Roles[role]) && counts[role] < capacity[role])
                    {
                        auto count = counts;
                        ++count[role];
                        if (next.contains(count)) continue;
                        auto roles = assigned;
                        roles.push_back(Roles[role]);
                        next.emplace(count, std::move(roles));
                    }
            choices = std::move(next);
        }
        return choices;
    };
    std::map<RoleCounts, Batch> states;
    for (auto& [counts, roles] : options(queue[seed].roles)) states.emplace(counts, Batch{{seed}, std::move(roles), queue[seed].humans ? queue[seed].humans : queue[seed].roles.size()});
    // At most 378 states for a 25-player raid; premades are indivisible units.
    // Maximize real queued members instead of greedily consuming scarce role slots.
    for (size_t i = 0; i < queue.size(); ++i)
    {
        if (i == seed || queue[i].team != queue[seed].team || queue[i].roles.empty()) continue;
        auto choices = options(queue[i].roles);
        auto next = states;
        for (auto const& [counts, batch] : states)
            for (auto const& [add, roles] : choices)
            {
                RoleCounts total{};
                bool fits = true;
                for (size_t r = 0; r < Roles.size(); ++r)
                {
                    unsigned sum = unsigned(counts[r]) + add[r];
                    if (sum > capacity[r]) { fits = false; break; }
                    total[r] = uint8_t(sum);
                }
                if (!fits) continue;
                size_t humans = batch.humans + (queue[i].humans ? queue[i].humans : queue[i].roles.size());
                auto previous = next.find(total);
                if (previous != next.end() && previous->second.humans >= humans) continue;
                Batch merged = batch;
                merged.humans = humans;
                merged.units.push_back(i);
                merged.roles.insert(merged.roles.end(), roles.begin(), roles.end());
                next.insert_or_assign(total, std::move(merged));
            }
        states = std::move(next);
    }
    std::optional<Batch> best;
    for (auto& [counts, batch] : states)
        if (!best || batch.humans > best->humans || (batch.humans == best->humans && batch.roles.size() > best->roles.size())) best = std::move(batch);
    return best;
}
}
#endif
