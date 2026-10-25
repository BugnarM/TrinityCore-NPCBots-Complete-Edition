#ifndef NPCBOT_DYNAMIC_ZONE_POLICY_H
#define NPCBOT_DYNAMIC_ZONE_POLICY_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace NPCBots::DynamicZone
{
// Shared by runtime and offline tests. Zone IDs alone do not identify a map.
constexpr std::uint64_t ZoneKey(std::uint32_t map, std::uint32_t zone)
{
    return (std::uint64_t(map) << 32) | zone;
}

struct LevelRange
{
    std::uint8_t min;
    std::uint8_t max;
};

constexpr std::optional<LevelRange> SpawnLevels(unsigned min, unsigned max, unsigned classMinimum, unsigned cap = 80)
{
    min = std::max({min, classMinimum, 1u});
    max = std::min(max, cap);
    if (min > max || max > 255) return std::nullopt;
    return LevelRange{std::uint8_t(min), std::uint8_t(max)};
}

constexpr bool InScope(std::uint32_t map, std::uint32_t zone, std::uint32_t homeMap, std::uint32_t homeZone)
{
    return !homeZone || (map == homeMap && zone == homeZone);
}

// Share a global budget evenly, independently of unordered-map iteration order.
constexpr unsigned ZoneTarget(std::size_t index, std::size_t zones, unsigned perZone, unsigned global)
{
    if (!zones || index >= zones) return 0;
    return std::min<std::size_t>(perZone, global / zones + (index < global % zones));
}

constexpr unsigned Normalize(int value, unsigned min, unsigned max)
{
    return unsigned(std::clamp(value, int(min), int(max)));
}
}
#endif
