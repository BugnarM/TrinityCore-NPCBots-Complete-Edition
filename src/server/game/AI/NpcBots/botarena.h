#ifndef NPCBOT_ARENA_H
#define NPCBOT_ARENA_H

#include <cstdint>

// Shared by the live arena code and the standalone regression tests.
namespace ArenaBots
{
constexpr bool IsTeamSize(std::uint32_t size) { return size == 2 || size == 3 || size == 5; }
constexpr std::uint32_t NormalizeTarget(int configured, std::uint32_t size)
{
    return !IsTeamSize(size) || configured == 0 ? 0 : size;
}
constexpr bool CanQueueGroup(std::uint32_t members, std::uint32_t size, bool fillEnabled)
{
    return IsTeamSize(size) && members > 0 && members <= size && (fillEnabled || members == size);
}
constexpr std::uint32_t MissingPlayers(std::uint32_t target, std::uint32_t queued)
{
    return queued < target ? target - queued : 0;
}
constexpr std::uint32_t FlagSpell(bool gold, bool hordeRace)
{
    return hordeRace ? (gold ? 35774 : 35775) : (gold ? 32724 : 32725);
}
enum class Result { Pending, Gold, Green, Draw };
constexpr Result Winner(std::uint32_t goldAlive, std::uint32_t greenAlive,
    std::uint32_t goldTotal, std::uint32_t greenTotal)
{
    if (!goldAlive && !greenAlive && (goldTotal || greenTotal))
        return Result::Draw;
    if (!goldAlive && greenTotal)
        return Result::Green;
    if (!greenAlive && goldTotal)
        return Result::Gold;
    return Result::Pending;
}
}

#endif
