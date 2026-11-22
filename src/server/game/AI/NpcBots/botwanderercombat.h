/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef NPCBOT_WANDERER_COMBAT_H
#define NPCBOT_WANDERER_COMBAT_H

#include <algorithm>
#include <cstddef>
#include <optional>
#include <span>

namespace NPCBots::WandererCombat
{
struct Settings
{
    unsigned playerTargetBias = 35;
    bool prioritizeBotsOverPlayers = true;
    bool spawnTruceProtectsPlayers = true;
    float tankInterceptRange = 30.f;
};

Settings const& GetSettings();

constexpr float DistanceBand = 8.f;
constexpr float FormationCredit = 12.f;

enum class Kind { Other, Player, Companion, Tank, OpposingWanderer };

struct Candidate
{
    Kind kind;
    float distance;
    float formationCredit;
    unsigned armor;
};

inline bool IsCompanion(Kind kind)
{
    return kind == Kind::Companion || kind == Kind::Tank;
}

inline double TargetWeight(Candidate const& target, unsigned attackerLevel)
{
    double const armor = target.armor;
    double const armorWeight = 1.0 + armor / (armor + 400.0 + 85.0 * attackerLevel);
    return (target.kind == Kind::Tank ? 3.0 : 1.0) * armorWeight;
}

// partyRoll is uniform in [0, 99]. weightRoll is uniform in [0, 1).
inline std::optional<std::size_t> PickTarget(std::span<Candidate const> targets, Settings const& settings,
    unsigned attackerLevel, unsigned partyRoll, double weightRoll)
{
    if (targets.empty())
        return std::nullopt;

    Candidate const& nearest = *std::ranges::min_element(targets, {}, &Candidate::distance);
    float const limit = nearest.distance + DistanceBand;
    auto eligible = [limit](Candidate const& target)
    {
        float const credit = IsCompanion(target.kind) ? std::clamp(target.formationCredit, 0.f, FormationCredit) : 0.f;
        return target.distance - credit <= limit;
    };

    bool players = false, companions = false, wanderers = false;
    for (Candidate const& target : targets)
        if (eligible(target))
        {
            players |= target.kind == Kind::Player;
            companions |= IsCompanion(target.kind);
            wanderers |= target.kind == Kind::OpposingWanderer;
        }

    // Keep ordinary PvE encounters in their original target category.
    Kind pool = nearest.kind;
    if (pool != Kind::Other && pool != Kind::OpposingWanderer)
    {
        if (wanderers && settings.prioritizeBotsOverPlayers)
            pool = Kind::OpposingWanderer;
        else if (players && (!companions || partyRoll < settings.playerTargetBias))
            pool = Kind::Player;
        else
            pool = Kind::Companion;
    }

    auto inPool = [&](Candidate const& target)
    {
        return eligible(target) && (IsCompanion(pool) ? IsCompanion(target.kind) : target.kind == pool);
    };

    double totalWeight = 0.0;
    for (Candidate const& target : targets)
        if (inPool(target))
            totalWeight += TargetWeight(target, attackerLevel);

    double ticket = std::clamp(weightRoll, 0.0, 1.0) * totalWeight;
    std::size_t last = std::size_t(&nearest - targets.data());
    for (std::size_t i = 0; i < targets.size(); ++i)
        if (inPool(targets[i]))
        {
            last = i;
            ticket -= TargetWeight(targets[i], attackerLevel);
            if (ticket < 0.0)
                return i;
        }
    return last;
}
}

#endif
