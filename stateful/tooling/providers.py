"""Generate operation seams and primitive event bridges, never feature algorithms."""
import json
import re
from pathlib import Path
import xml.etree.ElementTree as ET
from regions import emit_java, walk, require
PREFIXES = {'app.revanced.extension.youtube.patches.playback.speed':'RVSTPolicy',
            'app.revanced.extension.shared':'RVSTShared','app.revanced.extension.youtube.settings':'RVSTSettings',
            'app.revanced.extension.youtube.patches':'RVSTContext','rv.translated':'RVSTTest',
            'rv.fixture':'RVSTFixture','rv.platform':'RVSTPlatform'}


def objc_type(name):
    matches=[key for key in PREFIXES if name.startswith(key+'.')]
    require(matches, 'Backend namespace prefix required for '+name)
    package=max(matches,key=len)
    return PREFIXES[package]+name[len(package)+1:].replace('.','_')

NATIVE_BRIDGE = '''package rv.platform;
public final class NativeBridge {
    public static native boolean readBoolean(String key, boolean fallback);
    public static native float readFloat(String key, float fallback);
    public static native void writeFloat(String key, float value);
    public static native long wallMillis();
    public static native void postMain(Runnable callback, long milliseconds);
    public static native void showToast(String message);
    public static native boolean debugEnabled();
    public static native void log(String message);
}
'''


def production_sources(module, contracts, settings, resource_file):
    declarations = []
    for fact in settings:
        value = str(fact['default']).lower() if fact['type'] == 'BooleanSetting' else repr(fact['default']) + 'f'
        declarations.append('public static final ' + fact['type'] + ' ' + fact['symbol'].split('#')[1] +
                            '=new ' + fact['type'] + '(' + json.dumps(fact['key']) + ',' + value + ');')
    seams = {
        'rv/platform/NativeBridge.java': NATIVE_BRIDGE,
        'app/revanced/extension/youtube/settings/Settings.java': '''package app.revanced.extension.youtube.settings;
import rv.platform.NativeBridge;
public final class Settings {
    public static final class BooleanSetting {
        private final String key; private final boolean fallback;
        public BooleanSetting(String key,boolean fallback){this.key=key;this.fallback=fallback;}
        public boolean get(){return NativeBridge.readBoolean(key,fallback);}
    }
    public static final class FloatSetting {
        private final String key; private final float fallback;
        public FloatSetting(String key,float fallback){this.key=key;this.fallback=fallback;}
        public float get(){return NativeBridge.readFloat(key,fallback);}
        public void save(float value){NativeBridge.writeFloat(key,value);}
    }
''' + '\n'.join(declarations) + '\n}\n',
        'rv/translated/PlatformClock.java': 'package rv.translated; public final class PlatformClock {public static long wallMillis(){return rv.platform.NativeBridge.wallMillis();}}\n',
        'app/revanced/extension/shared/Logger.java': '''package app.revanced.extension.shared;
import rv.platform.NativeBridge;
public final class Logger {
    public interface LogMessage {String buildMessageString();}
    public static void printDebug(LogMessage message){if(NativeBridge.debugEnabled())NativeBridge.log(message.buildMessageString());}
    public static void printException(LogMessage message,Exception ex){NativeBridge.log(message.buildMessageString()+" | "+ex);}
}
''',
        'app/revanced/extension/shared/Utils.java': '''package app.revanced.extension.shared;
import rv.platform.NativeBridge;
public final class Utils {
    public static void runOnMainThreadDelayed(Runnable runnable,long delay){
        NativeBridge.postMain(() -> {try {runnable.run();}catch(Exception ex){Logger.printException(() -> "Main callback failure",ex);}},delay);
    }
    public static void showToastShort(String text){NativeBridge.showToast(text);}
}
''',
        'app/revanced/extension/youtube/patches/VideoInformation.java': 'package app.revanced.extension.youtube.patches; public final class VideoInformation {public interface PlaybackController {}}\n',
    }
    keys = sorted({n['arguments'][0]['value'] for n in walk(module)
                   if n['kind'] == 'METHOD_INVOCATION' and contracts.get(n['target'], {}).get('family') == 'localization'
                   and n['arguments'] and n['arguments'][0]['kind'] == 'STRING_LITERAL'})
    resources = {s.attrib.get('name'): ''.join(s.itertext()) for s in ET.parse(resource_file).iter('string')}
    cases = []
    for key in keys:
        require(key in resources, 'Missing source resource ' + key)
        require(resources[key].count('%s') == 1 and not re.search(r'%(?!s)', resources[key]), 'Unmodeled format resource ' + key)
        cases.append('if(key.equals(' + json.dumps(key) + '))return String.format(java.util.Locale.getDefault(),' +
                     json.dumps(resources[key]) + ',args);')
    seams['app/revanced/extension/shared/StringRef.java'] = '''package app.revanced.extension.shared;
public final class StringRef {public static String str(String key,Object... args){
''' + '\n'.join(cases) + '\nthrow new IllegalStateException("Unbound resource: "+key);}}\n'
    seams.update(emit_java(module, contracts))
    return seams, dict(locale='Source English fallback, JVM locale formatting; localized catalogs not yet imported',
                       resources={k: resources[k] for k in keys})


