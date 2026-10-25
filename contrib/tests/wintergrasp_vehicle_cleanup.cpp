// g++ -std=c++20 -O2 -Wall -Wextra -Werror contrib/tests/wintergrasp_vehicle_cleanup.cpp -o /tmp/wg-vehicle-cleanup
#include "../../src/server/scripts/Northrend/wintergrasp_bot_war.h"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using Registry = std::unordered_set<uint64_t>;

static void Require(bool value, char const* message)
{
    if (!value) throw std::runtime_error(message);
}

int main()
{
    try
    {
        std::mt19937 random(20261025);
        unsigned cases = 0;
        for (unsigned size = 0; size <= 120; ++size)
            for (unsigned mutation = 0; mutation < 6; ++mutation)
                for (unsigned attempt = 0; attempt < 8; ++attempt)
                {
                    std::vector<uint64_t> ids;
                    for (unsigned i = 0; i < size; ++i) ids.push_back(uint64_t(i + 1) * 0x100000001ULL);
                    std::shuffle(ids.begin(), ids.end(), random);
                    Registry live(ids.begin(), ids.end());
                    std::unordered_map<uint64_t, unsigned> visits;
                    unsigned removed = 0;
                    WintergraspBotWar::VisitVehicleSnapshot(live, [&](uint64_t guid) {
                        Require(++visits[guid] == 1, "A vehicle was visited twice");
                        removed += live.erase(guid); // Real OnCreatureRemove invalidates the current iterator.
                        switch (mutation)
                        {
                            case 0: break;
                            case 1: // Passenger/accessory cleanup removes another vehicle too.
                                if (!live.empty()) { live.erase(live.begin()); ++removed; }
                                break;
                            case 2: // A nested callback empties the registry.
                                removed += unsigned(live.size()); live.clear();
                                break;
                            case 3: // Callback inserts entries and causes a rehash.
                                live.insert(0xFFFFFFFF00000000ULL + guid);
                                live.rehash(257 + (visits.size() % 2) * 256);
                                break;
                            case 4: // A death/remove notification is delivered twice.
                                Require(live.erase(guid) == 0, "Repeated removal released a slot twice");
                                break;
                            case 5: // Reentrant snapshot from a removal hook.
                                WintergraspBotWar::VisitVehicleSnapshot(live, [&](uint64_t other) { removed += live.erase(other); });
                                break;
                        }
                    });
                    Require(visits.size() == size, "Original vehicle was skipped after a callback");
                    for (auto guid : ids) Require(visits[guid] == 1, "Original vehicle not processed exactly once");
                    if (mutation != 3)
                    {
                        Require(live.empty(), "A vehicle remains registered after cleanup");
                        Require(removed == size, "Vehicle capacity released more or less than once");
                    }
                    else
                        Require(live.size() == size, "New entries were consumed by the old cleanup pass");
                    ++cases;
                }
        Registry empty;
        WintergraspBotWar::VisitVehicleSnapshot(empty, [](uint64_t) { throw std::runtime_error("Visited empty registry"); });
        std::cout << "PASS: " << cases << " production snapshot cleanup cases: erase-current, erase-other, clear, rehash, repeated and reentrant removal.\n";
    }
    catch (std::exception const& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
