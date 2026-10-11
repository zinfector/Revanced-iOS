"""Stage a verified generation for independent cloud source/native checks."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
from driver import verify_delivery


def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--input',type=Path,required=True);p.add_argument('--out',type=Path,required=True)
    a=p.parse_args();src=a.input.resolve();out=a.out.resolve()
    if out.exists():raise ValueError('Fresh staging directory required')
    verify_delivery(src);out.mkdir(parents=True);delivery=out/'delivery';delivery.mkdir()
    for folder in ('original','retained','generated','production-java','production-native','provider-probe-java'):
        shutil.copytree(src/folder,delivery/folder)
    for name in ('contracts.json','generation-receipt.json','module.json','original-trace.txt','settings.json','production-contracts.json','object-module.json'):
        shutil.copy2(src/name,delivery/name)
    ledger={p.relative_to(delivery).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in delivery.rglob('*') if p.is_file()}
    (delivery/'delivery-ledger.json').write_text(json.dumps(ledger,indent=2)+'\n',encoding='utf-8',newline='\n')
    tooling=out/'tooling';tooling.mkdir();root=Path(__file__).resolve().parent
    for path in [*root.glob('*.py'),root/'ExportPatch.java']:shutil.copy2(path,tooling/path.name)
    shutil.copy2(root/'native_build.py',out/'native_build.py')
    print('Source, IR, operation adapters and validation tools staged with portable byte hashes')


if __name__=='__main__':main()
