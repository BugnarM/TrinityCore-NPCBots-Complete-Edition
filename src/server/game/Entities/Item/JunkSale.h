#ifndef TRINITY_JUNK_SALE_H
#define TRINITY_JUNK_SALE_H

#include "Define.h"

class Player;

namespace JunkSale
{
enum class Source { Merchant, Scrapper, Command };
enum class Mode { GreyOnly, Greens, Gear, All };

TC_GAME_API void SellFromBags(Player& player, Source source, Mode mode = Mode::GreyOnly);
}

#endif
