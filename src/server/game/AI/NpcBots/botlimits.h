#ifndef NPCBOT_LIMITS_H
#define NPCBOT_LIMITS_H

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace NPCBots::Limits
{
constexpr std::uint8_t PersonalLimit(std::uint8_t bracketLimit, std::uint8_t playerLimit, bool gameMaster)
{
    return gameMaster ? 255 : std::min(bracketLimit, playerLimit);
}

// Generated dungeon/raid bots and temporary class summons do not consume
// personal companion slots. An already-owned bot still needs a free active slot.
constexpr bool CanAttach(std::uint8_t limit, std::size_t activePersonalBots, bool temporary, bool gameMaster)
{
    return temporary || gameMaster || activePersonalBots < limit;
}
}

#endif
