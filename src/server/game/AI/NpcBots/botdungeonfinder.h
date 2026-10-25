#ifndef NPCBOT_DUNGEON_FINDER_H
#define NPCBOT_DUNGEON_FINDER_H

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace NPCBots::DungeonFinder
{
// These bits match LfgRoles; the core checks the mapping with static_assert.
inline constexpr std::uint8_t Tank = 2, Healer = 4, Damage = 8;
using RoleCounts = std::array<std::uint8_t, 3>;
inline constexpr RoleCounts Capacity{1, 1, 3};
inline constexpr std::array<std::uint8_t, 3> Roles{Tank, Healer, Damage};

constexpr std::uint32_t NormalizeWait(int seconds)
{
    return seconds < 0 ? 45u : std::min<std::uint32_t>(seconds, 3600u);
}
constexpr bool CanFill(std::int64_t now, std::int64_t joined, std::uint32_t delay,
    std::size_t members, bool enabled, bool queued)
{
    return enabled && queued && members > 0 && members < 5 && joined >= 0 && now >= joined &&
        (static_cast<std::uint64_t>(now) - static_cast<std::uint64_t>(joined)) >= delay;
}
constexpr std::uint32_t NextPlaceholderCounter(std::uint32_t previous)
{
    return previous >= 0xFFFFFFu ? 1u : previous + 1u;
}

inline std::optional<RoleCounts> MissingRoles(std::vector<std::uint8_t> const& assigned)
{
    if (assigned.empty() || assigned.size() > 5)
        return std::nullopt;
    RoleCounts missing = Capacity;
    for (auto role : assigned)
    {
        role &= Tank | Healer | Damage; // Ignore the leader bit.
        auto position = std::find(Roles.begin(), Roles.end(), role);
        if (position == Roles.end())
            return std::nullopt;
        auto index = std::size_t(position - Roles.begin());
        if (!missing[index])
            return std::nullopt;
        --missing[index];
    }
    return missing;
}

struct BotSlot
{
    std::size_t candidate;
    std::uint8_t role;
};

// Bipartite matching prevents a flexible tank/healer from consuming the only healer slot
// when a dedicated tank is available. No templates are consumed until the full plan exists.
inline std::optional<std::vector<BotSlot>> PlanBots(std::vector<std::uint8_t> const& candidates, RoleCounts const& needed)
{
    std::vector<std::uint8_t> slots;
    for (std::size_t role = 0; role < needed.size(); ++role)
    {
        if (needed[role] > Capacity[role])
            return std::nullopt;
        slots.insert(slots.end(), needed[role], Roles[role]);
    }
    if (slots.size() > candidates.size())
        return std::nullopt;
    std::vector<int> assigned(candidates.size(), -1);
    std::vector<bool> visited;
    std::function<bool(std::size_t)> match = [&](std::size_t slot) {
        for (std::size_t bot = 0; bot < candidates.size(); ++bot)
        {
            if (visited[bot] || !(candidates[bot] & slots[slot]))
                continue;
            visited[bot] = true;
            if (assigned[bot] < 0 || match(std::size_t(assigned[bot])))
            {
                assigned[bot] = int(slot);
                return true;
            }
        }
        return false;
    };
    for (std::size_t slot = 0; slot < slots.size(); ++slot)
    {
        visited.assign(candidates.size(), false);
        if (!match(slot))
            return std::nullopt;
    }
    std::vector<BotSlot> result;
    for (std::size_t bot = 0; bot < assigned.size(); ++bot)
        if (assigned[bot] >= 0)
            result.push_back({bot, slots[std::size_t(assigned[bot])]});
    return result;
}
}

#endif
