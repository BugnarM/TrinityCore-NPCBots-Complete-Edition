#ifndef NPCBOT_AUTOEQUIP_H
#define NPCBOT_AUTOEQUIP_H

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace NPCBots::AutoEquip
{
enum class Kind { Level, Blue, T7, T8, T9, T10, Wrathful, ItemLevel, Organic };

struct Preset
{
    Kind kind;
    uint32_t minimum, maximum;
    std::string name;
    uint32_t targetTier = 0; // 0 for level-based, or 7, 8, 9, 10
    bool IsPvP() const { return kind == Kind::Wrathful; }
    bool RequiresSet() const { return kind >= Kind::T7 && kind <= Kind::Wrathful; }
    bool IsOrganic() const { return kind == Kind::Organic; }
};

inline std::optional<Preset> Parse(std::string_view input, uint8_t level)
{
    std::string arg(input);
    for (char& c : arg)
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';

    auto levelPreset = [level](Kind kind, std::string const& label) -> Preset {
        if (level >= 80)
            return Preset{kind, 187, 219, label + " T7", 7};
        uint32_t cap = 0, min = 0;
        if (level < 60)
        {
            cap = level + 5;
            min = cap > 12 ? cap - 12 : 0;
        }
        else if (level < 70)
        {
            cap = 85 + (level - 60) * 4;
            min = cap > 20 ? cap - 20 : 65;
        }
        else
        {
            cap = 145 + (level - 70) * 4;
            min = cap > 25 ? cap - 25 : 120;
        }
        return Preset{kind, min, cap, label + " Level", 0};
    };

    if (arg.empty() || arg == "auto" || arg == "default")
    {
        if (level >= 80) return Preset{Kind::Blue, 187, 200, "Dungeon Blue"};
        return levelPreset(Kind::Level, "level-appropriate");
    }

    if (arg == "organic" || arg == "real" || arg == "random" || arg == "pug")
    {
        if (level >= 80) return Preset{Kind::Organic, 187, 219, "Organic T7", 7};
        return levelPreset(Kind::Organic, "Organic");
    }
    if (arg == "organic t7" || arg == "real t7" || arg == "random t7") return Preset{Kind::Organic, 187, 219, "Organic T7", 7};
    if (arg == "organic t8" || arg == "real t8" || arg == "random t8") return Preset{Kind::Organic, 200, 232, "Organic T8", 8};
    if (arg == "organic t9" || arg == "real t9" || arg == "random t9") return Preset{Kind::Organic, 219, 251, "Organic T9", 9};
    if (arg == "organic t10" || arg == "real t10" || arg == "random t10") return Preset{Kind::Organic, 232, 277, "Organic T10", 10};
    if (arg == "organic blue" || arg == "organic dungeon") return Preset{Kind::Organic, 175, 200, "Organic Blue", 0};

    if (arg == "blue" || arg == "dungeon" || arg == "preraid") return Preset{Kind::Blue, 187, 200, "Dungeon Blue"};
    if (arg == "t7" || arg == "naxx") return Preset{Kind::T7, 200, 213, "T7"};
    if (arg == "t8" || arg == "ulduar") return Preset{Kind::T8, 219, 232, "T8"};
    if (arg == "t9" || arg == "toc") return Preset{Kind::T9, 232, 258, "T9"};
    if (arg == "t10" || arg == "icc") return Preset{Kind::T10, 251, 277, "T10"};
    if (arg == "pvp" || arg == "wrathful") return Preset{Kind::Wrathful, 251, 284, "PvP Wrathful"};

    uint32_t cap = 0;
    auto [end, error] = std::from_chars(arg.data(), arg.data() + arg.size(), cap);
    if (error != std::errc{} || end != arg.data() + arg.size() || cap == 0 || cap > 284)
        return std::nullopt;
    return Preset{Kind::ItemLevel, cap > 15 ? cap - 15 : 0, cap, "ilvl <= " + std::to_string(cap)};
}

inline bool MatchesSet(Preset const& preset, uint32_t set)
{
    Kind k = preset.kind;
    if (k == Kind::Organic)
    {
        if (preset.targetTier == 7) k = Kind::T7;
        else if (preset.targetTier == 8) k = Kind::T8;
        else if (preset.targetTier == 9) k = Kind::T9;
        else if (preset.targetTier == 10) k = Kind::T10;
        else return false;
    }
    switch (k)
    {
        case Kind::T7: return set >= 787 && set <= 805;
        case Kind::T8: return set >= 820 && set <= 838;
        case Kind::T9: return set >= 843 && set <= 880;
        case Kind::T10: return set >= 883 && set <= 901;
        case Kind::Wrathful: return set >= 765 && set <= 780;
        default: return false;
    }
}

// Rows are the actual WotLK ItemSet IDs: T7, T8, T9 Alliance/Horde, T10, PvP.
// A primary stat match alone cannot distinguish priest/shaman/druid healer sets.
enum class SetProfile : uint8_t { WarriorDps, WarriorTank, PaladinHeal, PaladinTank, PaladinDps, Hunter, Rogue,
    PriestHeal, PriestDps, DKTank, DKDps, ShamanElemental, ShamanEnhancement, ShamanHeal, Mage, Warlock,
    DruidBalance, DruidFeral, DruidHeal, None };
inline SetProfile ClassSetProfile(uint8_t botClass, uint8_t botSpec, bool tank)
{
    switch (botClass)
    {
        case 1: return tank || botSpec == 3 ? SetProfile::WarriorTank : SetProfile::WarriorDps;
        case 2: return tank || botSpec == 5 ? SetProfile::PaladinTank : botSpec == 4 ? SetProfile::PaladinHeal : SetProfile::PaladinDps;
        case 3: return SetProfile::Hunter;
        case 4: return SetProfile::Rogue;
        case 5: return botSpec == 15 ? SetProfile::PriestDps : SetProfile::PriestHeal;
        case 6: return tank ? SetProfile::DKTank : SetProfile::DKDps;
        case 7: return botSpec == 21 ? SetProfile::ShamanHeal : botSpec == 20 ? SetProfile::ShamanEnhancement : SetProfile::ShamanElemental;
        case 8: return SetProfile::Mage;
        case 9: return SetProfile::Warlock;
        case 11: return botSpec == 30 ? SetProfile::DruidHeal : botSpec == 29 ? SetProfile::DruidFeral : SetProfile::DruidBalance;
        default: return SetProfile::None;
    }
}
inline constexpr std::array<std::array<uint32_t, 6>, 19> ClassSets{{
    {{788,830,867,868,895,765}}, {{787,831,869,870,896,765}},
    {{790,822,875,876,899,767}}, {{791,821,879,880,901,766}}, {{789,820,877,878,900,766}},
    {{794,838,859,860,891,772}}, {{801,826,857,858,890,776}},
    {{804,833,847,848,885,777}}, {{805,832,849,850,886,778}},
    {{793,835,873,874,898,768}}, {{792,834,871,872,897,768}},
    {{796,824,864,863,893,769}}, {{795,823,865,866,894,770}}, {{797,825,861,862,892,771}},
    {{803,836,843,844,883,779}}, {{802,837,846,845,884,780}},
    {{800,828,853,854,888,774}}, {{798,827,855,856,889,775}}, {{799,829,851,852,887,773}}
}};
inline bool IsClassSet(uint32_t set)
{
    for (auto const& row : ClassSets) if (std::find(row.begin(), row.end(), set) != row.end()) return true;
    return false;
}
inline bool FitsClassSet(SetProfile profile, uint32_t set)
{
    if (!IsClassSet(set)) return true;
    if (profile == SetProfile::None) return false;
    auto const& row = ClassSets[size_t(profile)];
    return std::find(row.begin(), row.end(), set) != row.end();
}

struct Candidate
{
    uint32_t id{}, level{}, set{}, uniqueCategory{}, categoryLimit{};
    float score{};
    bool unique{};
    bool pvp{};
    bool blocksOffhand{};
};

inline bool Better(Candidate const& next, Candidate const& old, Preset const& preset)
{
    if (!old.id) return true;
    bool inBand = next.level >= preset.minimum, oldInBand = old.level >= preset.minimum;
    if (inBand != oldInBand) return inBand;
    if (next.score != old.score) return next.score > old.score;
    if (next.level != old.level) return next.level > old.level;
    return next.id < old.id;
}

inline bool CanAdd(Candidate const& item, std::vector<Candidate> const& selected)
{
    uint32_t categoryCount = 0;
    for (Candidate const& other : selected)
    {
        if (item.id == other.id && (item.unique || other.unique)) return false;
        if (item.uniqueCategory && item.uniqueCategory == other.uniqueCategory) ++categoryCount;
    }
    return !item.uniqueCategory || !item.categoryLimit || categoryCount < item.categoryLimit;
}

inline Candidate Select(std::vector<Candidate> const& candidates, Preset const& preset,
    std::vector<Candidate> const& selected, uint32_t requiredSet = 0)
{
    Candidate best;
    for (Candidate const& item : candidates)
        if (item.level <= preset.maximum && (!requiredSet || item.set == requiredSet) &&
            CanAdd(item, selected) && Better(item, best, preset))
            best = item;
    return best;
}

template<size_t N, size_t S>
uint32_t SelectSet(std::array<std::vector<Candidate>, N> const& candidates,
    std::array<uint8_t, S> const& slots, Preset const& preset)
{
    uint32_t chosen = 0;
    float bestScore = -1.0f;
    for (Candidate const& head : candidates[slots.front()])
    {
        if (!MatchesSet(preset, head.set)) continue;
        float score = 0.0f;
        bool complete = true;
        for (uint8_t slot : slots)
        {
            Candidate best = Select(candidates[slot], preset, {}, head.set);
            if (!best.id || best.level < preset.minimum) { complete = false; break; }
            score += best.score;
        }
        if (complete && (score > bestScore || (score == bestScore && head.set < chosen)))
        {
            chosen = head.set;
            bestScore = score;
        }
    }
    return chosen;
}

// The real NPCBot inventory layout; shared with the planner tests.
inline constexpr uint8_t MainHandSlot = 0, OffHandSlot = 1, BodySlot = 6;
inline bool OrganicPvPSlot(uint8_t slot) { return slot == 8 || slot == 10 || slot == 11 || slot == 15 || slot == 16; }
inline uint32_t OrganicSeed(uint32_t identity, uint8_t spec, uint32_t roles)
{
    uint32_t hash = identity * 0x9e3779b9u;
    hash ^= uint32_t(spec) * 0x85ebca6bu;
    hash ^= roles * 0xc2b2ae35u;
    return hash ? hash : 987654321u;
}

template<size_t N, size_t S>
std::array<Candidate, N> SelectOrganicPlan(
    std::array<std::vector<Candidate>, N> const& candidates,
    std::array<uint8_t, S> const& tierSlots,
    Preset const& preset,
    uint32_t seed)
{
    static_assert(S > 0 && N > BodySlot);
    std::array<Candidate, N> plan{};
    std::vector<Candidate> selected;
    uint32_t state = seed ? seed : 987654321u;
    auto roll = [&state](uint32_t mod) {
        state = state * 1664525u + 1013904223u;
        return (state >> 16) % mod;
    };
    uint32_t set = preset.targetTier ? SelectSet(candidates, tierSlots, preset) : 0;
    std::array<bool, N> forced{};
    if (set)
    {
        uint32_t r = roll(100);
        size_t count = r < 35 ? 4 : r < 70 ? 2 : r < 85 ? 5 : 3;
        auto shuffled = tierSlots;
        for (size_t i = S - 1; i > 0; --i) std::swap(shuffled[i], shuffled[roll(uint32_t(i + 1))]);
        for (size_t i = 0; i < std::min(count, S); ++i) forced[shuffled[i]] = true;
    }
    for (uint8_t slot = 0; slot < N; ++slot)
    {
        if (slot == BodySlot || (slot == OffHandSlot && plan[MainHandSlot].blocksOffhand)) continue;
        bool wantPvP = !forced[slot] && OrganicPvPSlot(slot) && roll(100) < 10;
        auto eligible = [&](bool pvp) {
            std::vector<Candidate> pool;
            for (Candidate const& item : candidates[slot])
            {
                if (!item.id || item.level > preset.maximum || !CanAdd(item, selected)) continue;
                if (forced[slot] ? (item.set != set) : (set && item.set == set)) continue;
                if (item.pvp != pvp) continue;
                pool.push_back(item);
            }
            std::sort(pool.begin(), pool.end(), [&preset](Candidate const& a, Candidate const& b) { return Better(a, b, preset); });
            return pool;
        };
        auto pool = eligible(wantPvP);
        if (pool.empty() && wantPvP) pool = eligible(false);
        if (pool.empty()) continue; // A missing required slot is rejected by runtime preflight.
        size_t index = 0;
        if (!forced[slot] && pool.size() > 1)
        {
            uint32_t r = roll(100);
            index = std::min<size_t>(r < 60 ? 0 : r < 85 ? 1 : 2, pool.size() - 1);
        }
        plan[slot] = pool[index];
        selected.push_back(plan[slot]);
    }
    return plan;
}

} // namespace NPCBots::AutoEquip
#endif
