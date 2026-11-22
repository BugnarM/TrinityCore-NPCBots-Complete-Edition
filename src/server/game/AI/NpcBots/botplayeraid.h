/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef NPCBOT_PLAYER_AID_H
#define NPCBOT_PLAYER_AID_H

#include "Define.h"

class Player;
class WorldObject;

namespace NPCBots
{
    WorldObject* GetWandererRescueTarget(Player& player);
    void UpdateWandererPlayerRescue(uint32 diff);
}

#endif
