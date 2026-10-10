"""Write a compact receipt after the real corpus validation has completed."""
import argparse
import hashlib
import json
from pathlib import Path
import sys
import unittest

import adaptive
import patcher
from recipe_catalog import digest


def write(folder):
    folder=folder.resolve()
    corpus=json.loads((folder/'corpus-validation.json').read_text())
    cat=adaptive.catalog()
    build=json.loads((patcher.ROOT/'build/build-manifest.json').read_text())
    if patcher.file_sha(patcher.ROOT/'build/RVPort.dylib')!=build['payload_sha256']:
        raise ValueError('Build payload hash mismatch')
    for name,sha in build['source_files'].items():
        if patcher.file_sha(patcher.ROOT/'native'/name)!=sha: raise ValueError('Native source changed after build: '+name)
    rows=[]
    for version in corpus['versions']:
        check=version['archive_validation'];v=version['profile'].split('-')[1]
        profile=json.loads((folder/('profile-'+v+'.json')).read_text())
        if profile['catalog_sha256']!=digest(cat): raise ValueError('Corpus catalog is stale')
        if not all(check.get(key) for key in ('existing_load_commands_preserved','section_and_linkedit_bytes_preserved','zip_crc_verified')):
            raise ValueError('Missing archive invariants')
        target=folder/('YouTube-'+v+'-adaptive-unsigned.ipa')
        if patcher.file_sha(target)!=check['output_ipa_sha256']: raise ValueError('Archive changed after validation')
        rows.append(dict(version=v,executable_sha256=profile['executable_sha256'],
                         resolutions=profile['summary']['resolutions'],disabled_features=profile['disabled_features'],
                         archive=str(target.relative_to(patcher.ROOT)),archive_sha256=check['output_ipa_sha256'],
                         retained_members_identical=check['retained_members_identical']))
    sys.path.insert(0,str(patcher.ROOT/'tests'))
    suite=unittest.defaultTestLoader.discover(str(patcher.ROOT/'tests'))
    with (folder/'unit-tests.txt').open('w') as log:
        result=unittest.TextTestRunner(stream=log,verbosity=2).run(suite)
    if not result.wasSuccessful(): raise ValueError('Unit test failure; see unit-tests.txt')
    receipt=dict(schema=1,release='0.4.0-adaptive',catalog_sha256=digest(cat),
                 payload_sha256=build['payload_sha256'],build_source_sha256=build['build_source_sha256'],
                 contracts=len(cat['contracts']),source_call_sites=cat['source_call_sites'],
                 unit_tests=dict(run=result.testsRun,passed=result.testsRun-len(result.skipped),skipped=len(result.skipped),failures=0,errors=0),
                 versions=rows,signing_required=True,device_validated=False,
                 semantic_limits='Private wire/template/native settings adapters remain baseline-bound; runtime preflight and device testing required')
    (folder/'receipt.json').write_text(json.dumps(receipt,indent=2))
    return receipt


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('folder',type=Path)
    print(json.dumps(write(p.parse_args().folder),indent=2))
