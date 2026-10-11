"""Translate checked Java, run native traces, and compile the same code for iOS."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess


def sha(path):return hashlib.sha256(path.read_bytes()).hexdigest()
def run(command, path):
    with path.open('w',encoding='utf-8') as log:
        result=subprocess.run(list(map(str,command)),stdout=log,stderr=subprocess.STDOUT)
    if result.returncode:raise RuntimeError(path.read_text(errors='replace')[-8000:])


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--j2objc-home',type=Path,required=True)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--input',type=Path,default=Path(__file__).resolve().parent/'delivery')
    args=parser.parse_args();home=args.j2objc_home.resolve();out=args.out.resolve();inputs=args.input.resolve()
    if out.exists():raise ValueError('Fresh native output required')
    out.mkdir(parents=True);objc=out/'objc';objc.mkdir()
    prefixes=out/'prefixes.properties'
    prefixes.write_text('app.revanced.extension.youtube.patches.playback.speed=RVSTPolicy\napp.revanced.extension.shared=RVSTShared\napp.revanced.extension.youtube.settings=RVSTSettings\napp.revanced.extension.youtube.patches=RVSTContext\nrv.translated=RVSTTest\nrv.fixture=RVSTFixture\n')
    files=sorted((inputs/'generated').rglob('*.java'))
    if not files:raise ValueError('No graph-generated source')
    command=[home/'j2objc','-source','11','-use-arc','--prefixes',prefixes,'-classpath',home/'lib/j2objc_annotations.jar',
             '-sourcepath',inputs/'generated','-d',objc,*files]
    run(command,out/'translation.log')
    runner=out/'Harness.m';runner.write_text('''#import <Foundation/Foundation.h>
#include "rv/translated/TraceHarness.h"
#include "rv/fixture/ObjectProbe.h"
int main(void) { @autoreleasepool {
    if (RVSTFixtureObjectProbe_run()!=10) return 2;
    NSString *result=RVSTTestTraceHarness_runAll();
    fputs(result.UTF8String,stdout);return 0;
}}
''')
    common=['-fobjc-arc','-fobjc-arc-exceptions','-fblocks','-O2','-I',home/'include','-I',objc,
            *sorted(objc.rglob('*.m')),'-framework','Foundation','-framework','Security','-liconv','-lz','-licucore']
    compile_host=['xcrun','clang',*common,runner,home/'lib/macosx/libjre_core.a','-o',out/'Harness']
    run(compile_host,out/'compile-host.log');run([out/'Harness'],out/'native-trace.txt')
    expected=(inputs/'original-trace.txt').read_text();actual=(out/'native-trace.txt').read_text()
    if expected!=actual:
        import difflib
        (out/'trace-diff.txt').write_text(''.join(difflib.unified_diff(expected.splitlines(True),actual.splitlines(True))))
        raise ValueError('Original JVM versus generated Objective-C state/effect traces differ')
    sdk=subprocess.check_output(['xcrun','--sdk','iphoneos','--show-sdk-path'],text=True).strip()
    output=out/'StatefulPolicy.dylib'
    compile_ios=['xcrun','--sdk','iphoneos','clang','-target','arm64-apple-ios17.0','-isysroot',sdk,*common,
                 home/'lib/iphone/libjre_core.a','-dynamiclib','-Wl,-install_name,@executable_path/Frameworks/StatefulPolicy.dylib','-o',output]
    run(compile_ios,out/'compile-ios.log')
    symbols=subprocess.check_output(['xcrun','nm','-g',output],text=True)
    (out/'symbols.txt').write_text(symbols)
    if '_RVSTPolicyRememberPlaybackSpeedPatch_userSelectedPlaybackSpeedWithFloat_' not in symbols:raise ValueError('Missing translated stateful symbol')
    receipt=dict(schema='stateful-translator.native.v1',source_files={str(p.relative_to(inputs)):sha(p) for p in files},
                 generated_objc={str(p.relative_to(objc)):sha(p) for p in objc.rglob('*') if p.is_file()},
                 generation_receipt_sha256=sha(inputs/'generation-receipt.json'),vendor_translator_sha256=sha(home/'lib/j2objc.jar'),
                 runtime_mac_sha256=sha(home/'lib/macosx/libjre_core.a'),runtime_ios_sha256=sha(home/'lib/iphone/libjre_core.a'),
                 translation_command=list(map(str,command)),host_command=list(map(str,compile_host)),ios_command=list(map(str,compile_ios)),
                 trace_cases=len(expected.splitlines()),jvm_native_traces_equal=True,object_probe_result=10,
                 payload_sha256=sha(output),device_verified=False,app_hooks_installed=False,
                 scope='Generated stateful patch through deterministic operation providers; real native macOS execution and ARM64 iOS compilation')
    (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    (out/'artifact-ledger.json').write_text(json.dumps({str(p.relative_to(out)):sha(p) for p in out.rglob('*') if p.is_file()},indent=2)+'\n')
    print('Generated native state, callback and effect traces equal original JVM:',receipt['trace_cases'],'cases; object probe passed; ARM64 iOS compiled')


if __name__=='__main__':main()
