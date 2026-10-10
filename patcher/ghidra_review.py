"""Export source-linked, hash-bound decompilations for blocked adaptive recipes.

The report supports human adapter review. Ghidra never updates profiles itself.
"""
import argparse
import json
from pathlib import Path
import subprocess
from adaptive import ROOT, explanation


def targets(profile, limit):
    rows=[];seen=set()
    for record in profile['contracts']:
        evidence=record['resolution']
        if evidence['status'] in ('static_match','runtime_descriptor','runtime_inherited','runtime_framework'): continue
        candidates=evidence.get('candidates',[])
        if evidence.get('imp'): candidates.append(evidence)
        if evidence.get('metadata',{}).get('imp'): candidates.append(evidence['metadata'])
        for c in candidates:
            if c['imp'] not in seen:
                rows.append((record['id'],c['imp']));seen.add(c['imp'])
                if len(rows)>=limit: return rows
    return rows


def export(profile_path,out,headless=None,project=None,project_name='ReVanced',limit=8):
    p=json.loads(profile_path.read_text());out.mkdir(parents=True,exist_ok=True)
    rows=targets(p,limit)
    tsv=out/'review-targets.tsv';tsv.write_text(''.join(label+'\t'+address+'\n' for label,address in rows))
    (out/'review-contracts.json').write_text(json.dumps(explanation(p),indent=2))
    if not rows: return dict(status='no_static_candidates',reason='Blocked recipes have no unique static method; inspect descriptor/class/ABI changes',targets=0)
    if headless is None: return dict(status='targets_exported',targets=len(rows),file=str(tsv))
    command=[str(headless.resolve()),str(project.resolve()),project_name,'-process','YouTube','-recursive','-readOnly','-noanalysis',
             '-scriptPath',str(ROOT/'ghidra'),'-postScript','AdaptiveReview.java',str(out.resolve()),str(tsv.resolve()),p['executable_sha256'],
             '-log',str(out.resolve()/'ghidra.log')]
    with (out/'console.txt').open('w') as log:
        result=subprocess.run(command,stdout=log,stderr=subprocess.STDOUT)
    artifact=out/('ghidra-review-'+p['executable_sha256'][:16]+'.txt')
    if result.returncode or not artifact.exists():
        raise RuntimeError('Ghidra did not export matching-program evidence; see console.txt (some IPA folders have no main executable)')
    return dict(status='evidence_exported',targets=len(rows),file=str(artifact),read_only=True)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('profile',type=Path);parser.add_argument('-o','--out',type=Path,required=True)
    parser.add_argument('--headless',type=Path);parser.add_argument('--project',type=Path)
    parser.add_argument('--project-name',default='ReVanced');parser.add_argument('--limit',type=int,default=8)
    a=parser.parse_args()
    if a.limit<1 or a.limit>100: parser.error('--limit must be 1-100')
    if a.headless and not a.project: parser.error('--project is required with --headless')
    print(json.dumps(export(a.profile,a.out,a.headless,a.project,a.project_name,a.limit),indent=2))
