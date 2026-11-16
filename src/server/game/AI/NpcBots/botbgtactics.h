#ifndef NPCBOT_BG_TACTICS_H
#define NPCBOT_BG_TACTICS_H

#include <cstdint>
#include <cstddef>
#include <cmath>

namespace NPCBots::SiegeBG
{
// Keep the commanded waypoint while traversing long edges. Reattaching to the
// nearest graph node mid-edge can repeatedly send the mover back to its origin.
constexpr bool KeepSiegeCheckpoint(float distance) { return distance >= 18.f; }

enum Gate : uint8_t { Green, Yellow, Blue, Red, Purple, Ancient, NoGate };
constexpr bool Broken(uint32_t mask, uint8_t gate) { return (mask & (1u << gate)) != 0; }
constexpr bool OuterOpen(uint32_t mask) { return Broken(mask, Green) || Broken(mask, Blue); }
constexpr bool MiddleOpen(uint32_t mask) { return OuterOpen(mask) && (Broken(mask, Red) || Broken(mask, Purple)); }
constexpr bool RelicOpen(uint32_t mask) { return MiddleOpen(mask) && Broken(mask, Yellow) && Broken(mask, Ancient); }
constexpr bool SACaptureScout(uint32_t botId) { return botId % 5 == 4; }
constexpr bool SADefenderGunner(uint32_t botId) { return botId % 5 == 1 || botId % 5 == 2; }
constexpr uint8_t SACannonIndex(Gate gate, uint32_t botId)
{
    constexpr uint8_t base[] = {0, 2, 4, 6, 8};
    return gate < Ancient ? uint8_t(base[gate] + (botId % 5 == 2 ? 1 : 0)) : 10;
}

constexpr Gate AssaultGate(uint32_t mask, bool west)
{
    if (!OuterOpen(mask)) return west ? Green : Blue;
    if (!MiddleOpen(mask)) return west ? Purple : Red;
    if (!Broken(mask, Yellow)) return Yellow;
    if (!Broken(mask, Ancient)) return Ancient;
    return NoGate;
}

constexpr bool SAEdgeOpen(uint32_t from, uint32_t to, uint32_t mask)
{
    if (from > to) { uint32_t temp = from; from = to; to = temp; }
    if (from == 5205 && to == 5221) return Broken(mask, Green);
    if (from == 5207 && to == 5222) return Broken(mask, Blue);
    if (from == 5211 && to == 5223) return Broken(mask, Purple);
    if (from == 5213 && to == 5224) return Broken(mask, Red);
    if (from == 5217 && to == 5225) return Broken(mask, Yellow);
    if (from == 5219 && to == 5226) return Broken(mask, Ancient);
    return true;
}

constexpr bool SANodeAccessible(uint32_t id, uint32_t mask)
{
    if (id < 5200 || id > 5226) return false;
    if (id <= 5207) return true;
    if (id == 5221) return Broken(mask, Green);
    if (id == 5222) return Broken(mask, Blue);
    if (id == 5208 || id == 5209 || id == 5210 || id == 5211 || id == 5212 || id == 5213) return OuterOpen(mask);
    if (id == 5223) return MiddleOpen(mask) && Broken(mask, Purple);
    if (id == 5224) return MiddleOpen(mask) && Broken(mask, Red);
    if (id == 5214 || id == 5215 || id == 5216 || id == 5217) return MiddleOpen(mask);
    if (id == 5218 || id == 5219 || id == 5225) return MiddleOpen(mask) && Broken(mask, Yellow);
    if (id == 5220 || id == 5226) return RelicOpen(mask);
    return false;
}

// IC gate bits use ICDoorList: Horde front/west/east, then Alliance front/west/east.
constexpr bool ICEdgeOpen(uint32_t from, uint32_t to, uint8_t /*team*/, uint32_t mask)
{
    if (from > to) { uint32_t temp = from; from = to; to = temp; }
    if (from == 5300 && to >= 5301 && to <= 5303)
        return Broken(mask, uint8_t(to - 5301 + 3));
    if (from == 5305 && to >= 5306 && to <= 5308)
        return Broken(mask, uint8_t(to - 5306));
    return true;
}

// Vehicle passengers are rooted by the core. That root must not hand control
// back to owner-following vehicle AI (which requires a real player owner).
constexpr bool UseOwnerVehicleAI(bool freeBot, bool aboard) { return aboard && !freeBot; }
constexpr bool SiegeControlBlocked(bool aboard, bool casting, bool crowdControlled)
{
    return casting || (!aboard && crowdControlled);
}

// NORMAL (1) and a navigable INCOMPLETE prefix (4) are valid. The 74-polygon
// corridor limit requires following long roads in successive prefixes.
// Combined flags indicate shortcuts, missing tiles or far-from-poly endpoints.
constexpr bool VerifiedSiegePath(uint32_t type, std::size_t points) { return (type == 1 || type == 4) && points >= 2; }

constexpr uint8_t ICNodeKeep(uint32_t id)
{
    if (id == 5300 || id == 5304 || id == 5321) return 0;
    if (id == 5305 || id == 5309 || id == 5322) return 1;
    return 2;
}

// Courtyard boundaries from the six native IC gate positions. Boundary
// checkpoints belong to the exterior, not to an already-entered courtyard.
constexpr uint8_t ICPositionKeep(float x, float y)
{
    if (x >= 200.f && x < 412.98f && y > -902.83f && y < -765.08f) return 0;
    if (x > 1151.4f && x <= 1350.f && y > -850.65f && y < -677.45f) return 1;
    return 2;
}

inline bool ICSegmentOpen(float x1, float y1, float x2, float y2, uint32_t mask)
{
    struct Wall { bool vertical; float plane, low, high, gate; };
    constexpr Wall walls[] = {
        {true, 1150.9f, -851.15f, -676.95f, -762.61f},
        {false, -851.15f, 1150.9f, 1350.f, 1218.74f},
        {false, -676.95f, 1150.9f, 1350.f, 1217.9f},
        {true, 413.48f, -903.33f, -764.58f, -833.95f},
        {false, -764.58f, 200.f, 413.48f, 353.44f},
        {false, -903.33f, 200.f, 413.48f, 351.02f}
    };
    for (uint8_t i = 0; i < 6; ++i)
    {
        auto const& wall = walls[i];
        float a = wall.vertical ? x1 : y1, b = wall.vertical ? x2 : y2;
        if (std::fabs(b - a) < 0.001f) continue;
        float t = (wall.plane - a) / (b - a);
        if (t < 0.f || t > 1.f) continue;
        float cross = wall.vertical ? y1 + t * (y2 - y1) : x1 + t * (x2 - x1);
        if (cross < wall.low || cross > wall.high) continue;
        if (!Broken(mask, i) || std::fabs(cross - wall.gate) > 18.f) return false;
    }
    return true;
}

constexpr uint8_t ICSquad(uint32_t botId) { return uint8_t((botId % 40) / 5); }
constexpr uint8_t ICPrimaryNode(uint8_t squad, uint8_t team)
{
    // ICNodePointType: refinery=0, quarry=1, docks=2, hangar=3, workshop=4.
    constexpr uint8_t objectives[] = {4, 2, 3, 1, 0, 4, 2, 4};
    uint8_t node = objectives[squad % 8];
    return team == 1 && (node == 0 || node == 1) ? uint8_t(1 - node) : node;
}
}

#endif
