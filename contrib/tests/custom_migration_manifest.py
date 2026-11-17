#!/usr/bin/env python3
"""Validate production manifests and prove altered SQL / unresolved dependencies are rejected."""
from pathlib import Path
import importlib.util
import json
import shutil
import sys
import tempfile
sys.dont_write_bytecode = True
root=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('migrations',root/'sql/custom/apply_custom_sql.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
for scope in ('world','characters','auth'):
    rows=module.manifest(scope)
    assert rows
with tempfile.TemporaryDirectory(prefix='custom-manifest-') as temp:
    target=Path(temp);shutil.copytree(root/'sql/custom/characters',target/'characters')
    module.ROOT=target
    module.manifest('characters')
    file=next((target/'characters').glob('*.sql'));original=file.read_bytes();file.write_bytes(original+b'\n-- changed\n')
    try:module.manifest('characters')
    except ValueError:pass
    else:raise AssertionError('Altered migration accepted')
    file.write_bytes(original)
    manifest=target/'characters/manifest.json';rows=json.loads(manifest.read_text());rows[0]['after']=['missing.sql'];manifest.write_text(json.dumps(rows))
    try:module.manifest('characters')
    except ValueError:pass
    else:raise AssertionError('Missing prerequisite accepted')
assert (root/'sql/custom/characters/2026_11_17_00_characters.sql').read_bytes()==(root/'sql/updates/characters/3.3.5/2026_11_17_00_characters.sql').read_bytes()
print('PASS: all three migration manifests, pinned checksums, prerequisites and updater/custom schema identity')
