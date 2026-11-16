#include "../../src/server/game/AI/NpcBots/botlimits.h"
#include "../../src/server/game/AI/NpcBots/botdungeonfinder.h"

#include <cassert>
#include <iostream>
#include <numeric>
#include <vector>

int main()
{
    using namespace NPCBots::Limits;
    using namespace NPCBots::DungeonFinder;

    // A live realm with a large staff limit must still cap ordinary accounts.
    assert(PersonalLimit(39, 1, false) == 1);
    assert(PersonalLimit(39, 1, true) == 39);
    assert(PersonalLimit(0, 1, false) == 0);
    assert(PersonalLimit(39, 0, false) == 0);
    assert(CanAttach(1, 0, false, false));
    assert(!CanAttach(1, 1, false, false));
    assert(!CanAttach(1, 4, false, false)); // Previously owned/shared companions cannot bypass the cap.
    assert(CanAttach(1, 1, true, false));   // Finder additions are independent of the personal slot.
    assert(CanAttach(1, 4, false, true));  // Staff retains existing attachment behavior.
    assert(!CanAttach(0, 0, false, false));

    // A solo DPS gets four bots. Bringing one personal DPS bot needs only three.
    auto solo = MissingRoles(std::vector<std::uint8_t>{ Roles[2] });
    assert(solo && std::accumulate(solo->begin(), solo->end(), 0u) == 4);
    auto companion = MissingRoles(std::vector<std::uint8_t>{ Roles[2], Roles[2] });
    assert(companion && std::accumulate(companion->begin(), companion->end(), 0u) == 3);
    for (std::uint8_t role : Roles)
    {
        auto missing = MissingRoles(std::vector<std::uint8_t>{role});
        assert(missing && std::accumulate(missing->begin(), missing->end(), 0u) == 4);
    }
    assert(!MissingRoles(std::vector<std::uint8_t>{Roles[0], Roles[0]}));
    auto full = MissingRoles(std::vector<std::uint8_t>{Roles[0], Roles[1], Roles[2], Roles[2], Roles[2]});
    assert(full && std::accumulate(full->begin(), full->end(), 0u) == 0);
    std::cout << "NPCBot player limits and Finder composition: passed\n";
}
