"""Validate discovery and unsigned packaging against the local original IPAs."""
import argparse
import json
from pathlib import Path
import time
import adaptive
import integration_check
import patcher
from recipe_catalog import canonical


def run(source, out, package):
    out.mkdir(parents=True,exist_ok=True)
    rows=[]
    for ipa in sorted(source.glob('com.google.ios.youtube_*.ipa')):
        start=time.monotonic();p=adaptive.discover(ipa)
        profile=out/('profile-'+p['version']+'.json');profile.write_bytes(canonical(p))
        row=adaptive.explanation(p)
        if package:
            target=out/('YouTube-'+p['version']+'-adaptive-unsigned.ipa')
            patcher.patch(ipa,target,patcher.ROOT/'build/RVPort.dylib',{},adaptive_profile=profile)
            row['archive_validation']=integration_check.check(ipa,target)
        row['seconds']=round(time.monotonic()-start,2);rows.append(row)
        print(p['version'],p['summary']['resolutions'],flush=True)
    result=dict(schema=1,device_validated=False,versions=rows)
    (out/'corpus-validation.json').write_text(json.dumps(result,indent=2))
    if not rows: raise ValueError('No original IPAs found')
    return result


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ipas',type=Path,default=patcher.ROOT.parent/'Ghidra Project')
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--package',action='store_true')
    a=parser.parse_args();run(a.ipas,a.out,a.package)
