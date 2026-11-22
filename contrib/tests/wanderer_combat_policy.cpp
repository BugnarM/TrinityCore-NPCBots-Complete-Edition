#include "../../src/server/game/AI/NpcBots/botwanderercombat.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <iostream>

using namespace NPCBots::WandererCombat;

int main()
{
    Settings settings;
    assert(!PickTarget({}, settings, 45, 0, 0.0));

    std::array party = {
        Candidate{ Kind::Player, 6.f, 0.f, 500 },
        Candidate{ Kind::Companion, 10.f, 0.f, 2000 },
        Candidate{ Kind::Tank, 25.f, 12.f, 2000 }
    };
    unsigned humans = 0, companions = 0, tanks = 0;
    for (unsigned roll = 0; roll < 100; ++roll)
        for (unsigned ticket = 0; ticket < 100; ++ticket)
        {
            auto target = *PickTarget(party, settings, 45, roll, (ticket + 0.5) / 100.0);
            humans += target == 0;
            companions += target == 1;
            tanks += target == 2;
        }
    assert(humans == 3500);
    assert(companions == 1625 && tanks == 4875);

    // A follower can reach the distance band through formation credit, even if
    // the owner is seen first and the follower is 19 yards behind.
    assert(*PickTarget(party, settings, 45, 70, 0.9) == 2);
    party[2].formationCredit = 0;
    assert(*PickTarget(party, settings, 45, 70, 0.9) == 1);
    party[2].formationCredit = 200.f;
    party[2].distance = 27.f;
    assert(*PickTarget(party, settings, 45, 70, 0.9) == 1);

    std::array factionEncounter = {
        Candidate{ Kind::Player, 7.f, 0.f, 500 },
        Candidate{ Kind::Companion, 10.f, 0.f, 750 },
        Candidate{ Kind::OpposingWanderer, 14.f, 0.f, 1000 }
    };
    assert(*PickTarget(factionEncounter, settings, 45, 0, 0.2) == 2);
    settings.prioritizeBotsOverPlayers = false;
    assert(*PickTarget(factionEncounter, settings, 45, 0, 0.2) == 0);
    settings.prioritizeBotsOverPlayers = true;
    factionEncounter[2].distance = 16.f;
    assert(*PickTarget(factionEncounter, settings, 45, 95, 0.9) == 1);
    factionEncounter[1].kind = Kind::Other;
    assert(*PickTarget(factionEncounter, settings, 45, 95, 0.9) == 0);
    factionEncounter[0].kind = Kind::Other;
    factionEncounter[1].kind = Kind::Player;
    assert(*PickTarget(factionEncounter, settings, 45, 0, 0.5) == 0);

    std::array equal = {
        Candidate{ Kind::Other, 3.f, 0.f, 1500 },
        Candidate{ Kind::Other, 11.f, 0.f, 1500 }
    };
    assert(*PickTarget(equal, settings, 45, 0, 0.49) == 0);
    assert(*PickTarget(equal, settings, 45, 0, 0.51) == 1);
    std::ranges::reverse(equal);
    assert(*PickTarget(equal, settings, 45, 0, 0.49) == 0);
    assert(*PickTarget(equal, settings, 45, 0, 0.51) == 1);

    assert(TargetWeight({Kind::Companion, 10.f, 0.f, 4000}, 45) >
        TargetWeight({Kind::Companion, 10.f, 0.f, 1000}, 45));
    std::cout << "PASS weighted companions, formation hold, faction-war priority, PvE fallback, and equal local distances\n";
}
