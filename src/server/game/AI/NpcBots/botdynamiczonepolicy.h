#ifndef NPCBOT_DYNAMIC_ZONE_POLICY_H
#define NPCBOT_DYNAMIC_ZONE_POLICY_H

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <tuple>
#include <vector>

class WanderNode;

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

constexpr bool CanTravelAtLevel(unsigned level, unsigned minimum, unsigned maximum)
{
    return level + 2 >= minimum && level <= maximum;
}

constexpr std::optional<LevelRange> TravelLevelOverlap(LevelRange source, unsigned minimum, unsigned maximum)
{
    return SpawnLevels(source.min, std::min<unsigned>(source.max, maximum), minimum > 2 ? minimum - 2 : 1, source.max);
}

constexpr bool OverlappingTravelLevels(LevelRange source, unsigned minimum, unsigned maximum)
{
    return TravelLevelOverlap(source, minimum, maximum).has_value();
}

inline auto EligibleSpawnLevels(std::vector<std::uint8_t> const& routeLevels, LevelRange range, unsigned targetLevel = 0)
{
    auto const first = std::lower_bound(routeLevels.begin(), routeLevels.end(), targetLevel ? std::max<unsigned>(targetLevel, range.min) : range.min);
    auto const last = std::upper_bound(first, routeLevels.end(), targetLevel ? std::min<unsigned>(targetLevel, range.max) : range.max);
    return std::pair{first, last};
}

std::vector<std::uint8_t> GetSpawnRouteLevels(WanderNode const& node, std::uint32_t spawnFaction, std::uint32_t movementFaction);

constexpr auto SpawnPriority(unsigned nearbyBots, bool markedSpawn, float playerDistance)
{
    // Spread newcomers before reusing an anchor. The nearest road remains an option.
    return std::tuple{nearbyBots, !markedSpawn, playerDistance};
}

constexpr bool InScope(std::uint32_t map, std::uint32_t zone, std::uint32_t homeMap, std::uint32_t homeZone)
{
    return !homeZone || (map == homeMap && zone == homeZone);
}

// Return the actual elapsed time between scans, including the current tick.
inline std::optional<std::uint32_t> ScanElapsed(std::uint32_t& accumulated, std::uint32_t diff, std::uint32_t interval)
{
    auto const elapsed = std::uint64_t(accumulated) + diff;
    if (elapsed < interval)
    {
        accumulated = std::uint32_t(elapsed);
        return std::nullopt;
    }
    accumulated = 0;
    return std::uint32_t(std::min<std::uint64_t>(elapsed, UINT32_MAX));
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
