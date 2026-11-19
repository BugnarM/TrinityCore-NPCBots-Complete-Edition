#ifndef NPCBOT_HEALING_POLICY_H
#define NPCBOT_HEALING_POLICY_H

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <tuple>

namespace NPCBots::Healing
{
struct Settings
{
    bool topOff = true, playerAid = true, allowDuelAid = false, allowStealthedAid = false;
    bool diagnostics = false, aoeLoot = true, heirlooms = true;
    unsigned topOffPct = 95, minManaPct = 25, ownerEmergencyPct = 60, groupEmergencyPct = 50;
    unsigned idleScanMs = 750, combatScanMs = 250, retryMs = 750;
    unsigned bonesSeconds = 1800, reviveMs = 180000, companionCooldownSeconds = 18000;
    unsigned maxCorpses = 60;
    float aidRange = 40.f, lootRange = 120.f, ghostSpeed = 3.f, defenderLeash = 65.f, bossLeash = 90.f;
};
inline float FiniteClamp(float value, float fallback, float low, float high)
{
    return std::isfinite(value) ? std::clamp(value, low, high) : fallback;
}
struct DecisionMetrics
{
    static constexpr std::array<std::uint64_t, 8> bounds{10, 50, 100, 250, 500, 1000, 5000, UINT64_MAX};
    std::array<std::uint64_t, 8> histogram{};
    std::uint64_t count{}, maximum{};
    void Record(std::uint64_t microseconds)
    {
        ++count;
        maximum = std::max(maximum, microseconds);
        for (unsigned i = 0; i < bounds.size(); ++i)
            if (microseconds <= bounds[i]) { ++histogram[i]; break; }
    }
    std::uint64_t Percentile(unsigned percent) const
    {
        std::uint64_t cumulative = 0;
        for (unsigned i = 0; i < bounds.size(); ++i)
        {
            cumulative += histogram[i];
            if (count && cumulative * 100 >= count * percent) return std::min(bounds[i], maximum);
        }
        return 0;
    }
};
class DecisionScope
{
    DecisionMetrics* metrics;
    std::chrono::steady_clock::time_point start;
public:
    explicit DecisionScope(DecisionMetrics* value) : metrics(value), start(value ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{}) {}
    ~DecisionScope()
    {
        if (metrics)
            metrics->Record(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count());
    }
};
struct Projection
{
    std::int32_t health, missing, percent;
    double percentPerSecond;
};
inline Projection Project(std::uint32_t health, std::uint32_t maximum, double hps, double seconds)
{
    double const limit = std::numeric_limits<std::int32_t>::max();
    double const predicted = std::clamp(double(health) + hps * seconds, -limit, limit);
    return {std::int32_t(predicted), std::int32_t(std::clamp(double(maximum) - predicted, 0., limit)),
        std::int32_t(maximum ? std::clamp(predicted * 100. / maximum, -limit, limit) : 100.),
        maximum ? hps * 100. / maximum : 0.};
}
inline unsigned Tier(unsigned hp) { return hp <= 35 ? 3 : hp <= 60 ? 2 : hp <= 85 ? 1 : 0; }
inline auto Priority(unsigned hp, bool owner, bool player, bool pointed, bool tank, bool pet, bool attacked)
{
    // Emergency band always wins over identity. Preferences apply within a band.
    int const score = int(100 - std::min(hp, 100u)) + 100 * owner + 80 * player +
        150 * pointed + 50 * tank - 40 * pet + 20 * attacked;
    return std::tuple{Tier(hp), score};
}
inline bool StartTopOff(bool enabled, bool player, bool casterCombat, bool targetCombat,
    unsigned hp, unsigned personal, unsigned global)
{
    return enabled && player && !casterCombat && !targetCombat && hp < std::min(personal, global);
}
inline bool CancelDirectHeal(bool idlePlayer, unsigned hp, unsigned personal, unsigned topOff)
{
    return idlePlayer ? hp >= std::min(personal, topOff) : hp > 90;
}
}
#endif
