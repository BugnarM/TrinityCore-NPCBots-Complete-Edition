#!/usr/bin/env python3
"""Plan/apply checksum-pinned custom SQL migrations, separately for each database."""
import argparse
import gzip
import hashlib
import json
from pathlib import Path
import re
import subprocess
import sys
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parent


def manifest(scope):
    rows = json.loads((ROOT / scope / 'manifest.json').read_text())
    seen = set()
    for row in rows:
        name = row['file']
        if Path(name).name != name or not name.endswith('.sql') or name in seen:
            raise ValueError('Invalid/duplicate migration filename')
        if not set(row.get('after', [])).issubset(seen):
            raise ValueError(f'Unresolved migration dependency: {name}')
        data = (ROOT / scope / name).read_bytes()
        if hashlib.sha256(data).hexdigest() != row['sha256']:
            raise ValueError(f'Checksum differs from manifest: {name}')
        seen.add(name)
    if seen != {p.name for p in (ROOT / scope).glob('*.sql')}:
        raise ValueError('SQL files and manifest differ; register the new migration explicitly')
    return rows


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--scope', choices=['world', 'characters', 'auth'], required=True)
    parser.add_argument('--database', required=True)
    parser.add_argument('--defaults-file', type=Path, help='Protected MySQL client option file; passwords are never CLI arguments')
    parser.add_argument('--apply', action='store_true', help='Apply pending migrations; otherwise print the plan')
    parser.add_argument('--baseline-existing', action='store_true', help='Record verified pre-existing migrations without replaying their SQL')
    parser.add_argument('--backup-dir', type=Path, help='Required for writes; creates a complete compressed database backup first')
    args = parser.parse_args()
    if not re.fullmatch(r'[A-Za-z0-9_]+', args.database):
        parser.error('Database name must contain only letters, digits and underscores')
    if args.baseline_existing and not args.apply:
        parser.error('--baseline-existing requires --apply')
    rows = manifest(args.scope)
    defaults = []
    if args.defaults_file:
        path = args.defaults_file.resolve()
        if path.stat().st_mode & 0o077:
            parser.error('--defaults-file must not be accessible to group/others (chmod 600)')
        defaults = [f'--defaults-extra-file={path}']
    mysql = ['mysql', *defaults, '--batch', '--raw', '--skip-column-names', args.database]
    exists = subprocess.check_output(mysql, input=b"SHOW TABLES LIKE 'npcbot_custom_migrations';\n")
    applied = {}
    if exists.strip():
        output = subprocess.check_output(mysql, input=b'SELECT filename, sha256, status FROM npcbot_custom_migrations;\n').decode()
        for line in output.splitlines():
            name, checksum, status = line.split('\t')
            applied[name] = (checksum, status)
    pending = []
    for row in rows:
        prior = applied.get(row['file'])
        if prior and prior != (row['sha256'], 'applied'):
            raise ValueError(f"Migration changed or incomplete: {row['file']}; inspect/restore before retry")
        if not prior:
            pending.append(row)
    for row in pending:
        print(('BASELINE' if args.baseline_existing else 'APPLY') + ' ' + row['file'])
    if not args.apply or not pending:
        return
    if not args.backup_dir:
        parser.error('--backup-dir is required before any database write')
    args.backup_dir.mkdir(parents=True, exist_ok=True, mode=0o700)
    stamp = datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%S%fZ')
    backup = args.backup_dir / f'{args.database}-{stamp}.sql.gz'
    with backup.open('xb') as raw:
        backup.chmod(0o600)
        with gzip.GzipFile(fileobj=raw, mode='wb') as archive:
            proc = subprocess.Popen(['mysqldump', *defaults, '--single-transaction', '--routines', '--events', '--hex-blob', args.database], stdout=subprocess.PIPE)
            while chunk := proc.stdout.read(1024 * 1024):
                archive.write(chunk)
            if proc.wait():
                raise RuntimeError('Backup failed; no migrations applied')
    # Read through gzip to verify the trailer/CRC before the first write.
    with gzip.open(backup, 'rb') as archive:
        while archive.read(1024 * 1024):
            pass
    checksum = hashlib.sha256(backup.read_bytes()).hexdigest()
    backup.with_suffix(backup.suffix + '.sha256').write_text(checksum + '\n')
    print('Backup:', backup)
    sql = bytearray(b"SET autocommit=1;\nSET SESSION sql_mode='STRICT_ALL_TABLES';\nCREATE TEMPORARY TABLE _npcbot_migration_lock (held INT NOT NULL);\n")
    lock = f'npcbots:{args.database}'
    sql.extend(f"INSERT INTO _npcbot_migration_lock VALUES (IF(GET_LOCK('{lock}', 0)=1,1,NULL));\n".encode())
    sql.extend(b"CREATE TABLE IF NOT EXISTS npcbot_custom_migrations (filename VARCHAR(190) NOT NULL PRIMARY KEY, sha256 CHAR(64) NOT NULL, status VARCHAR(16) NOT NULL, applied_at TIMESTAMP NULL) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;\n")
    for row in pending:
        name = row['file'].replace("'", "''")
        digest = row['sha256']
        sql.extend(f"INSERT INTO npcbot_custom_migrations(filename,sha256,status) VALUES ('{name}','{digest}','running');\n".encode())
        if not args.baseline_existing:
            sql.extend((ROOT / args.scope / row['file']).read_bytes())
            sql.extend(b'\n')
        sql.extend(f"COMMIT; SET autocommit=1;\nUPDATE npcbot_custom_migrations SET status='applied', applied_at=CURRENT_TIMESTAMP WHERE filename='{name}';\n".encode())
    sql.extend(f"DO RELEASE_LOCK('{lock}');\n".encode())
    subprocess.run(mysql, input=sql, check=True)


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
