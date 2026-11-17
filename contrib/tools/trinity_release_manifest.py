#!/usr/bin/env python3
"""Record reproducible source, build, schema and installed artifact identities without secrets."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
from datetime import datetime, timezone


def digest(path):
    hasher=hashlib.sha256()
    with path.open('rb') as stream:
        while chunk:=stream.read(1024*1024):
            hasher.update(chunk)
    return hasher.hexdigest()


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build',type=Path,required=True)
    parser.add_argument('--install',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    repo=Path(__file__).resolve().parents[2]
    def git(*cmd):
        return subprocess.check_output(['git',*cmd],cwd=repo,text=True).strip()
    revision=(args.build/'revision_data.h').read_text()
    match=re.search(r'TRINITY_GIT_COMMIT_HASH\s+"([^"]+)"',revision)
    data={'created_utc':datetime.now(timezone.utc).isoformat(),'source_commit':git('rev-parse','HEAD'),
          'source_branch':git('branch','--show-current'),'source_dirty':bool(git('status','--porcelain')),
          'source_diff_sha256':hashlib.sha256(subprocess.check_output(['git','diff','HEAD'],cwd=repo)).hexdigest(),
          'build_revision':match[1] if match else None,'artifacts':{},'migrations':{}}
    for name,path in {'worldserver':args.install/'bin/worldserver','authserver':args.install/'bin/authserver',
                      'worldserver.conf':args.install/'etc/worldserver.conf','CMakeCache.txt':args.build/'CMakeCache.txt'}.items():
        data['artifacts'][name]={'path':str(path),'sha256':digest(path),'bytes':path.stat().st_size}
    for path in sorted((repo/'sql/custom').glob('*/manifest.json')):
        data['migrations'][str(path.relative_to(repo))]=digest(path)
    migration=repo/'sql/updates/characters/3.3.5/2026_11_17_00_characters.sql'
    data['migrations'][str(migration.relative_to(repo))]=digest(migration)
    args.output.write_text(json.dumps(data,indent=2)+'\n')
    print(args.output)


if __name__=='__main__':
    main()
