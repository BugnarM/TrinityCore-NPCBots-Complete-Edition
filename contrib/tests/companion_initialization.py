#!/usr/bin/env python3
"""Exercise the NPCBot initialization path for spawnless companions and wanderers."""

from pathlib import Path
import subprocess
import tempfile


root = Path(__file__).resolve().parents[2]
source = (root / 'src/server/game/AI/NpcBots/bot_ai.cpp').read_text()
start = source.index('void bot_ai::InitializeAI()')
end = source.index('void bot_ai::BotSay(', start)

harness = r'''
#include <cassert>

struct Creature
{
    unsigned spawnId = 0;
    unsigned entry = 73255;
    bool summon = false;

    unsigned GetSpawnId() const { return spawnId; }
    unsigned GetEntry() const { return entry; }
    bool IsSummon() const { return summon; }
};

struct BotDataMgr
{
    static void const* GetBotExtraCreatureTemplate(unsigned entry)
    {
        static int generatedTemplate = 0;
        return entry == 971000 ? &generatedTemplate : nullptr;
    }
};

struct BotData { unsigned owner; };

struct bot_ai
{
    Creature* me;
    BotData const* _botData = nullptr;
    bool temporary = false;
    bool wanderer = false;
    bool generatedWandererGear = false;
    bool reset = false;

    bool IsTempBot() const { return temporary; }
    void SetWanderer() { wanderer = true; }
    void Reset() { reset = true; generatedWandererGear = wanderer; }
    void InitializeAI();
};

INITIALIZE_AI

int main()
{
    Creature companion;
    BotData unowned{0};
    bot_ai fresh{&companion, &unowned};
    fresh.InitializeAI();
    assert(fresh.reset && !fresh.wanderer && !fresh.generatedWandererGear);

    BotData owned{22};
    bot_ai restored{&companion, &owned};
    restored.InitializeAI();
    assert(restored.reset && !restored.wanderer && !restored.generatedWandererGear);

    Creature generated{0, 971000};
    bot_ai wanderer{&generated, &unowned};
    wanderer.InitializeAI();
    assert(wanderer.reset && wanderer.wanderer && wanderer.generatedWandererGear);

    bot_ai temp{&generated, &unowned, true};
    temp.InitializeAI();
    assert(temp.reset && !temp.wanderer);

    generated.summon = true;
    bot_ai summon{&generated, &unowned};
    summon.InitializeAI();
    assert(summon.reset && !summon.wanderer);
}
'''.replace('INITIALIZE_AI', source[start:end])

with tempfile.TemporaryDirectory(prefix='companion-initialization-') as temp:
    cpp = Path(temp) / 'test.cpp'
    exe = Path(temp) / 'test'
    cpp.write_text(harness)
    subprocess.run(['c++', '-std=c++20', '-Wall', '-Wextra', '-Werror', str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
