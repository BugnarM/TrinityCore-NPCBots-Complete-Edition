#!/usr/bin/env python3
"""Run standalone policy suites and all custom Python regression entrypoints."""
import argparse
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import subprocess


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--jobs',type=int,default=2)
    parser.add_argument('--output-dir',type=Path,default=Path('/tmp/trinity-custom-audit-tests'))
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[2]
    args.output_dir.mkdir(parents=True,exist_ok=True)
    cpp=['arena_bots','siege_battlegrounds','wintergrasp_bot_war','dungeon_finder_bots',
         'hazard_avoidance_and_autoequip_test','raid_finder_test','dynamic_zone_test',
         'city_raid_tactics','wintergrasp_tactics','wintergrasp_vehicle_cleanup','npcbot_player_limits','healer_policy']
    cases=[root/'contrib/tests'/(name+'.cpp') for name in cpp]+sorted((root/'contrib/tests').glob('*.py'))
    def run(path):
        binary=args.output_dir/path.stem
        commands=[['c++','-std=c++20','-O2','-Wall','-Wextra','-Werror',str(path),'-o',str(binary)],[str(binary)]] if path.suffix=='.cpp' else [['python3',str(path)]]
        log=''
        for command in commands:
            try:
                result=subprocess.run(command,cwd=root,text=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=180)
                log+=result.stdout
                if result.returncode:break
            except subprocess.TimeoutExpired:
                (args.output_dir/(path.name+'.log')).write_text(log+'TIMEOUT\n')
                return path.name,124
        (args.output_dir/(path.name+'.log')).write_text(log)
        return path.name,result.returncode
    with ThreadPoolExecutor(max_workers=max(1,min(args.jobs,4))) as executor:
        results=list(executor.map(run,cases))
    summary='\n'.join(f'{name}: {status}' for name,status in results)+'\n'
    (args.output_dir/'summary.txt').write_text(summary)
    for name,status in results:print('PASS' if status==0 else 'FAIL',name)
    print(f'{sum(status==0 for _,status in results)}/{len(results)} passed')
    return int(any(status for _,status in results))


if __name__=='__main__':
    raise SystemExit(main())
