"""Build the generated batch with a pinned J2ObjC runtime on macOS/Xcode."""
import argparse
import hashlib
import json
import subprocess
import difflib
from pathlib import Path


def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def run(cmd,log):
    with log.open('w',encoding='utf-8') as f:r=subprocess.run(list(map(str,cmd)),stdout=f,stderr=subprocess.STDOUT)
    if r.returncode:raise RuntimeError(f'Command exited with {r.returncode}; log: {log}\n'+log.read_text(encoding='utf-8',errors='replace')[-14000:])


ICU_COMMIT='8eca245c7484ac6cc179e3e5f7c1ea7680810f39'


def build(home,inputs,native,out,icu):
    icu_common=icu/'icu4c/source/common'
    icu_i18n=icu/'icu4c/source/i18n'
    if subprocess.check_output(['git','-C',str(icu),'rev-parse','HEAD'],text=True).strip()!=ICU_COMMIT:
        raise ValueError('Unexpected ICU headers revision')
    if not (icu_i18n/'unicode/unum.h').is_file():raise ValueError('Missing pinned ICU number-format headers')
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
#include "java/lang/Long.h"
#include "java/lang/Integer.h"
#include "java/lang/Double.h"
#include "java/lang/Throwable.h"
#include "IOSObjectArray.h"
#include "NSObject+JavaObject.h"
#include "Format.h"
#include "rv/batch/test/BatchHarness.h"
static NSString *format(id value,NSString *style,int min,int max){
 id args[]={value,@"en_US",style,JavaLangInteger_valueOfWithInt_(min),JavaLangInteger_valueOfWithInt_(max),nil};
 BOOL handled=NO;NSString *s=RVBFormat(@"format.number",[IOSObjectArray arrayWithObjects:args count:6 type:NSObject_class_()],&handled);
 return handled?s:nil;
}
int main(void){@autoreleasepool{
 NSDictionary *formats=@{@"exact_int64":format(JavaLangLong_valueOfWithLong_(9007199254740993LL),@"decimal",0,0),@"compact":format(JavaLangLong_valueOfWithLong_(1500),@"compact_short",1,1),@"percent":format(JavaLangDouble_valueOfWithDouble_(0.125),@"percent",1,1)};
 if(![formats[@"exact_int64"] isEqual:@"9,007,199,254,740,993"]||![formats[@"compact"] isEqual:@"1.5K"]||![formats[@"percent"] isEqual:@"12.5%"]){NSLog(@"ICU probe mismatch: %@",formats);return 5;}
 [[NSJSONSerialization dataWithJSONObject:formats options:NSJSONWritingPrettyPrinted error:NULL] writeToFile:@"FORMAT_PROBE_PATH" atomically:YES];
 NSUserDefaults *p=[[NSUserDefaults alloc] initWithSuiteName:@"org.revanced.batch.native-probe"];
 [p removePersistentDomainForName:@"org.revanced.batch.native-probe"];
 RVBConfigure(p,^(NSString *k,id f){return f;},^(NSString *op,IOSObjectArray *a){return (id)nil;},^(NSString *k,IOSObjectArray *a){return k;});
 if(!RVBReady()||RVBEvents_showVideoAds())return 2;
 RVBEvents_playerStateWithNSString_withBoolean_(@"WATCH_WHILE_MAXIMIZED",NO);
 if(!RVBEvents_allowBackgroundWithBoolean_(NO))return 3;
 RVBEvents_playerStateWithNSString_withBoolean_(@"WATCH_WHILE_MAXIMIZED",YES);
 if(RVBEvents_allowBackgroundWithBoolean_(NO))return 4;
 [p removePersistentDomainForName:@"org.revanced.batch.native-probe"];
 @try{NSString *trace=TEST_PREFIXBatchHarness_run();fputs(trace.UTF8String,stdout);return 0;}
 @catch(JavaLangThrowable *e){fputs(TEST_PREFIXBatchHarness_diagnosticTrace().UTF8String,stdout);[e printStackTrace];return 7;}
}}
'''.replace('TEST_PREFIX',test_prefix).replace('FORMAT_PROBE_PATH',str(out/'formatter-probe.json')),encoding='utf-8')
    # Apple exports unsuffixed ICU C symbols. Header-only upstream ICU normally
    # renames them by release; do not link those version-suffixed declarations.
    common=['-fobjc-arc','-fobjc-arc-exceptions','-fblocks','-O2','-g','-Wno-parentheses-equality','-DU_DISABLE_RENAMING=1','-DU_SHOW_CPLUSPLUS_API=0','-I',icu_i18n,'-I',icu_common,'-I',home/'include','-I',objc,'-I',native,*sorted(objc.rglob('*.m')),*sorted(native.glob('*.m')),'-framework','Foundation','-framework','Security','-liconv','-lz','-licucore']
    host=['xcrun','clang',*common,'-I',test_objc,*sorted(test_objc.rglob('*.m')),runner,home/'lib/macosx/libjre_emul.a',home/'lib/macosx/libjson.a','-o',out/'Probe']
    run(host,out/'compile-host.log')
    probe_error=None
    try:run([out/'Probe'],out/'provider-probe.txt')
    except RuntimeError as e:probe_error=e
    sdk=subprocess.check_output(['xcrun','--sdk','iphoneos','--show-sdk-path'],text=True).strip()
    payload=out/'TranslatedBatch.dylib'
    ios=['xcrun','--sdk','iphoneos','clang','-target','arm64-apple-ios17.0','-isysroot',sdk,*common,home/'lib/iphone/libjre_emul.a',home/'lib/iphone/libjson.a','-framework','UIKit','-dynamiclib','-Wl,-install_name,@executable_path/Frameworks/TranslatedBatch.dylib','-o',payload]
    run(ios,out/'compile-ios.log')
    if probe_error:raise probe_error
    expected=inputs.parent/'original-trace.txt'
    actual_trace=(out/'provider-probe.txt').read_text(encoding='utf-8')
    expected_trace=expected.read_text(encoding='utf-8')
    if actual_trace!=expected_trace:
        diff=''.join(difflib.unified_diff(expected_trace.splitlines(True),actual_trace.splitlines(True),fromfile='original JVM',tofile='generated native'))
        (out/'behavior-diff.txt').write_text(diff,encoding='utf-8')
        raise ValueError('Original source versus native generated batch traces differ:\n'+diff[:8000])
    symbols=subprocess.check_output(['xcrun','nm','-g',payload],text=True);(out/'symbols.txt').write_text(symbols,encoding='utf-8')
    if '_RVBEvents_beginVideoWithNSString_' not in symbols:raise ValueError('Missing generated event placement API')
    receipt=dict(schema='stateful.batch.native.v1',java_files={str(p.relative_to(inputs)):sha(p) for p in files},objc_files={str(p.relative_to(objc)):sha(p) for p in objc.rglob('*') if p.is_file()},payload_sha256=sha(payload),translation_command=list(map(str,command)),host_command=list(map(str,host)),ios_command=list(map(str,ios)),arm64_ios_compiled=True,provider_probe_passed=True,source_jvm_native_traces_equal=True,trace_cases=len(expected.read_text(encoding='utf-8').splitlines()),app_hooks_installed=False,device_verified=False)
    (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n',encoding='utf-8')
    (out/'icu-headers.json').write_text(json.dumps(dict(repository='https://github.com/unicode-org/icu',commit=ICU_COMMIT,unum_header_sha256=sha(icu_i18n/'unicode/unum.h'),disable_symbol_renaming=True,linked_library='Apple libicucore'),indent=2)+'\n',encoding='utf-8')


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--j2objc-home',type=Path,required=True);p.add_argument('--input',type=Path,required=True);p.add_argument('--native',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--icu-headers',type=Path,required=True);a=p.parse_args();build(a.j2objc_home.resolve(),a.input.resolve(),a.native.resolve(),a.out.resolve(),a.icu_headers.resolve())
