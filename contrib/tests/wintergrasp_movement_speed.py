#!/usr/bin/env python3
"""Check the production WG speed helper against the core's speed conversion."""
from pathlib import Path
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
zone = (repo / "src/server/scripts/Northrend/zone_wintergrasp.cpp").read_text()
unit = (repo / "src/server/game/Entities/Unit/Unit.cpp").read_text()

def function(text, signature):
    start = text.index(signature)
    end = text.index("{", start) + 1
    depth = 1
    while depth:
        depth += (text[end] == "{") - (text[end] == "}")
        end += 1
    return text[start:end]

helper = function(zone, "void SetBotWarMovementSpeed(Creature* bot, bool mounted)")
get_speed = function(unit, "float Unit::GetSpeed(UnitMoveType mtype) const")
set_speed = function(unit, "void Unit::SetSpeed(UnitMoveType mtype, float newValue)")
base_speeds = unit[unit.index("float baseMoveSpeed[MAX_MOVE_TYPE]"):unit.index("DamageInfo::DamageInfo")]
route_move = function(zone[zone.index("struct BotRouteMovement"):], "    void Move(Creature* bot)")
assert "bot->SetWalk(false)" in route_move
assert "SetSpeed(MOVE_RUN" not in zone, "WG must use a speed multiplier, not 1/2 metres per second"
for target in ("vehicle", "accessory"):
    boarding = zone[:zone.index(f"me->EnterVehicle({target}, 0)")]
    assert "SetBotWarMovementSpeed(me, false)" in boarding[boarding.rfind("me->Dismount()"):]

code = r'''
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
enum UnitMoveType
{
    MOVE_WALK, MOVE_RUN, MOVE_RUN_BACK, MOVE_SWIM, MOVE_SWIM_BACK,
    MOVE_TURN_RATE, MOVE_FLIGHT, MOVE_FLIGHT_BACK, MOVE_PITCH_RATE, MAX_MOVE_TYPE
};
BASE_SPEEDS
struct Unit
{
    std::array<float, MAX_MOVE_TYPE> m_speed_rate{};
    bool controlled=false, walking=true;
    Unit() { m_speed_rate.fill(1.f); }
    bool IsControlledByPlayer() const { return controlled; }
    void SetSpeedRate(UnitMoveType type, float rate) { m_speed_rate[type]=rate; }
    void SetWalk(bool value) { walking=value; }
    float GetSpeed(UnitMoveType type) const;
    void SetSpeed(UnitMoveType type, float value);
};
GET_SPEED
SET_SPEED
struct Creature : Unit { };
HELPER
int main()
{
    for(bool controlled : {false,true})
    {
        Creature bot; bot.controlled=controlled;
        // Reproduce the old absolute-speed error using the core implementation.
        bot.SetSpeed(MOVE_RUN,1.f);
        assert(std::fabs(bot.GetSpeed(MOVE_RUN)-1.f)<0.001f);
        bot.SetSpeed(MOVE_RUN,2.f);
        assert(std::fabs(bot.GetSpeed(MOVE_RUN)-2.f)<0.001f);
        // Spawn/running, mount, dismount for combat/healing/boarding, remount.
        for(bool mounted : {false,true,false,true,false})
        {
            bot.walking=true;
            SetBotWarMovementSpeed(&bot,mounted);
            assert(!bot.walking);
            assert(std::fabs(bot.GetSpeed(MOVE_RUN)-(mounted?14.f:7.f))<0.001f);
        }
    }
    std::cout << "PASS: core speed conversion reproduces 1/2 m/s bug; production WG helper restores 7/14 m/s and running mode, including boarding\n";
}
'''
for marker, value in [("BASE_SPEEDS", base_speeds), ("GET_SPEED", get_speed), ("SET_SPEED", set_speed), ("HELPER", helper)]:
    code = code.replace(marker, value)
with tempfile.TemporaryDirectory(prefix="wg-speed-") as tmp:
    cpp, binary = Path(tmp) / "test.cpp", Path(tmp) / "test"
    cpp.write_text(code)
    subprocess.run(["c++", "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror", str(cpp), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
