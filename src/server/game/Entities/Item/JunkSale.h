#ifndef TRINITY_JUNK_SALE_H
#define TRINITY_JUNK_SALE_H

#include "Define.h"

class Player;

namespace JunkSale
{
enum class Source { Merchant, Scrapper };

TC_GAME_API void SellFromBags(Player& player, Source source);
}

#endif
