#!/usr/bin/env python3
import math

class MockPlayer:
    def __init__(self, name, x, y, z, phase=1, alive=True, gm=False, friendly=True):
        self.name = name
        self.x = x
        self.y = y
        self.z = z
        self.phase = phase
        self.alive = alive
        self.gm = gm
        self.friendly = friendly

    def get_dist(self, ox, oy, oz):
        return math.sqrt((self.x - ox)**2 + (self.y - oy)**2 + (self.z - oz)**2)

def find_nearest_friendly_player(victim_pos, victim_phase, players, max_dist=50.0):
    vx, vy, vz = victim_pos
    best = None
    best_dist = max_dist
    for p in players:
        if not p.alive or p.gm or p.phase != victim_phase or not p.friendly:
            continue
        d = p.get_dist(vx, vy, vz)
        if d <= best_dist:
            best_dist = d
            best = p
    return best, best_dist

def parse_companion_class(cArg):
    cArg = cArg.lower()
    if cArg in ['dk', 'deathknight', 'death_knight', 'darkknight', 'dark_knight']:
        return 6
    if cArg in ['warrior', 'warr']:
        return 1
    if cArg in ['paladin', 'pala']:
        return 2
    if cArg in ['hunter']:
        return 3
    if cArg in ['rogue']:
        return 4
    if cArg in ['priest']:
        return 5
    if cArg in ['shaman', 'sham']:
        return 7
    if cArg in ['mage']:
        return 8
    if cArg in ['warlock', 'lock']:
        return 9
    if cArg in ['druid']:
        return 11
    return 0

# Test 1: Nearby friendly player receives credit within 50 yards
p1 = MockPlayer('NearbyPlayer', 10, 0, 0, phase=1, alive=True, gm=False, friendly=True)
p_far = MockPlayer('FarPlayer', 60, 0, 0, phase=1, alive=True, gm=False, friendly=True)
p_hostile = MockPlayer('HostilePlayer', 5, 0, 0, phase=1, alive=True, gm=False, friendly=False)
p_dead = MockPlayer('DeadPlayer', 8, 0, 0, phase=1, alive=False, gm=False, friendly=True)
p_gm = MockPlayer('GMPlayer', 7, 0, 0, phase=1, alive=True, gm=True, friendly=True)
p_phased = MockPlayer('PhasedPlayer', 6, 0, 0, phase=2, alive=True, gm=False, friendly=True)

players = [p_far, p_hostile, p_dead, p_gm, p_phased, p1]
target, dist = find_nearest_friendly_player((0,0,0), 1, players, 50.0)
assert target == p1, f'Expected {p1.name}, got {target.name if target else None}'
assert dist == 10.0

# Test 2: Out of range player (>50 yards) receives no credit
target_none, _ = find_nearest_friendly_player((0,0,0), 1, [p_far], 50.0)
assert target_none is None, 'Expected None for out of range player'

# Test 3: Companion class aliases
assert parse_companion_class('dk') == 6
assert parse_companion_class('darkknight') == 6
assert parse_companion_class('dark_knight') == 6
assert parse_companion_class('deathknight') == 6
assert parse_companion_class('pala') == 2
assert parse_companion_class('mage') == 8

print('PASS: Wanderer kill share & companion class parsing verified')
