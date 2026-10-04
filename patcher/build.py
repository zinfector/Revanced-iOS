"""Build RVPort with Xcode clang or a local Zig compiler plus an iOS SDK."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent

def materialize_sdk_file_links(sdk):
    """Git on Windows represents SDK symlinks as tiny text files."""
    if os.name != 'nt' or not (sdk.parent/'.git').exists(): return
    data=subprocess.check_output(['git','-C',str(sdk.parent),'ls-files','-s','-z',sdk.name])
    pending=[]
    for record in data.split(b'\0'):
        if not record:continue
        meta,name=record.split(b'\t',1)
        if not meta.startswith(b'120000 '):continue
        path=sdk.parent/name.decode()
        if path.is_file() and path.stat().st_size < 2048:
            target=(path.parent/path.read_text().strip()).resolve()
            if target.is_file():pending.append((path,target))
    for _ in range(8):
        progress=False
        for path,target in list(pending):
            if any(target==p for p,t in pending):continue
            path.write_bytes(target.read_bytes());pending.remove((path,target));progress=True
        if not progress:break

def build(sdk=None, zig=None):
    out=ROOT/'build';out.mkdir(exist_ok=True)
    from features import native_header
    (ROOT/'native/RVFeatures.h').write_text(native_header(),encoding='utf-8')
    if sys.platform == 'darwin' and not zig:
        sdk=Path(sdk or subprocess.check_output(['xcrun','--sdk','iphoneos','--show-sdk-path'],text=True).strip())
        compiler=['xcrun','--sdk','iphoneos','clang','-target','arm64-apple-ios17.0']
    else:
        sdk=Path(sdk or ROOT/'.tools/sdks/iPhoneOS16.5.sdk').resolve()
        zig=Path(zig or ROOT/'.tools/python/ziglang/zig.exe').resolve()
        if not zig.exists() or not sdk.is_dir():
            raise RuntimeError('Need --zig <zig executable> and --sdk <iPhoneOS SDK>, or build on macOS with Xcode')
        materialize_sdk_file_links(sdk)
        compiler=[str(zig),'cc','-target','aarch64-ios.17.0']
    args=compiler+['-isysroot',str(sdk),'-isystem',str(sdk/'usr/include'),'-L'+str(sdk/'usr/lib'),
        '-F'+str(sdk/'System/Library/Frameworks'),'-F'+str(sdk/'System/Library/PrivateFrameworks'),
        '-fobjc-arc','-fblocks','-O2','-Wall','-Wextra',
        '-Werror','-Wno-unused-parameter','-fvisibility=hidden','-dynamiclib',
        str(ROOT/'native/RVPort.m'),'-o',str(out/'RVPort.dylib'),
        '-framework','Foundation','-framework','UIKit','-framework','AVFoundation','-framework','MediaPlayer','-framework','CoreGraphics','-framework','QuartzCore','-framework','Network','-lobjc',
        '-Wl,-install_name,@executable_path/Frameworks/RVPort.dylib']
    with (out/'build.log').open('w',encoding='utf-8') as log:
        result=subprocess.run(args,stdout=log,stderr=subprocess.STDOUT)
    if result.returncode:
        print((out/'build.log').read_text(),file=sys.stderr)
        raise RuntimeError(f'Compiler exited {result.returncode}')
    from patcher import payload
    binary=payload(out/'RVPort.dylib')
    source=(ROOT/'native/RVPort.m').read_bytes()
    metadata={'payload_sha256':hashlib.sha256(binary).hexdigest(),'source_sha256':hashlib.sha256(source).hexdigest(),
              'source_files':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted((ROOT/'native').glob('*')) if p.is_file()},
              'compiler_command':args,'sdk':str(sdk),'runtime_validated':False}
    (out/'build-manifest.json').write_text(json.dumps(metadata,indent=2),encoding='utf-8')
    print(f'Built {out / "RVPort.dylib"} ({len(binary):,} bytes)')

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--sdk');p.add_argument('--zig');a=p.parse_args()
    try:build(a.sdk,a.zig)
    except (RuntimeError,OSError,ValueError) as ex:print(f'Build error: {ex}',file=sys.stderr);raise SystemExit(2)
