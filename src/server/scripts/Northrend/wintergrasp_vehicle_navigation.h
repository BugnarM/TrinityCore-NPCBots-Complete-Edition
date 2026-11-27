#ifndef WINTERGRASP_VEHICLE_NAVIGATION_H
#define WINTERGRASP_VEHICLE_NAVIGATION_H

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <initializer_list>

namespace WintergraspBotWar::Navigation
{
// PathGenerator flags: reject shortcuts, absent tiles, off-mesh endpoints and
// forced destinations. Allow FARFROMPOLY_END (0x80) so navigable bridge/ramp
// and gate approaches follow genuine navmesh prefixes and replan smoothly.
constexpr bool VerifiedPath(std::uint32_t type, std::size_t points)
{
    std::uint32_t const base = type & ~0x80u;
    return (base == 1 || base == 4) && points >= 2;
}

// End-height hints do not invalidate a real navmesh prefix. Ground spawns
// are projected before creation; invalid starts and shortcuts remain rejected.
constexpr bool VerifiedInfantryPath(std::uint32_t type, std::size_t points)
{
    return VerifiedPath(type & ~0x80u, points);
}

template<class Point, class RayClear>
bool SegmentClear(Point const& a, Point const& b, float radius, float height, RayClear const& clear)
{
    if (!std::isfinite(a.x) || !std::isfinite(a.y) || !std::isfinite(a.z) ||
        !std::isfinite(b.x) || !std::isfinite(b.y) || !std::isfinite(b.z))
        return false;
    float dx = b.x - a.x, dy = b.y - a.y;
    float length = std::hypot(dx, dy);
    if (length < 0.01f)
        return std::fabs(a.z - b.z) < 1.f;
    radius = std::clamp(radius, 0.25f, 2.5f);
    float zOffset = std::clamp(height * 0.5f, 0.75f, 2.f);
    for (float side : {0.f, -1.f, 1.f})
    {
        float ox = -dy / length * radius * side, oy = dx / length * radius * side;
        if (!clear(a.x + ox, a.y + oy, a.z + zOffset, b.x + ox, b.y + oy, b.z + zOffset))
            return false;
    }
    return true;
}

template<class Points, class RayClear>
bool PathClear(Points const& points, float radius, float height, RayClear const& clear)
{
    if (points.size() < 2)
        return false;
    for (std::size_t i = 1; i < points.size(); ++i)
        if (!SegmentClear(points[i - 1], points[i], radius, height, clear))
            return false;
    return true;
}

struct Detour
{
    float Angle;
    float Distance;
};

inline Detour RecoveryDetour(std::uint32_t attempt)
{
    constexpr float angles[] = {0.f, 0.7853982f, -0.7853982f, 1.5707963f,
        -1.5707963f, 2.3561945f, -2.3561945f, 3.1415927f};
    std::uint32_t const ring = attempt % 24;
    float const distance = ring < 8 ? 64.f : (ring < 16 ? 24.f : 12.f);
    return {angles[attempt % 8], distance};
}
}

#endif
