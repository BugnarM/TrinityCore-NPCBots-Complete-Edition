#!/usr/bin/env python3
"""Execute the production grant function: partial bags, sold items, restart and failed transaction."""
from pathlib import Path
import subprocess
import sys
import os
import tempfile
root=Path(__file__).resolve().parents[2]
s=(root/'src/server/scripts/Custom/custom_script_loader.cpp').read_text()
a=s.index('static bool GrantClassHeirlooms(');b=s.index('\nclass companion_playerscript',a)
method=s[a:b]
code=r'''
#include <array>
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
using uint8=std::uint8_t;using uint16=std::uint16_t;using uint32=std::uint32_t;
namespace Trinity{template<class... Args>std::string StringFormat(std::string s,Args... args){auto add=[&](auto value){s.replace(s.find("{}"),2,std::to_string(value));};(add(args),...);return s;}}
enum{CLASS_WARRIOR=1,CLASS_PALADIN=2,CLASS_HUNTER=3,CLASS_ROGUE=4,CLASS_PRIEST=5,CLASS_DEATH_KNIGHT=6,CLASS_SHAMAN=7,CLASS_MAGE=8,CLASS_WARLOCK=9,CLASS_DRUID=11,TEAM_ALLIANCE=0,NULL_SLOT=255,NULL_BAG=255,EQUIP_ERR_OK=0};
using ItemPosCountVec=std::vector<unsigned>;
struct Field{unsigned value;unsigned GetUInt32()const{return value;}};
struct Result{std::vector<std::array<Field,2>> rows;unsigned cursor=0;Field operator[](unsigned i)const{return rows.at(cursor).at(i);}bool NextRow(){return ++cursor<rows.size();}};
using QueryResult=std::shared_ptr<Result>;
struct Transaction{std::vector<std::string> sql;std::map<unsigned,unsigned> inventory;bool saved=false;template<class... Args>void PAppend(std::string fmt,Args... args){sql.push_back(Trinity::StringFormat(fmt,args...));}};
using CharacterDatabaseTransaction=std::shared_ptr<Transaction>;
struct Database{
 bool received=false,fail=false;std::map<unsigned,unsigned> ledger,inventory;unsigned commits=0;
 QueryResult Query(char const* query){auto r=std::make_shared<Result>();if(std::string(query).find("SELECT 1")!=std::string::npos){if(!received)return {};r->rows.push_back({Field{1},Field{0}});}else{for(auto [item,count]:ledger)r->rows.push_back({Field{item},Field{count}});}return r->rows.empty()?QueryResult{}:r;}
 CharacterDatabaseTransaction BeginTransaction(){return std::make_shared<Transaction>();}
 void DirectCommitTransaction(CharacterDatabaseTransaction transaction){assert(transaction->saved);if(fail)throw std::runtime_error("simulated connection loss before atomic commit");inventory=transaction->inventory;for(auto const& sql:transaction->sql){unsigned guid,item,count;if(std::sscanf(sql.c_str(),"REPLACE INTO character_heirloom_delivery (guid, item, count) VALUES (%u, %u, %u)",&guid,&item,&count)==3)ledger[item]=count;else if(sql.find("INSERT IGNORE")!=std::string::npos)received=true;}++commits;}
}CharacterDatabase;
struct BotCfg{struct Settings{bool heirlooms=true;};static Settings GetHealingSettings(){return {};}};
struct Item{};
struct Player{
 struct Guid{unsigned GetCounter()const{return 1;}};std::map<unsigned,unsigned> inventory=CharacterDatabase.inventory;unsigned capacity=100,totalGranted=0;bool equip=false;std::set<unsigned> spells;Item item;
 bool IsBeingTeleported()const{return false;}Guid GetGUID()const{return {};};unsigned GetClass()const{return CLASS_PRIEST;}unsigned GetTeamId()const{return TEAM_ALLIANCE;}
 bool HasSpell(unsigned spell)const{return spells.contains(spell);}void LearnSpell(unsigned spell,bool){spells.insert(spell);}
 unsigned GetItemCount(unsigned id,bool)const{auto it=inventory.find(id);return it==inventory.end()?0:it->second;}
 unsigned Used()const{unsigned n=0;for(auto [_,count]:inventory)n+=count;return n;}
 unsigned CanEquipNewItem(unsigned,uint16& slot,unsigned,bool)const{slot=0;return equip&&Used()<capacity?EQUIP_ERR_OK:1;}
 Item* EquipNewItem(unsigned,unsigned id,bool){++inventory[id];++totalGranted;return &item;}
 void AutoUnequipOffhandIfNeed(){}void SendNewItem(Item*,unsigned,bool,bool){}
 unsigned CanStoreNewItem(unsigned,unsigned,ItemPosCountVec& dest,unsigned id,unsigned)const{dest={id};return Used()<capacity?EQUIP_ERR_OK:1;}
 Item* StoreNewItem(ItemPosCountVec const& dest,unsigned,bool){++inventory[dest.at(0)];++totalGranted;return &item;}
 void SaveToDB(CharacterDatabaseTransaction transaction){transaction->saved=true;transaction->inventory=inventory;}
 void* GetSession()const{return nullptr;}
};
struct ChatHandler{explicit ChatHandler(void*){}void SendSysMessage(char const*){}};
METHOD
int main(){
 {Player partial;partial.capacity=3;assert(!GrantClassHeirlooms(&partial,false));assert(!CharacterDatabase.received&&CharacterDatabase.ledger.at(41600)==3&&CharacterDatabase.inventory.at(41600)==3);}
 --CharacterDatabase.inventory[41600]; // one delivered bag sold before reconnect
 {Player reconnect;assert(GrantClassHeirlooms(&reconnect,false));assert(CharacterDatabase.received&&CharacterDatabase.ledger.at(41600)==4&&CharacterDatabase.inventory.at(41600)==3);assert(reconnect.spells.contains(198)&&!reconnect.spells.contains(199));}
 auto delivered=CharacterDatabase.inventory;CharacterDatabase.inventory.erase(42947);
 {Player repeat;assert(GrantClassHeirlooms(&repeat,false));assert(repeat.totalGranted==0&&!repeat.inventory.contains(42947));}
 CharacterDatabase={};CharacterDatabase.fail=true;
 try{Player crash;GrantClassHeirlooms(&crash,false);assert(false);}catch(std::runtime_error const&){}
 assert(CharacterDatabase.inventory.empty()&&CharacterDatabase.ledger.empty()&&!CharacterDatabase.received);
 CharacterDatabase.fail=false;
 {Player retry;retry.equip=true;assert(GrantClassHeirlooms(&retry,false));assert(CharacterDatabase.received&&CharacterDatabase.inventory.at(41600)==4);}
 std::cout<<"PASS: production heirloom grant resumes partial delivery, prevents sold-item regrants and commits inventory+ledger together\n";
}
'''.replace('METHOD',method)
with tempfile.TemporaryDirectory(prefix='heirloom-delivery-') as temp:
 cpp=Path(temp)/'test.cpp';exe=Path(temp)/'test';cpp.write_text(code)
 subprocess.run(['c++','-std=c++20','-O2','-Wall','-Wextra','-Werror',*(['-fsanitize=address,undefined','-fno-omit-frame-pointer'] if '--sanitize' in sys.argv else []),str(cpp),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env=os.environ | ({'ASAN_OPTIONS':'detect_leaks=0'} if '--sanitize' in sys.argv else {}))
