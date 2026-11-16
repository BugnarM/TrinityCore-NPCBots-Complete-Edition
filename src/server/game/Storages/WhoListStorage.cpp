/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "WhoListStorage.h"
#include "World.h"
#include "ObjectAccessor.h"
#include "Player.h"
#include "GuildMgr.h"
#include "WorldSession.h"
#include "Config.h"
#include "Creature.h"
#include "botdatamgr.h"
#include "botcommon.h"
#include <shared_mutex>

WhoListStorageMgr* WhoListStorageMgr::instance()
{
    static WhoListStorageMgr instance;
    return &instance;
}

void WhoListStorageMgr::Update()
{
    // clear current list
    _whoListStorage.clear();
    _whoListStorage.reserve(sWorld->GetPlayerCount() + 100);

    HashMapHolder<Player>::MapType const& m = ObjectAccessor::GetPlayers();
    for (HashMapHolder<Player>::MapType::const_iterator itr = m.begin(); itr != m.end(); ++itr)
    {
        if (!itr->second->FindMap() || itr->second->GetSession()->PlayerLoading())
            continue;

        std::string playerName = itr->second->GetName();
        std::wstring widePlayerName;
        if (!Utf8toWStr(playerName, widePlayerName))
            continue;

        wstrToLower(widePlayerName);

        std::string guildName = sGuildMgr->GetGuildNameById(itr->second->GetGuildId());
        std::wstring wideGuildName;
        if (!Utf8toWStr(guildName, wideGuildName))
            continue;

        wstrToLower(wideGuildName);

        _whoListStorage.emplace_back(itr->second->GetGUID(), itr->second->GetTeam(), itr->second->GetSession()->GetSecurity(), itr->second->GetLevel(),
            itr->second->GetClass(), itr->second->GetRace(), itr->second->GetZoneId(), itr->second->GetNativeGender(), itr->second->IsVisible(),
            widePlayerName, wideGuildName, playerName, guildName);
    }

    if (sConfigMgr->GetBoolDefault("NpcBot.WhoList.Enable", true) && BotDataMgr::AllBotsLoaded())
    {
        std::string botGuild = sConfigMgr->GetStringDefault("NpcBot.WhoList.GuildName", "");
        std::wstring wideBotGuild;
        if (!botGuild.empty())
        {
            if (Utf8toWStr(botGuild, wideBotGuild))
                wstrToLower(wideBotGuild);
        }

        std::shared_lock lock(*BotDataMgr::GetLock());
        NpcBotRegistry const& bots = BotDataMgr::GetExistingNPCBots();
        for (Creature const* bot : bots)
        {
            if (!bot || !bot->IsInWorld() || !bot->FindMap() || !bot->IsAlive())
                continue;

            std::string botName = bot->GetName();
            std::wstring wideBotName;
            if (!Utf8toWStr(botName, wideBotName))
                continue;

            wstrToLower(wideBotName);

            uint32 team = Player::TeamForRace(bot->GetRace());
            if (!team)
                team = (BotDataMgr::GetTeamIdForFaction(bot->GetFaction()) == TEAM_ALLIANCE) ? ALLIANCE : HORDE;

            uint8 clss = bot->GetBotClass();
            if (clss >= BOT_CLASS_EX_START)
            {
                switch (clss)
                {
                    case BOT_CLASS_ARCHMAGE: clss = CLASS_MAGE; break;
                    case BOT_CLASS_DREADLORD: clss = CLASS_WARLOCK; break;
                    case BOT_CLASS_SPELLBREAKER: clss = CLASS_WARRIOR; break;
                    case BOT_CLASS_DARK_RANGER: clss = CLASS_HUNTER; break;
                    case BOT_CLASS_NECROMANCER: clss = CLASS_WARLOCK; break;
                    case BOT_CLASS_SEA_WITCH: clss = CLASS_SHAMAN; break;
                    case BOT_CLASS_CRYPT_LORD: clss = CLASS_WARRIOR; break;
                    default: clss = CLASS_WARRIOR; break;
                }
            }

            _whoListStorage.emplace_back(bot->GetGUID(), team, SEC_PLAYER, bot->GetLevel(),
                clss, bot->GetRace(), bot->GetZoneId(), uint8(bot->GetNativeGender()), true,
                wideBotName, wideBotGuild, botName, botGuild);
        }
    }
}
