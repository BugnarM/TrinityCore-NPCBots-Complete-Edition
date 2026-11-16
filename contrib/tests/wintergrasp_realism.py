#!/usr/bin/env python3
"""Test realism behaviors: workshop defense, last stand, southern spawns, and anti-siege targeting."""
from pathlib import Path
import re
import subprocess
import tempfile

repo = Path(__file__).resolve().parents[2]
source = (repo / "src/server/scripts/Northrend/zone_wintergrasp.cpp").read_text()

# Verify code markers
assert "LAST STAND: If Vault Gate 191810 is destroyed, all keep defenders fall back to Titan Relic!" in source
assert "FORWARD WORKSHOP DEFENSE: KeepWest & KeepEast defend Broken Temple & Sunken Ring" in source
assert "!me->IsInCombat() && me->IsWithinDistInMap(relic, 7.f)" in source
assert "lane == KeepCenter || lane == KeepWest || lane == KeepEast" in source
assert "Bots do not need rank to build siege vehicles" in source
assert "4390.78f, 3304.09f, 372.43f" in source
assert "4398.08f, 2356.50f, 376.19f" in source

print("PASS: static checks for workshop defense, last stand, anti-combat relic channeling, keep vehicles, and southern attacker spawns.")
