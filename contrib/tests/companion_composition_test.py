#!/usr/bin/env python3
"""Exercise the C++ party and cooldown policies shared by item and command."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
test = r'''
#include "CompanionPartyPolicy.h"
#include <cassert>
using namespace NPCBots::Companions;

int main()
{
    static_assert(MaxCompanions == 4);
    static_assert(NextRole(0, 0) == Role::Healer);
    static_assert(NextRole(1, 0) == Role::Tank);
    static_assert(NextRole(1, 1) == Role::Damage);
    static_assert(NextRole(0, 1) == Role::Healer);
    static_assert(!CanFill(Role::Healer, false, true));
    static_assert(!CanFill(Role::Tank, true, false));
    static_assert(CanFill(Role::Tank, true, true));
    static_assert(CanFill(Role::Damage, false, false));
    static_assert(CooldownRemaining(100, 0, 18000) == 0);
    static_assert(CooldownRemaining(100, 100, 18000) == 18000);
    static_assert(CooldownRemaining(110, 100, 18000) == 17990);
    static_assert(CooldownRemaining(18100, 100, 18000) == 0);
    static_assert(CooldownRemaining(100, 101, 18000) == 18000);
    static_assert(CooldownRemaining(100, 100, 0) == 0);
}
'''
with tempfile.TemporaryDirectory(prefix='companion-policy-') as directory:
    cpp = Path(directory) / 'test.cpp'
    exe = Path(directory) / 'test'
    cpp.write_text(test)
    subprocess.run(['c++', '-std=c++20', '-Wall', '-Wextra', '-Werror',
                    '-I', str(root / 'src/server/scripts/Custom'), str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('PASS: production companion role and cooldown policies')
