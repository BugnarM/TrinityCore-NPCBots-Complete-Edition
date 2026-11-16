#!/usr/bin/env python3
"""Summarize native Wintergrasp BotWar battle_end log records, without a database."""
import argparse
import csv
import json
import re
from pathlib import Path

TEAM_NAMES = {0: 'Alliance', 1: 'Horde'}

def parse_records(lines):
    records = []
    for line in lines:
        if 'WG BotWar event=battle_end ' not in line:
            continue
        fields = dict(re.findall(r'(\w+)=([^\s]+)', line.split('WG BotWar ', 1)[1]))
        try:
            record = {key: (1 if value == 'true' else 0 if value == 'false' else int(value))
                      for key, value in fields.items() if key != 'event'}
            if record['attacker'] not in TEAM_NAMES or record['winner'] not in TEAM_NAMES:
                continue
            if not record.get('participated', 0):
                continue  # armies were never activated by a real participant
            if record['duration_ms'] < 0:
                continue
        except (KeyError, ValueError):
            continue  # ignore partial/truncated records, never invent a result
        records.append(record)
    return records

def summarize(records):
    result = {'completed_battles': len(records), 'by_attacker': {}}
    for team, name in TEAM_NAMES.items():
        battles = [row for row in records if row['attacker'] == team]
        wins = sum(row['winner'] == team for row in battles)
        result['by_attacker'][name] = {
            'battles': len(battles), 'wins': wins,
            'win_rate': wins / len(battles) if battles else None,
            'mean_duration_seconds': sum(row['duration_ms'] for row in battles) / len(battles) / 1000 if battles else None,
            'vehicles_built': sum(row.get('built_a', 0) + row.get('built_h', 0) for row in battles),
            'vehicles_lost': sum(row.get('lost_a', 0) + row.get('lost_h', 0) for row in battles),
            'vehicles_salvaged': sum(row.get('salvaged_a', 0) + row.get('salvaged_h', 0) for row in battles),
            'stalls': sum(row.get('stalls_a', 0) + row.get('stalls_h', 0) for row in battles),
            'recoveries': sum(row.get('recovered_a', 0) + row.get('recovered_h', 0) for row in battles),
        }
    return result

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('logs', nargs='+', type=Path)
    parser.add_argument('--json', type=Path, dest='json_path')
    parser.add_argument('--csv', type=Path, dest='csv_path')
    args = parser.parse_args()
    records = []
    for path in args.logs:
        with path.open(errors='replace') as stream:
            records.extend(parse_records(stream))
    report = summarize(records)
    print(json.dumps(report, indent=2))
    if args.json_path:
        args.json_path.write_text(json.dumps({'summary': report, 'battles': records}, indent=2) + '\n')
    if args.csv_path:
        fields = sorted(set().union(*(row.keys() for row in records))) if records else ['serial', 'attacker', 'winner', 'duration_ms']
        with args.csv_path.open('w', newline='') as stream:
            writer = csv.DictWriter(stream, fieldnames=fields)
            writer.writeheader()
            writer.writerows(records)

if __name__ == '__main__':
    main()
