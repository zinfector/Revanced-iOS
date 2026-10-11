"""Compile only the fresh adapter host against the generated Android library."""
import argparse,hashlib,json,subprocess
from pathlib import Path

def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def build(source,core,native,out):
    out.mkdir(parents=True,exist_ok=True)
    sdk=subprocess.check_output(['xcrun','--sdk','iphoneos','--show-sdk-path'],text=True).strip()
    target=out/'RVBHost.dylib'
    cmd=['xcrun','--sdk','iphoneos','clang','-target','arm64-apple-ios17.0','-isysroot',sdk,'-fobjc-arc','-fblocks','-O2','-g','-Wall','-Wextra','-Wno-unused-parameter','-I',str(native),str(source),str(core),'-dynamiclib','-framework','Foundation','-framework','UIKit','-framework','Security','-framework','Network','-lobjc','-Wl,-install_name,@executable_path/Frameworks/RVBHost.dylib','-Wl,-headerpad,0x4000','-o',str(target)]
    with (out/'compile-host-adapter.log').open('w') as log:r=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT)
    if r.returncode:raise RuntimeError((out/'compile-host-adapter.log').read_text()[-16000:])
    subprocess.run(['xcrun','dsymutil',str(target),'-o',str(out/'RVBHost.dSYM')],check=True)
    deps=subprocess.check_output(['xcrun','otool','-L',str(target)],text=True)
    if '@executable_path/Frameworks/TranslatedBatch.dylib' not in deps:raise ValueError('Host does not link the translated core')
    (out/'dependencies.txt').write_text(deps)
    (out/'receipt.json').write_text(json.dumps(dict(schema='stateful.batch.app-host.v1',host_sha256=sha(target),core_sha256=sha(core),adapter_source_sha256=sha(source),compiler_command=cmd,legacy_ios_sources_compiled=False,full_comparison_executed=False,device_verified=False),indent=2)+'\n')

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--source',type=Path,required=True);p.add_argument('--core',type=Path,required=True);p.add_argument('--native',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();build(a.source.resolve(),a.core.resolve(),a.native.resolve(),a.out.resolve())
