"""Generate, edit and validate source-derived stateful Java patch modules."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
from contracts import CONTRACTS
from regions import annotate, emit_java, hash_graph, verify, walk, semantic_form
from providers import generate as generate_providers

ROOT = Path(__file__).resolve().parent
REVANCED = ROOT.parent
UPSTREAM = REVANCED / 'revanced-patches-main'
YT = UPSTREAM / 'extensions/youtube/src/main/java/app/revanced/extension/youtube'
POLICY = YT / 'patches/playback/speed/RememberPlaybackSpeedPatch.java'
PACKAGE = 'app/revanced/extension/youtube/patches/playback/speed'
JAR = REVANCED.parent / 'jadx-gui-1.5.6-with-jre-win/lib/jadx-gui-1.5.6-all.jar'


def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def write(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8', newline='\n')
def source(path, value):
    path.parent.mkdir(parents=True, exist_ok=True);path.write_text(value, encoding='utf-8', newline='\n')
def run(command, log):
    with log.open('w', encoding='utf-8') as stream:
        result = subprocess.run(list(map(str, command)), stdout=stream, stderr=subprocess.STDOUT)
    if result.returncode: raise RuntimeError(log.read_text(encoding='utf-8', errors='replace')[-7000:])
def classes(src, dest, log):
    dest.mkdir();run([shutil.which('javac'), '-d', dest, *sorted(src.rglob('*.java'))], log)
def ledger(out):
    write(out / 'artifact-ledger.json', {str(p.relative_to(out)): sha(p) for p in out.rglob('*')
                                       if p.is_file() and p.name != 'artifact-ledger.json'})


def boundaries():
    settings = (YT / 'settings/Settings.java').read_text(encoding='utf-8')
    policy = POLICY.read_text(encoding='utf-8')
    selected = sorted(set(re.findall(r'Settings\.([A-Z][A-Z_0-9]*)', policy)))
    declarations=[];facts=[]
    for name in selected:
        match=re.search(r'public static final (BooleanSetting|FloatSetting) '+name+r' = new \1\("([^"]+)",\s*([^,;)]+)',settings)
        if not match: raise ValueError('Unresolved setting declaration: '+name)
        typ,key,default=match.groups();default=default.strip()
        if typ=='BooleanSetting':
            if default not in ('FALSE','TRUE','false','true'): raise ValueError('Computed setting default')
            value=default.lower(); parsed=value=='true'
        else:
            if not re.fullmatch(r'-?[0-9]+(?:\.[0-9]+)?f', default): raise ValueError('Computed float default')
            value=default;parsed=float(default[:-1])
        declarations.append(f'    public static final {typ} {name}=new {typ}("{key}",{value});')
        facts.append(dict(symbol='app.revanced.extension.youtube.settings.Settings#'+name,key=key,type=typ,default=parsed))
    custom=(YT / 'patches/playback/speed/CustomPlaybackSpeedPatch.java').read_text(encoding='utf-8')
    match=re.search(r'public static final float PLAYBACK_SPEED_MAXIMUM = ([0-9]+(?:\.[0-9]+)?);', custom)
    if not match: raise ValueError('Unresolved original maximum')
    maximum=match[1]+'f'
    settings_java='''package app.revanced.extension.youtube.settings;
import rv.translated.Simulator;
public final class Settings {
    public static final class BooleanSetting {
        public final String key; public boolean value;
        public BooleanSetting(String key,boolean initial){this.key=key;value=initial;}
        public boolean get(){Simulator.event("read|"+key);return value;}
    }
    public static final class FloatSetting {
        public final String key; public float value;
        public FloatSetting(String key,float initial){this.key=key;value=initial;}
        public float get(){Simulator.event("read|"+key);return value;}
        public void save(float number){Simulator.event("save|"+key+"|"+Float.floatToRawIntBits(number));value=number;}
    }
'''+ '\n'.join(declarations)+'\n}\n'
    return {
        'app/revanced/extension/youtube/settings/Settings.java':settings_java,
        PACKAGE+'/CustomPlaybackSpeedPatch.java':'package app.revanced.extension.youtube.patches.playback.speed;\npublic final class CustomPlaybackSpeedPatch {public static final float PLAYBACK_SPEED_MAXIMUM='+maximum+';}\n',
        'app/revanced/extension/youtube/patches/VideoInformation.java':'package app.revanced.extension.youtube.patches;\npublic final class VideoInformation {public interface PlaybackController {}}\n',
        'app/revanced/extension/shared/Logger.java':'''package app.revanced.extension.shared;
import rv.translated.Simulator;
public final class Logger {
    public interface LogMessage {String buildMessageString();}
    public static void printDebug(LogMessage message){if(Simulator.debug)Simulator.event("debug|"+message.buildMessageString());}
    public static void printException(LogMessage message,Exception ex){Simulator.trace.add("error|"+message.buildMessageString()+"|"+ex.getClass().getSimpleName());}
}
''',
        'app/revanced/extension/shared/Utils.java':'''package app.revanced.extension.shared;
import rv.translated.Simulator;
public final class Utils {
    public static void runOnMainThreadDelayed(Runnable runnable,long delay){
        Simulator.post(() -> {try {runnable.run();}catch(Exception ex){Simulator.trace.add("callback_error|"+ex.getClass().getSimpleName());}},delay);
    }
    public static void showToastShort(String text){Simulator.event("toast|"+text);}
}
''',
        'app/revanced/extension/shared/StringRef.java':'''package app.revanced.extension.shared;
import rv.translated.Simulator;
public final class StringRef {
    public static String str(String key,Object... args){Simulator.event("localize|"+key);return key+"|"+args[0];}
}
''',
        'rv/translated/PlatformClock.java':'package rv.translated;\npublic final class PlatformClock {public static long wallMillis(){return Simulator.wallMillis();}}\n',
    }, facts


def export(inputs, stubs, out, compiler, name):
    run([shutil.which('java'), '-cp', str(JAR)+os.pathsep+str(compiler), 'ExportPatch', out/name,
         stubs, '', *inputs], out/(name+'.log'))
    return json.loads((out/name).read_text(encoding='utf-8'))


def generate(out):
    if out.exists(): raise ValueError('Fresh generation output required')
    inputs=[POLICY,YT/'settings/Settings.java',YT/'patches/playback/speed/CustomPlaybackSpeedPatch.java',
            UPSTREAM/'extensions/shared/library/src/main/java/app/revanced/extension/shared/Utils.java',
            *[p for p in ROOT.glob('*.py')], ROOT/'ExportPatch.java', JAR,
            *sorted((ROOT/'adapters').glob('*')),
            UPSTREAM/'patches/src/main/resources/addresources/values/strings.xml',
            *sorted((ROOT/'fixtures').glob('*.java'))]
    hashes={str(p):sha(p) for p in inputs};out.mkdir(parents=True)
    seams,facts=boundaries();original=out/'original';original.mkdir()
    for path,text in seams.items():source(original/path,text)
    for name in ('Simulator','TraceHarness'):
        source(original/('rv/translated/'+name+'.java'),(ROOT/'fixtures'/f'{name}.java').read_text(encoding='utf-8'))
    original_text=POLICY.read_text(encoding='utf-8')
    needle='System.currentTimeMillis()'
    if original_text.count(needle)!=1:raise ValueError('Clock boundary seam changed')
    source(original/(PACKAGE+'/RememberPlaybackSpeedPatch.java'),original_text.replace(needle,'rv.translated.PlatformClock.wallMillis()'))
    source(out/'retained/RememberPlaybackSpeedPatch.java',original_text)
    shutil.copyfile(POLICY,out/'retained/RememberPlaybackSpeedPatch.java')
    classes(original,out/'original-classes',out/'original-javac.log')
    compiler=out/'exporter';compiler.mkdir()
    run([shutil.which('javac'),'-cp',JAR,'-d',compiler,ROOT/'ExportPatch.java'],out/'exporter-javac.log')
    # Attribute the unmodified policy. The clock seam applies only to the oracle.
    module=export([POLICY],out/'original-classes',out,compiler,'attributed.json')
    module['external_fields']={f['symbol']:f for f in facts}
    module['sources']={str(POLICY):hashes[str(POLICY)]}
    module=annotate(module,CONTRACTS);admission=verify(module,CONTRACTS)
    write(out/'module.json',module);write(out/'contracts.json',CONTRACTS);write(out/'settings.json',facts)
    generated=out/'generated';generated.mkdir()
    for path in original.rglob('*.java'):
        if path.name!='RememberPlaybackSpeedPatch.java':source(generated/path.relative_to(original),path.read_text(encoding='utf-8'))
    for name,text in emit_java(module,CONTRACTS).items():source(generated/name,text)
    classes(generated,out/'generated-classes',out/'generated-javac.log')
    reimport=export([generated/(PACKAGE+'/RememberPlaybackSpeedPatch.java')],out/'generated-classes',out,compiler,'reimported.json')
    # javac re-attribution validates generated types/overloads separately from annotations.
    require_symbols={m['symbol'] for c in module['classes'] for m in c['methods']}
    if require_symbols!={m['symbol'] for c in reimport['classes'] for m in c['methods']}:raise ValueError('Generated method identities changed')
    if semantic_form(module,CONTRACTS)!=semantic_form(reimport,CONTRACTS):
        write(out/'semantic-before.json',semantic_form(module,CONTRACTS));write(out/'semantic-after.json',semantic_form(reimport,CONTRACTS))
        raise ValueError('Re-attributed graph changes source semantics/types')
    for variant in ('original','generated'):
        run([shutil.which('java'),'-cp',out/(variant+'-classes'),'rv.translated.TraceHarness'],out/(variant+'-trace.txt'))
    a=(out/'original-trace.txt').read_text();b=(out/'generated-trace.txt').read_text()
    if a!=b:raise ValueError('Original versus generated state/effect traces differ')
    # A separate generic object/monitor/closure/exception probe uses the same frontend/backend.
    object_module=export([ROOT/'fixtures/Box.java',ROOT/'fixtures/ObjectProbe.java',ROOT/'fixtures/VolatileProbe.java'],out/'original-classes',out,compiler,'object-attributed.json')
    object_module=annotate(object_module,CONTRACTS);verify(object_module,CONTRACTS)
    write(out/'object-module.json',object_module)
    for name,text in emit_java(object_module,CONTRACTS).items():source(generated/name,text)
    # Compile new object modules without overwriting recorded classes.
    classes(generated,out/'complete-classes',out/'complete-javac.log')
    object_reimport=export([generated/'rv/fixture/Box.java',generated/'rv/fixture/ObjectProbe.java',generated/'rv/fixture/VolatileProbe.java'],out/'complete-classes',out,compiler,'object-reimported.json')
    if semantic_form(object_module,CONTRACTS)!=semantic_form(object_reimport,CONTRACTS):raise ValueError('Object reimport semantics differ')
    object_main='public final class ObjectHarness {public static void main(String[] args){if(rv.fixture.ObjectProbe.run()!=10)throw new AssertionError();for(int i=0;i<100;i++)if(rv.fixture.VolatileProbe.run()!=7)throw new AssertionError();System.out.println("Object, closure, monitor, exception and volatile probes passed");}}'
    for variant, files in [('original-object',[ROOT/'fixtures/Box.java',ROOT/'fixtures/ObjectProbe.java',ROOT/'fixtures/VolatileProbe.java']),('generated-object',[generated/'rv/fixture/Box.java',generated/'rv/fixture/ObjectProbe.java',generated/'rv/fixture/VolatileProbe.java'])]:
        src=out/variant;src.mkdir();source(src/'ObjectHarness.java',object_main)
        for f in files:source(src/'rv/fixture'/f.name,f.read_text(encoding='utf-8'))
        classes(src,out/(variant+'-classes'),out/(variant+'-javac.log'))
        run([shutil.which('java'),'-cp',out/(variant+'-classes'),'ObjectHarness'],out/(variant+'-trace.txt'))
    if {str(p):sha(p) for p in inputs}!=hashes:raise ValueError('Input drift during generation')
    generate_providers(out,module,CONTRACTS,facts,UPSTREAM/'patches/src/main/resources/addresources/values/strings.xml',source,write)
    classes(out/'production-java',out/'production-classes',out/'production-javac.log')
    production_reimport=export([out/'production-java'/(PACKAGE+'/RememberPlaybackSpeedPatch.java')],out/'production-classes',out,compiler,'production-reimported.json')
    if semantic_form(module,CONTRACTS)!=semantic_form(production_reimport,CONTRACTS):raise ValueError('Production policy semantics differ')
    if {str(p):sha(p) for p in inputs}!=hashes:raise ValueError('Input drift during production generation')
    callbacks=[n for c in module['classes'] for m in c['methods'] for n in walk(m['body']) if n['kind']=='LAMBDA_EXPRESSION']
    receipt=dict(schema='stateful-translator.generation.v1',input_sha256=hashes,module_sha256=sha(out/'module.json'),
        graph_sha256=hash_graph(module),admission=admission,callbacks=[dict(id=n['id'],captures=n['captures']) for n in callbacks],
        original_clock_seam=dict(from_call=needle,to_call='rv.translated.PlatformClock.wallMillis()',replacements=1),
        jvm_trace_cases=len(a.splitlines()),jvm_trace_sha256=sha(out/'original-trace.txt'),jvm_traces_equal=True,
        policy_generated_from_ir=True,reimported_semantics_equal=True,handwritten_ios_policy_used=False,dependency_closure='Selected class plus explicitly modeled operation/library boundaries',
        object_probe_generated=True,production_policy_reimport_equal=True,device_verified=False,native_compiled=False,
        dialect='Attributed typed structured regions; not full SSA or arbitrary Java',
        provider_scope='Deterministic settings, clock, scheduling, lazy logs, localization and toast seams; not Android SDK emulation')
    write(out/'generation-receipt.json',receipt);ledger(out)
    print(json.dumps({k:receipt[k] for k in ('admission','jvm_trace_cases','jvm_traces_equal','policy_generated_from_ir','object_probe_generated')},indent=2))


def verify_delivery(out):
    r=json.loads((out/'generation-receipt.json').read_text())
    for name,value in r['input_sha256'].items():
        if sha(name)!=value:raise ValueError('Input drift: '+name)
    for name,value in json.loads((out/'artifact-ledger.json').read_text()).items():
        if sha(out/name)!=value:raise ValueError('Artifact drift: '+name)
    module=json.loads((out/'module.json').read_text());verify(module,CONTRACTS)
    if hash_graph(module)!=r['graph_sha256']:raise ValueError('Graph drift')
    print('Source, typed graph, JVM trace and generation artifacts verified')


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('command',choices=['generate','verify','emit','edit'])
    parser.add_argument('--out',type=Path,default=ROOT/'build'/datetime.now(timezone.utc).strftime('%Y%m%dT%H%M%SZ'))
    parser.add_argument('--input',type=Path);parser.add_argument('--node');parser.add_argument('--value');parser.add_argument('--expected-hash')
    args=parser.parse_args()
    if args.command=='generate':generate(args.out.resolve())
    elif args.command=='verify':verify_delivery(args.out.resolve())
    else:
        module=json.loads(args.input.read_text())
        if args.command=='edit':
            if hash_graph(module)!=args.expected_hash:raise ValueError('Expected graph hash mismatch')
            matches=[n for n in walk(module) if n.get('id')==args.node and n['kind'].endswith('_LITERAL')]
            if len(matches)!=1:raise ValueError('Select exactly one literal node')
            matches[0]['value']=json.loads(args.value)
            # Attributed constant-use metadata must follow a literal field edit.
            for cls in module['classes']:
                for field in cls['fields']:
                    if field.get('value') is matches[0]:
                        for node in walk(module):
                            if node.get('symbol')==field['symbol'] and 'constant' in node:
                                node['constant']=matches[0]['value']
            module=annotate(module,CONTRACTS);verify(module,CONTRACTS)
            if args.out.exists():raise ValueError('Fresh edit output required')
            write(args.out,module)
        else:
            if args.out.exists():raise ValueError('Fresh emission output required')
            for name,text in emit_java(module,CONTRACTS).items():source(args.out/name,text)


if __name__=='__main__':main()
