"""Build the generated batch with a pinned J2ObjC runtime on macOS/Xcode."""
import argparse
import hashlib
import json
import subprocess
from pathlib import Path


def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(cmd,log):
    with log.open('w',encoding='utf-8') as f:r=subprocess.run(list(map(str,cmd)),stdout=f,stderr=subprocess.STDOUT)
    if r.returncode:raise RuntimeError(log.read_text(encoding='utf-8',errors='replace')[-14000:])


def build(home,inputs,native,out):
    if out.exists():raise ValueError('Fresh native build directory required')
    out.mkdir(parents=True);objc=out/'objc';objc.mkdir()
    files=sorted(inputs.rglob('*.java'));packages={p.read_text(encoding='utf-8').split('package ',1)[1].split(';',1)[0].strip() for p in files}
    prefixes=out/'prefixes.properties';prefixes.write_text('\n'.join(p+'='+('RVB' if p=='rv.batch' else 'RVB'+hashlib.sha256(p.encode()).hexdigest()[:8].upper()) for p in sorted(packages))+'\n',encoding='utf-8')
    command=[home/'j2objc','-source','17','-use-arc','--prefixes',prefixes,'-classpath',str(home/'lib/json.jar')+':'+str(home/'lib/j2objc_annotations.jar'),'-sourcepath',inputs,'-d',objc,*files]
    run(command,out/'translation.log')
    test_package='rv.batch.test';test_prefix='RVB'+hashlib.sha256(test_package.encode()).hexdigest()[:8].upper()
    with prefixes.open('a',encoding='utf-8') as f:f.write(test_package+'='+test_prefix+'\n')
    test_objc=out/'test-objc';test_objc.mkdir()
    tests=inputs.parent/'tests'
    run([home/'j2objc','-source','17','-use-arc','--prefixes',prefixes,'-classpath',str(home/'lib/json.jar'),'-sourcepath',inputs,'-d',test_objc,tests/'BatchHarness.java'],out/'test-translation.log')
    runner=out/'Probe.m';runner.write_text('''#import <Foundation/Foundation.h>
#include "Operations.h"
#include "rv/batch/Events.h"
#include "java/lang/Boolean.h"
#include "rv/batch/test/BatchHarness.h"
int main(void){@autoreleasepool{
 NSUserDefaults *p=[[NSUserDefaults alloc] initWithSuiteName:@"org.revanced.batch.native-probe"];
 [p removePersistentDomainForName:@"org.revanced.batch.native-probe"];
 RVBConfigure(p,^(NSString *k,id f){return f;},^(NSString *op,IOSObjectArray *a){return (id)nil;},^(NSString *k,IOSObjectArray *a){return k;});
 if(!RVBReady()||RVBEvents_showVideoAds())return 2;
 RVBEvents_playerStateWithNSString_withBoolean_(@"WATCH_WHILE_MAXIMIZED",NO);
 if(!RVBEvents_allowBackgroundWithBoolean_(NO))return 3;
 RVBEvents_playerStateWithNSString_withBoolean_(@"WATCH_WHILE_MAXIMIZED",YES);
 if(RVBEvents_allowBackgroundWithBoolean_(NO))return 4;
 [p removePersistentDomainForName:@"org.revanced.batch.native-probe"];
 NSString *trace=TEST_PREFIXBatchHarness_run();fputs(trace.UTF8String,stdout);return 0;
}}
'''.replace('TEST_PREFIX',test_prefix),encoding='utf-8')
    common=['-fobjc-arc','-fobjc-arc-exceptions','-fblocks','-O2','-I',home/'include','-I',objc,'-I',native,*sorted(objc.rglob('*.m')),*sorted(native.glob('*.m')),'-framework','Foundation','-framework','Security','-liconv','-lz','-licucore']
    host=['xcrun','clang',*common,'-I',test_objc,*sorted(test_objc.rglob('*.m')),runner,home/'lib/macosx/libjre_emul.a',home/'lib/macosx/libjson.a','-o',out/'Probe']
    run(host,out/'compile-host.log');run([out/'Probe'],out/'provider-probe.txt')
    expected=inputs.parent/'original-trace.txt'
    if (out/'provider-probe.txt').read_text(encoding='utf-8')!=expected.read_text(encoding='utf-8'):raise ValueError('Original source versus native generated batch traces differ')
    sdk=subprocess.check_output(['xcrun','--sdk','iphoneos','--show-sdk-path'],text=True).strip()
    payload=out/'TranslatedBatch.dylib'
    ios=['xcrun','--sdk','iphoneos','clang','-target','arm64-apple-ios17.0','-isysroot',sdk,*common,home/'lib/iphone/libjre_emul.a',home/'lib/iphone/libjson.a','-framework','UIKit','-dynamiclib','-Wl,-install_name,@executable_path/Frameworks/TranslatedBatch.dylib','-o',payload]
    run(ios,out/'compile-ios.log')
    symbols=subprocess.check_output(['xcrun','nm','-g',payload],text=True);(out/'symbols.txt').write_text(symbols,encoding='utf-8')
    if '_RVBEvents_beginVideoWithNSString_' not in symbols:raise ValueError('Missing generated event placement API')
    receipt=dict(schema='stateful.batch.native.v1',java_files={str(p.relative_to(inputs)):sha(p) for p in files},objc_files={str(p.relative_to(objc)):sha(p) for p in objc.rglob('*') if p.is_file()},payload_sha256=sha(payload),translation_command=list(map(str,command)),host_command=list(map(str,host)),ios_command=list(map(str,ios)),arm64_ios_compiled=True,provider_probe_passed=True,source_jvm_native_traces_equal=True,trace_cases=len(expected.read_text(encoding='utf-8').splitlines()),app_hooks_installed=False,device_verified=False)
    (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--j2objc-home',type=Path,required=True);p.add_argument('--input',type=Path,required=True);p.add_argument('--native',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();build(a.j2objc_home.resolve(),a.input.resolve(),a.native.resolve(),a.out.resolve())