def event_bridge(module):
    """Primitive ABI derived from methods. Drop only provably unread object parameters."""
    types = {'boolean': ('bool', 'Boolean'), 'float': ('float', 'Float'), 'double': ('double', 'Double'),
             'int': ('int32_t', 'Int'), 'long': ('int64_t', 'Long'), 'void': ('void', '')}
    headers = ['#pragma once', '#include <stdbool.h>', '#include <stdint.h>']
    bodies = ['#include "Events.h"', '#include "Operations.h"']
    inventory = []
    names = set()
    for cls in module['classes']:
        objc = objc_type(cls['name'])
        bodies.append('#include "' + cls['name'].replace('.', '/') + '.h"')
        for method in cls['methods']:
            if 'public' not in method['modifiers'] or method['constructor']: continue
            require('static' in method['modifiers'], 'Instance event bridge requires receiver binding')
            require(method['result'] in types, 'Object result requires an identity adapter')
            params, args, suffix, dropped = [], [], '', []
            used = {n.get('symbol') for n in walk(method['body'])}
            for index, p in enumerate(method['parameters']):
                if p['declared_type'] in types and p['declared_type'] != 'void':
                    ct, jt = types[p['declared_type']]; params.append(ct + ' ' + p['name']); args.append(p['name'])
                else:
                    require(p['symbol'] not in used, 'Read object parameter requires a reviewed identity adapter')
                    jt = objc_type(p['declared_type']);args.append('nil');dropped.append(p['symbol'])
                suffix += ('With' if index == 0 else 'with') + jt + '_'
            cname = 'RVST_' + cls['simple_name'] + '_' + method['name']
            require(cname not in names, 'Overloaded event needs explicit ABI disambiguation'); names.add(cname)
            signature = types[method['result']][0] + ' ' + cname + '(' + (', '.join(params) or 'void') + ')'
            headers.append(signature + ';')
            call = objc + '_' + method['name'] + suffix + '(' + ', '.join(args) + ')'
            bodies.append(signature + ' { RVSTRequireEventContext(); ' + ('return ' if method['result'] != 'void' else '') + call + '; }')
            inventory.append(dict(source=method['symbol'], exported=cname, dropped_unread_parameters=dropped,
                                  affinity='main', role='Requires reviewed event placement; not inferred from method name'))
    return '\n'.join(headers) + '\n', '\n'.join(bodies) + '\n', inventory


def generate(out, module, contracts, settings, resource_file, source_writer, json_writer):
    sources, resources = production_sources(module, contracts, settings, resource_file)
    for path, text in sources.items(): source_writer(out/'production-java'/path, text)
    h, m, events = event_bridge(module)
    source_writer(out/'production-native/Events.h', h);source_writer(out/'production-native/Events.m', m)
    source_writer(out/'production-native/prefixes.properties',''.join(k+'='+v+'\n' for k,v in PREFIXES.items()))
    here = Path(__file__).resolve().parent
    for path in (here/'adapters').glob('*'):
        if path.suffix not in ('.h', '.m'): continue
        source_writer(out/'production-native'/path.name, path.read_text(encoding='utf-8'))
    source_writer(out/'provider-probe-java/rv/platform/ProviderProbe.java', (here/'adapters/ProviderProbe.java').read_text(encoding='utf-8'))
    json_writer(out/'production-contracts.json', dict(events=events, resources=resources,
                preferences_suite='org.revanced.translated.stateful-v1',
                float_encoding='Typed f32 raw IEEE bits in NSUserDefaults',
                scheduling='dispatch_after, always queued, strong capture until execution, Java exception wrapper',
                runtime_ready_requires=['Configure preference domain and toast presenter', 'Main-thread event placement'],
                automatic_hook_installation=False, device_verified=False))
