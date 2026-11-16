#!/usr/bin/env python3
"""Validate completed-match filtering, win rates, and exported telemetry artifacts."""
from pathlib import Path
import csv
import importlib.util
import json
import subprocess
import tempfile
repo=Path(__file__).resolve().parents[2]
tool=repo/'contrib/tools/wintergrasp_battle_report.py'
spec=importlib.util.spec_from_file_location('wg_battle_report',tool)
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
lines=[
 'INFO WG BotWar event=battle_end serial=1 attacker=0 winner=0 participated=true duration_ms=900000 built_a=6 built_h=4 salvaged_a=2',
 'INFO WG BotWar event=battle_end serial=2 attacker=0 winner=1 participated=true duration_ms=1800000 lost_a=3 lost_h=2 stalls_h=4',
 'INFO WG BotWar event=battle_end serial=3 attacker=1 winner=1 participated=1 duration_ms=600000',
 'INFO WG BotWar event=battle_end serial=4 attacker=0 winner=0 participated=false duration_ms=1800000',
 'INFO WG BotWar event=progress serial=3 elapsed_ms=600000',
 'INFO WG BotWar event=battle_end serial=5 attacker=0 participated=true duration_ms=bad',
 'INFO WG BotWar event=battle_end serial=6 attacker=2 winner=0 participated=true duration_ms=20',
]
records=module.parse_records(lines);assert len(records)==3
summary=module.summarize(records)
a,h=summary['by_attacker']['Alliance'],summary['by_attacker']['Horde']
assert a['battles']==2 and a['wins']==1 and a['win_rate']==0.5 and a['mean_duration_seconds']==1350
assert a['vehicles_built']==10 and a['vehicles_lost']==5 and a['vehicles_salvaged']==2
assert h['battles']==1 and h['wins']==1 and h['win_rate']==1
assert module.summarize([])['by_attacker']['Horde']['win_rate'] is None
with tempfile.TemporaryDirectory(prefix='wg-battle-report-') as temp:
    temp=Path(temp);log=temp/'Server.log';j=temp/'report.json';c=temp/'report.csv'
    log.write_text('\n'.join(lines))
    subprocess.run(['python3',str(tool),str(log),'--json',str(j),'--csv',str(c)],check=True,stdout=subprocess.DEVNULL)
    assert json.loads(j.read_text())['summary']==summary
    with c.open() as stream: assert len(list(csv.DictReader(stream)))==3
print('PASS: native battle logs exclude unattended/partial records, compute both attacker win rates and export JSON/CSV')
