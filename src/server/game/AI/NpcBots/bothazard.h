#ifndef NPCBOT_HAZARD_H
#define NPCBOT_HAZARD_H

#include <algorithm>
#include <cmath>
#include <vector>

namespace NPCBots::Hazard
{
struct Point { float x, y; };
struct Circle { Point center; float radius; };
inline float Distance(Point a, Point b) { return std::hypot(a.x - b.x, a.y - b.y); }
inline bool Contains(Circle const& c, Point p) { return Distance(c.center, p) < c.radius; }
inline bool Unsafe(Point p, std::vector<Circle> const& circles)
{
    return std::any_of(circles.begin(), circles.end(), [p](Circle const& c) { return Contains(c, p); });
}

// Permit leaving a circle containing the start, never crossing another pool or
// moving deeper into the pool being escaped. Applied to every navigation segment.
inline bool SafeSegment(Point from, Point to, std::vector<Circle> const& circles)
{
    float dx = to.x - from.x, dy = to.y - from.y;
    float length2 = dx * dx + dy * dy;
    if (length2 < 0.0001f)
        return true;
    for (Circle const& c : circles)
    {
        float projection = ((c.center.x - from.x) * dx + (c.center.y - from.y) * dy) / length2;
        if (Contains(c, from))
        {
            if (projection > 0.000001f)
                return false;
        }
        else
        {
            float t = std::clamp(projection, 0.0f, 1.0f);
            if (Distance(c.center, {from.x + t * dx, from.y + t * dy}) < c.radius)
                return false;
        }
    }
    return true;
}

inline std::vector<Point> EscapeCandidates(Point start, float orientation, std::vector<Circle> const& circles)
{
    std::vector<Point> result;
    if (!Unsafe(start, circles))
        return result;
    constexpr float Pi = 3.14159265358979323846f;
    // Prefer a five metre lateral step. Large/growing pools require a longer exit.
    for (unsigned i = 0; i < 32; ++i)
    {
        float angle = orientation + Pi / 2 + i * Pi / 16;
        float dx = std::cos(angle), dy = std::sin(angle);
        float distance = 5.0f;
        for (size_t pass = 0; pass <= circles.size(); ++pass)
        {
            bool extended = false;
            for (Circle const& c : circles)
            {
                Point end{start.x + dx * distance, start.y + dy * distance};
                Circle padded{c.center, c.radius + 0.75f};
                if (!Contains(padded, end))
                    continue;
                float x = start.x - c.center.x, y = start.y - c.center.y;
                float b = x * dx + y * dy;
                float discriminant = b * b - x * x - y * y + padded.radius * padded.radius;
                if (discriminant >= 0)
                {
                    distance = std::max(distance, -b + std::sqrt(discriminant) + 0.05f);
                    extended = true;
                }
            }
            if (!extended)
                break;
        }
        Point end{start.x + dx * distance, start.y + dy * distance};
        if (distance <= 40.0f && !Unsafe(end, circles) && SafeSegment(start, end, circles))
            result.push_back(end);
    }
    std::stable_sort(result.begin(), result.end(), [start](Point a, Point b) { return int(Distance(start, a) * 1000 + 0.5f) < int(Distance(start, b) * 1000 + 0.5f); });
    return result;
}
}
#endif
