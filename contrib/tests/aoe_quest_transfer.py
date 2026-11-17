#!/usr/bin/env python3
"""Exercise the production Loot transfer API against per-player/FFA/partial-stack cases."""
from pathlib import Path
import subprocess
import sys
import os
import tempfile
root = Path(__file__).resolve().parents[2]
s = (root/'src/server/game/Loot/Loot.cpp').read_text()
helpers = s[s.index('namespace\n{\n    bool SimpleQuestRow'):s.index('bool Loot::CanTransferQuestLootFor')]
methods = s[s.index('bool Loot::CanTransferQuestLootFor'):s.index('void Loot::FillNotNormalLootFor')]
code = r'''
#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <set>
#include <vector>
using uint8=std::uint8_t;using uint32=std::uint32_t;
constexpr unsigned MAX_NR_LOOT_ITEMS=16,MAX_NR_QUEST_ITEMS=32;
struct Guid{unsigned id=0;bool IsEmpty()const{return !id;}auto operator<=>(Guid const&)const=default;};
struct Player{Guid guid{1};bool grouped=false;void const* GetGroup()const{return grouped?this:nullptr;}Guid GetGUID()const{return guid;}};
struct LootItem{bool is_looted=false,freeforall=false,follow_loot_rules=false,is_blocked=true,is_counted=false,allowed=true;unsigned itemid=10,randomPropertyId=0,randomPropertySeed=0,itemIndex=0;uint8 count=1;Guid rollWinnerGUID;std::set<Guid> allowedGUIDs;std::vector<int> conditions;bool AllowedForPlayer(Player const*,Guid)const{return allowed;}};
struct NotNormalLootItem{uint8 index;bool is_looted;NotNormalLootItem(uint8 i,bool l=false):index(i),is_looted(l){}};
using NotNormalLootItemList=std::vector<NotNormalLootItem>;
struct ItemTemplate{unsigned GetMaxStackSize()const{return 20;}};
struct Manager{ItemTemplate item;ItemTemplate const* GetItemTemplate(unsigned)const{return &item;}} manager;
auto* sObjectMgr=&manager;
struct Loot{
 Guid lootOwnerGUID{1};std::set<Guid> PlayersLooting;std::vector<LootItem> items,quest_items;uint8 unlootedCount=0;
 std::map<Guid,NotNormalLootItemList*> PlayerQuestItems;
 ~Loot(){for(auto& [_,rows]:PlayerQuestItems)delete rows;}
 void Add(unsigned count=1){LootItem item;item.count=count;item.itemIndex=quest_items.size();quest_items.push_back(item);if(!PlayerQuestItems.contains({1}))PlayerQuestItems[{1}]=new NotNormalLootItemList();PlayerQuestItems.at({1})->emplace_back(uint8(item.itemIndex));++unlootedCount;}
 bool CanTransferQuestLootFor(Player const*,size_t)const;
 uint32 TransferQuestLootFrom(Loot&,Player*,size_t,uint32);
 void ConsolidateQuestLootFor(Player*);
};
HELPERS
METHODS
int main(){
 Player p;
 {Loot source,main;source.Add(10);main.Add(15);assert(main.TransferQuestLootFrom(source,&p,0,10)==10);assert(main.quest_items[0].count==20&&main.quest_items[1].count==5);assert(source.unlootedCount==0&&source.quest_items[0].is_looted&&source.PlayerQuestItems.at({1})->at(0).is_looted);assert(main.unlootedCount==2);}
 {Loot source,main;source.Add(10);main.Add(19);main.items.resize(15);assert(main.TransferQuestLootFrom(source,&p,0,10)==1);assert(source.quest_items[0].count==9&&source.unlootedCount==1&&!source.quest_items[0].is_looted);}
 {Loot source,main;source.Add(10);source.quest_items[0].freeforall=true;source.PlayerQuestItems[{2}]=new NotNormalLootItemList{{0}};assert(!main.TransferQuestLootFrom(source,&p,0,10));assert(source.quest_items[0].count==10&&source.PlayerQuestItems.at({2})->at(0).is_looted==false);}
 {Loot source,main;source.Add(10);p.grouped=true;assert(!main.TransferQuestLootFrom(source,&p,0,10));p.grouped=false;}
 {Loot source,main;source.Add(10);source.PlayerQuestItems.at({1})->at(0).is_looted=true;assert(!main.TransferQuestLootFrom(source,&p,0,10));}
 {Loot source,main;source.Add(10);main.Add(10);main.PlayerQuestItems.at({1})->at(0).is_looted=true;assert(main.TransferQuestLootFrom(source,&p,0,10)==10);assert(main.quest_items[0].count==10&&main.quest_items[1].count==10);}
 {Loot source,main;source.Add(10);source.quest_items[0].conditions.push_back(1);assert(!main.TransferQuestLootFrom(source,&p,0,10));}
 {Loot source,main;source.Add(10);main.lootOwnerGUID={2};assert(!main.TransferQuestLootFrom(source,&p,0,10));}
 {Loot source,main;source.Add(10);main.PlayersLooting.insert({2});assert(!main.TransferQuestLootFrom(source,&p,0,10));}
 {Loot main;main.Add(3);main.Add(4);main.ConsolidateQuestLootFor(&p);assert(main.quest_items[0].count==7&&main.quest_items[1].is_looted&&main.unlootedCount==1&&main.PlayerQuestItems.at({1})->size()==1);}
 for(unsigned count=1;count<100;++count){Loot source,main;source.Add(count);main.Add(19);unsigned moved=main.TransferQuestLootFrom(source,&p,0,count);unsigned total=source.quest_items[0].count;for(auto const& item:main.quest_items)total+=item.count;assert(total==count+19);assert(moved==count);for(auto const& item:main.quest_items)assert(item.count<=20);}
 std::cout<<"PASS: native quest transfer preserves counts, source visibility, slot limits and protected shared/conditional rights\n";
}
'''.replace('HELPERS',helpers).replace('METHODS',methods)
with tempfile.TemporaryDirectory(prefix='aoe-quest-') as temp:
    cpp=Path(temp)/'test.cpp'; exe=Path(temp)/'test'; cpp.write_text(code)
    subprocess.run(['c++','-std=c++20','-O2','-Wall','-Wextra','-Werror',*(['-fsanitize=address,undefined','-fno-omit-frame-pointer'] if '--sanitize' in sys.argv else []),str(cpp),'-o',str(exe)],check=True)
    subprocess.run([str(exe)],check=True,env=os.environ | ({'ASAN_OPTIONS':'detect_leaks=0'} if '--sanitize' in sys.argv else {}))
