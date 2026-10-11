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


def production_build(home, inputs, out, prefixes, sdk):
    """Build real operation providers separately from simulator and object probes."""
    objc=out/'production-objc';objc.mkdir()
    files=sorted((inputs/'production-java').rglob('*.java'))
    command=[home/'j2objc','-source','11','-use-arc','--prefixes',prefixes,'-classpath',home/'lib/j2objc_annotations.jar',
             '-sourcepath',inputs/'production-java','-d',objc,*files]
    run(command,out/'production-translation.log')
    probe=out/'provider-probe-objc';probe.mkdir()
    run([home/'j2objc','-source','11','-use-arc','--prefixes',prefixes,'-classpath',home/'lib/j2objc_annotations.jar',
         '-sourcepath',inputs/'production-java','-d',probe,inputs/'provider-probe-java/rv/platform/ProviderProbe.java'],out/'provider-probe-translation.log')
    runner=out/'ProviderHarness.m';runner.write_text('''#import <Foundation/Foundation.h>
#include <math.h>
#include <string.h>
#include "Operations.h"
#include "Events.h"
#include "rv/platform/NativeBridge.h"
#include "rv/platform/ProviderProbe.h"
static void check(BOOL ok) { if(!ok) abort(); }
static void drain(NSTimeInterval seconds) { [[NSRunLoop mainRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:seconds]]; }
int main(void) { @autoreleasepool {
    BOOL rejected=NO; @try { RVST_RememberPlaybackSpeedPatch_getPlaybackSpeedOverride(); } @catch(id ex) { rejected=YES; }
    check(rejected && !RVSTOperationsReady());
    NSString *suite=[@"org.revanced.translated.probe." stringByAppendingString:NSUUID.UUID.UUIDString];
    NSUserDefaults *prefs=[[NSUserDefaults alloc] initWithSuiteName:suite];
    NSMutableArray *toasts=[NSMutableArray array]; RVSTConfigureOperations(prefs, ^(NSString *message){[toasts addObject:message];});
    check(RVSTOperationsReady());
    check(RVSTPlatformNativeBridge_readBooleanWithNSString_withBoolean_(@"absent",YES));
    check(RVSTPlatformNativeBridge_readFloatWithNSString_withFloat_(@"absent",-2.0f)==-2.0f);
    uint32_t values[]={0x80000000,0x7fc01234,0x7f800000,0xff800000,0x3fc00000};
    for(int i=0;i<5;i++){float f;memcpy(&f,&values[i],4);RVSTPlatformNativeBridge_writeFloatWithNSString_withFloat_(@"raw",f);
        float g=RVSTPlatformNativeBridge_readFloatWithNSString_withFloat_(@"raw",0);uint32_t bits;memcpy(&bits,&g,4);check(bits==values[i]);}
    NSUserDefaults *fresh=[[NSUserDefaults alloc] initWithSuiteName:suite];check([fresh objectForKey:@"raw"]!=nil);
    [prefs setObject:@42 forKey:@"bad"];rejected=NO;
    @try { RVSTPlatformNativeBridge_readFloatWithNSString_withFloat_(@"bad",0); } @catch(id ex){rejected=YES;}check(rejected);
    RVSTPlatformProviderProbe_enqueueZero();check(!RVSTPlatformProviderProbe_called());drain(0.05);check(RVSTPlatformProviderProbe_called());
    RVSTPlatformProviderProbe_enqueueThrowing();drain(0.05);check(RVSTPlatformProviderProbe_throwingCalls()==1);
    check(llabs(RVSTPlatformNativeBridge_wallMillis()-(int64_t)(NSDate.date.timeIntervalSince1970*1000.0))<100);
    NSString *enabled=@"revanced_remember_playback_speed_last_selected", *toast=@"revanced_remember_playback_speed_last_selected_toast", *speed=@"revanced_playback_speed_default";
    [prefs setObject:@{@"type":@"bool",@"value":@YES} forKey:enabled];
    [prefs setObject:@{@"type":@"bool",@"value":@YES} forKey:toast];
    RVSTPlatformNativeBridge_writeFloatWithNSString_withFloat_(speed,2.0f);
    RVST_RememberPlaybackSpeedPatch_newVideoStarted();check(RVST_RememberPlaybackSpeedPatch_getPlaybackSpeedOverride()==2.0f);
    check(RVST_RememberPlaybackSpeedPatch_getPlaybackSpeedOverride()==-2.0f);
    RVST_RememberPlaybackSpeedPatch_userSelectedPlaybackSpeed(10.0f);
    check(RVSTPlatformNativeBridge_readFloatWithNSString_withFloat_(speed,0)==2.0f);drain(0.95);
    check(RVSTPlatformNativeBridge_readFloatWithNSString_withFloat_(speed,0)==8.0f && toasts.count==1);
    check([toasts[0] isEqual:@"Changed default speed to: 8.0x"]);
    [prefs removePersistentDomainForName:suite];puts("Production operation adapters and generated event bridge passed");return 0;
}}
''')
    native=inputs/'production-native'
    common=['-fobjc-arc','-fobjc-arc-exceptions','-fblocks','-O2','-I',home/'include','-I',objc,'-I',native,
            *sorted(objc.rglob('*.m')),*sorted(native.glob('*.m')),'-framework','Foundation','-framework','Security','-liconv','-lz','-licucore']
    host=['xcrun','clang',*common,'-I',probe,*sorted(probe.rglob('*.m')),runner,home/'lib/macosx/libjre_core.a','-o',out/'ProviderHarness']
    run(host,out/'production-host.log');run([out/'ProviderHarness'],out/'production-probe.txt')
    output=out/'TranslatedRememberSpeed.dylib'
    ios=['xcrun','--sdk','iphoneos','clang','-target','arm64-apple-ios17.0','-isysroot',sdk,*common,
         home/'lib/iphone/libjre_core.a','-framework','UIKit','-dynamiclib',
         '-Wl,-install_name,@executable_path/Frameworks/TranslatedRememberSpeed.dylib','-o',output]
    run(ios,out/'production-ios.log')
    symbols=subprocess.check_output(['xcrun','nm','-g',output],text=True);(out/'production-symbols.txt').write_text(symbols)
    if '_RVST_RememberPlaybackSpeedPatch_getPlaybackSpeedOverride' not in symbols:raise ValueError('Missing event bridge')
    if 'RVSTTestSimulator' in symbols or 'RVSTPlatformProviderProbe' in symbols:raise ValueError('Test providers leaked into production')
    return dict(operation_probes_passed=True,production_payload_sha256=sha(output),production_test_providers_present=False,
                production_files={str(p.relative_to(inputs)):sha(p) for p in [*files,*native.glob('*')]},
                production_objc={str(p.relative_to(objc)):sha(p) for p in objc.rglob('*') if p.is_file()},
                production_translation_command=list(map(str,command)),production_host_command=list(map(str,host)),production_ios_command=list(map(str,ios)))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--j2objc-home',type=Path,required=True)
    parser.add_argument('--out',type=Path,required=True)
    parser.add_argument('--input',type=Path,default=Path(__file__).resolve().parent/'delivery')
    args=parser.parse_args();home=args.j2objc_home.resolve();out=args.out.resolve();inputs=args.input.resolve()
    if out.exists():raise ValueError('Fresh native output required')
    out.mkdir(parents=True);objc=out/'objc';objc.mkdir()
    prefixes=out/'prefixes.properties'
    prefixes.write_text('app.revanced.extension.youtube.patches.playback.speed=RVSTPolicy\napp.revanced.extension.shared=RVSTShared\napp.revanced.extension.youtube.settings=RVSTSettings\napp.revanced.extension.youtube.patches=RVSTContext\nrv.translated=RVSTTest\nrv.fixture=RVSTFixture\nrv.platform=RVSTPlatform\n')
    files=sorted((inputs/'generated').rglob('*.java'))
    if not files:raise ValueError('No graph-generated source')
    command=[home/'j2objc','-source','11','-use-arc','--prefixes',prefixes,'-classpath',home/'lib/j2objc_annotations.jar',
             '-sourcepath',inputs/'generated','-d',objc,*files]
    run(command,out/'translation.log')
    runner=out/'Harness.m';runner.write_text('''#import <Foundation/Foundation.h>
#include "rv/translated/TraceHarness.h"
#include "rv/fixture/ObjectProbe.h"
#include "rv/fixture/VolatileProbe.h"
int main(void) { @autoreleasepool {
    if (RVSTFixtureObjectProbe_run()!=10) return 2;
    for(int i=0;i<100;i++) if (RVSTFixtureVolatileProbe_run()!=7) return 3;
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
                 home/'lib/iphone/libjre_core.a','-framework','UIKit','-dynamiclib','-Wl,-install_name,@executable_path/Frameworks/StatefulPolicy.dylib','-o',output]
    run(compile_ios,out/'compile-ios.log')
    symbols=subprocess.check_output(['xcrun','nm','-g',output],text=True)
    (out/'symbols.txt').write_text(symbols)
    if '_RVSTPolicyRememberPlaybackSpeedPatch_userSelectedPlaybackSpeedWithFloat_' not in symbols:raise ValueError('Missing translated stateful symbol')
    production=production_build(home,inputs,out,prefixes,sdk)
    receipt=dict(schema='stateful-translator.native.v1',source_files={str(p.relative_to(inputs)):sha(p) for p in files},
                 generated_objc={str(p.relative_to(objc)):sha(p) for p in objc.rglob('*') if p.is_file()},
                 generation_receipt_sha256=sha(inputs/'generation-receipt.json'),vendor_translator_sha256=sha(home/'lib/j2objc.jar'),
                 runtime_mac_sha256=sha(home/'lib/macosx/libjre_core.a'),runtime_ios_sha256=sha(home/'lib/iphone/libjre_core.a'),
                 translation_command=list(map(str,command)),host_command=list(map(str,compile_host)),ios_command=list(map(str,compile_ios)),
                 trace_cases=len(expected.splitlines()),jvm_native_traces_equal=True,object_probe_result=10,volatile_thread_trials=100,
                 payload_sha256=sha(output),device_verified=False,app_hooks_installed=False,
                 scope='Generated stateful patch through deterministic operation providers; real native macOS execution and ARM64 iOS compilation')
    receipt.update(production)
    (out/'receipt.json').write_text(json.dumps(receipt,indent=2)+'\n')
    (out/'artifact-ledger.json').write_text(json.dumps({str(p.relative_to(out)):sha(p) for p in out.rglob('*') if p.is_file()},indent=2)+'\n')
    print('Generated native state, callback and effect traces equal original JVM:',receipt['trace_cases'],'cases; object probe passed; ARM64 iOS compiled')


if __name__=='__main__':main()
