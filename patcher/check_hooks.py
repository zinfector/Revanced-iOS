"""Compare literal hook declarations with the read-only extracted Objective-C metadata.

This is a static check of directly named hooks, not a runtime installation/device test.
Loop-generated hooks, inherited methods and GPB dynamic accessors require runtime diagnostics.
"""
import json
from pathlib import Path
import re
import sys

ROOT=Path(__file__).resolve().parent
def check():
    metadata=ROOT.parent/'analysis/evidence/objc_classes.json'
    if not metadata.exists():metadata=ROOT/'profiles/literal-hook-metadata.json'
    classes={c['name']:c for c in json.loads(metadata.read_text(encoding='utf-8'))}
    source='\n'.join(p.read_text(encoding='utf-8') for p in (ROOT/'native').glob('*') if p.suffix in ('.m','.inc'))
    declarations=[(a,b,c,'+' if kind=='YES' else '-') for a,b,c,kind in re.findall(r'RVHook\(@"([^"]+)",\s*@"([^"]+)",\s*@"([^"]+)",\s*(YES|NO)',source)]
    declarations += [(a,b,'B@:','-') for a,b in re.findall(r'RVBoolGate\(@"([^"]+)",\s*@"([^"]+)"',source)]
    for getters,cls,signature,kind in re.findall(r'for \(NSString \*getter in @\[(.*?)\]\) \{\s*RVHook\(@"([^"]+)",getter,@"([^"]+)",(YES|NO)',source,re.S):
        declarations += [(cls,sel,signature,'+' if kind=='YES' else '-') for sel in re.findall(r'@"([^"]+)"',getters)]
    rows=[]
    for cls,sel,signature,kind in declarations:
        methods=[m for m in classes.get(cls,{}).get('methods',[]) if m['selector']==sel and m['kind']==kind]
        encodings=[re.sub(r'\d+','',m['encoding']) for m in methods]
        opposite=any(m['selector']==sel and m['kind']!=kind for m in classes.get(cls,{}).get('methods',[]))
        status='match' if signature in encodings else 'mismatch' if encodings or opposite else 'runtime_resolution_required'
        rows.append(dict(class_name=cls,selector=sel,kind=kind,expected=signature,metadata_encodings=encodings,status=status))
    report={'scope':'Literal RVHook/RVBoolGate calls and literal getter arrays. Other loops and runtime-resolved accessors require device diagnostics.',
            'device_validated':False,'counts':{s:sum(r['status']==s for r in rows) for s in ('match','mismatch','runtime_resolution_required')},'hooks':rows}
    (ROOT/'build').mkdir(exist_ok=True)
    (ROOT/'build/hook-check.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
    print(json.dumps(report['counts']))
    for r in rows:
        if r['status']!='match':print(r)
    return 1 if report['counts']['mismatch'] else 0
if __name__=='__main__':sys.exit(check())
