"""Execute untouched source algorithms and IR output against identical operation fixtures."""
import argparse
import difflib
import json
import os
import re
import shutil
from pathlib import Path
import batch
import regions
import lowering


def oracle_sources(generation,root):
    module=json.loads((generation/'attributed.json').read_text(encoding='utf-8'))
    edits={}
    for n in regions.walk(module):
        if n['kind']!='METHOD_INVOCATION':continue
        target=n['target'];origin=n['origin']
        if target=='java.lang.System#currentTimeMillis()':replacement='rv.batch.Platform.wallMillis()'
        elif target=='java.net.URL#openConnection()':
            receiver=n['receiver']['origin'];source=Path(origin['source']).read_text(encoding='utf-8')
            # Javac source positions count UTF-16 code units, not Python code points.
            raw=source.encode('utf-16-le');text=raw[receiver['start']*2:receiver['end']*2].decode('utf-16-le')
            replacement='rv.batch.Network.open('+text+')'
        else:continue
        edits.setdefault(origin['source'],[]).append((origin['start'],origin['end'],replacement))
    ledger=[]
    shutil.copytree(generation/'portable-src',root)
    for p in sorted((generation/'original').rglob('*.java')):
        text=p.read_text(encoding='utf-8');raw=text.encode('utf-16-le')
        for start,end,replacement in sorted(set(edits.get(str(p.resolve()),[])),reverse=True):
            before=raw[start*2:end*2].decode('utf-16-le')
            raw=raw[:start*2]+replacement.encode('utf-16-le')+raw[end*2:]
            ledger.append(dict(file=p.relative_to(generation/'original').as_posix(),start=start,end=end,original=before,replacement=replacement))
        text=raw.decode('utf-16-le')
        for old,new in sorted(lowering.ALIASES.items(),key=lambda x:-len(x[0])):
            text=re.sub(r'\b'+re.escape(old)+r'\b',new,text)
        batch.write(root/p.relative_to(generation/'original'),text)
    return ledger


def validate(generation,out):
    if out.exists():raise ValueError('Fresh validation output required')
    out.mkdir(parents=True)
    ledger=oracle_sources(generation,out/'oracle-src')
    shutil.copytree(generation/'portable-src',out/'generated-src')
    harness=batch.ROOT/'tests/BatchHarness.java'
    outputs={}
    for label in ('oracle','generated'):
        src=out/(label+'-src');batch.write(src/'rv/batch/test/BatchHarness.java',harness.read_text(encoding='utf-8'))
        batch.compile_sources(src,out/(label+'-classes'),out/(label+'-javac.log'),str(batch.JSON_JAR))
        batch.run(['java','-cp',str(out/(label+'-classes'))+os.pathsep+str(batch.JSON_JAR),'rv.batch.test.BatchHarness'],out/(label+'-trace.txt'))
        outputs[label]=(out/(label+'-trace.txt')).read_text(encoding='utf-8')
    if outputs['oracle']!=outputs['generated']:
        batch.write(out/'trace-diff.txt',''.join(difflib.unified_diff(outputs['oracle'].splitlines(True),outputs['generated'].splitlines(True))))
        raise ValueError('Source and IR behavior traces differ')
    receipt=dict(schema='stateful.batch.behavior.v1',source_generated_traces_equal=True,trace_cases=len(outputs['oracle'].splitlines()),source_transport_clock_edits=ledger,
                 source_algorithms_retained=True,operation_fixture='Deterministic Java facade and recorded operations; not Android runtime or live iOS',device_verified=False)
    batch.save(out/'receipt.json',receipt);print(json.dumps({k:v for k,v in receipt.items() if k!='source_transport_clock_edits'},indent=2))


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--generation',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();validate(a.generation.resolve(),a.out.resolve())
