#!/usr/bin/env python3
"""Execute native WG player/pet/bot-shared promotion and exact 5/10-kill thresholds."""
from pathlib import Path
import subprocess,tempfile
repo=Path(__file__).resolve().parents[2]
s=(repo/'src/server/scripts/Battlefield/BattlefieldWG.cpp').read_text()
def function(signature):
 start=s.index(signature);end=s.index('{',start)+1;depth=1
 while depth:depth+=(s[end]=='{')-(s[end]=='}');end+=1
 return s[start:end]
code=r'''
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <map>
#include <set>
#include <string>
using uint32=unsigned;using ObjectGuid=unsigned;
enum TeamId{TEAM_ALLIANCE,TEAM_HORDE,PVP_TEAMS_COUNT};
enum{TYPEID_UNIT,TYPEID_PLAYER,UNIT_FLAG_SKINNABLE,SPELL_RECRUIT=10,SPELL_CORPORAL,SPELL_LIEUTENANT,
 BATTLEFIELD_WG_TEXT_RANK_CORPORAL,BATTLEFIELD_WG_TEXT_RANK_FIRST_LIEUTENANT,CHAT_MSG_ADDON,LANG_ADDON,TEXT_RANGE_NORMAL,TEAM_OTHER};
unsigned WintergraspFaction[]={1732,1735};
#define TC_LOG_INFO(...) ((void)0)
struct Config{bool enabled=true;bool GetBoolDefault(char const*,bool,bool){return enabled;}} config;
auto* sConfigMgr=&config;
struct Map{};struct Creature;
struct Unit{
 unsigned entry=900001,guid=100,zone=4197,faction=1735;Map* map=nullptr;float x=0;
 virtual ~Unit()=default;virtual Creature* ToCreature(){return nullptr;}virtual int GetTypeId()const{return TYPEID_UNIT;}
 unsigned GetMapId()const{return 571;}unsigned GetZoneId()const{return zone;}unsigned GetGUID()const{return guid;}
 unsigned GetEntry()const{return entry;}Map* GetMap()const{return map;}void SetUnitFlag(unsigned){}
};
struct BotAI{bool controlled=true;bool IsBattlefieldBot()const{return controlled;}};
struct Creature:Unit{BotAI ai;bool native=true;Creature* ToCreature()override{return this;}
 bool IsNPCBot()const{return native;}BotAI* GetBotAI(){return &ai;}unsigned GetFaction()const{return faction;}};
struct Aura{unsigned stacks=0;unsigned GetStackAmount()const{return stacks;}};
struct Player:Unit{
 std::map<unsigned,Aura> auras;TeamId team=TEAM_ALLIANCE;
 int GetTypeId()const override{return TYPEID_PLAYER;}TeamId GetTeamId()const{return team;}
 float GetDistance2d(Unit const* u)const{return std::fabs(x-u->x);}std::string GetName()const{return "Participant";}
 Aura* GetAura(unsigned id){auto i=auras.find(id);return i==auras.end()?nullptr:&i->second;}
 void RemoveAura(unsigned id){auras.erase(id);}void CastSpell(Player*,unsigned id,bool){++auras[id].stacks;}
};
namespace ObjectAccessor{std::map<unsigned,Player*> players;Player* FindPlayer(unsigned id){auto i=players.find(id);return i==players.end()?nullptr:i->second;}}
struct TextMgr{template<class... Args>void SendChat(Args...){}} textMgr;auto* sCreatureTextMgr=&textMgr;
struct BattlefieldWG{
 bool war=true,m_isActive=true;unsigned m_ZoneId=4197,StalkerGuid=1,normalKills=0;
 std::array<std::set<unsigned>,2> m_PlayersInWar;std::set<unsigned> m_promotedBotWarVictims;
 bool IsWarTime()const{return war;}Creature* GetCreature(unsigned){return nullptr;}
 void HandleKill(Player*,Unit*);void HandleBotWarPromotion(TeamId,Unit*);void PromotePlayer(Player*);
 void HandlePromotion(Player*,Unit*){++normalKills;}
};
HANDLE
CREDIT
PROMOTE
int main(){Map map;BattlefieldWG wg;Player hero,distant,enemy,outsider;Creature victim;
 hero.map=distant.map=enemy.map=outsider.map=victim.map=&map;distant.x=41;enemy.team=TEAM_HORDE;
 for(Player* p:{&hero,&distant,&enemy,&outsider})p->CastSpell(p,SPELL_RECRUIT,true);
 ObjectAccessor::players={{1,&hero},{2,&distant},{3,&enemy},{4,&outsider}};wg.m_PlayersInWar[0]={1,2,99};wg.m_PlayersInWar[1]={3};
 for(unsigned kill=1;kill<=10;++kill){
  victim.guid=100+kill;
  wg.HandleBotWarPromotion(TEAM_ALLIANCE,&victim); // allied native bot's finishing blow
  wg.HandleKill(&hero,&victim); // direct/pet/reward-share callback for the SAME death
  if(kill==4)assert(hero.GetAura(SPELL_RECRUIT)->stacks==5);
  if(kill==5)assert(hero.GetAura(SPELL_CORPORAL)&&!hero.GetAura(SPELL_RECRUIT));
 }
 assert(hero.GetAura(SPELL_LIEUTENANT)&&!hero.GetAura(SPELL_CORPORAL));
 assert(distant.GetAura(SPELL_RECRUIT)->stacks==1&&enemy.GetAura(SPELL_RECRUIT)->stacks==1&&outsider.GetAura(SPELL_RECRUIT)->stacks==1);
 Player fresh;fresh.map=&map;fresh.CastSpell(&fresh,SPELL_RECRUIT,true);ObjectAccessor::players[1]=&fresh;
 victim.guid=200;victim.faction=1732;wg.HandleKill(&fresh,&victim);assert(fresh.GetAura(SPELL_RECRUIT)->stacks==1);
 victim.faction=1735;victim.ai.controlled=false;wg.HandleKill(&fresh,&victim);assert(fresh.GetAura(SPELL_RECRUIT)->stacks==1);
 victim.ai.controlled=true;victim.zone=0;wg.HandleKill(&fresh,&victim);assert(fresh.GetAura(SPELL_RECRUIT)->stacks==1);
 victim.zone=4197;wg.war=false;wg.HandleKill(&fresh,&victim);assert(fresh.GetAura(SPELL_RECRUIT)->stacks==1);
 wg.war=true;wg.HandleKill(&fresh,&victim);assert(fresh.GetAura(SPELL_RECRUIT)->stacks==2); // pets use this same native entry point
 wg.HandleKill(&fresh,&enemy);assert(wg.normalKills==1); // real-player PvP retains original promotion path
 std::cout<<"PASS: actual native WG promotion gives Corporal at 5 and Lieutenant at 10; player/pet/allied-bot callbacks count once; wrong faction, range, zone, peace and nonparticipants excluded\n";
}
'''.replace('HANDLE',function('void BattlefieldWG::HandleKill(')).replace('CREDIT',function('void BattlefieldWG::HandleBotWarPromotion(')).replace('PROMOTE',function('void BattlefieldWG::PromotePlayer('))
with tempfile.TemporaryDirectory(prefix='wg-player-credit-') as tmp:
 cpp=Path(tmp)/'test.cpp';exe=Path(tmp)/'test';cpp.write_text(code)
 subprocess.run(['c++','-std=c++20','-O2','-Wall','-Wextra','-Werror',str(cpp),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
