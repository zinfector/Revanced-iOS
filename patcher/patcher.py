"""YouTube iOS native-payload patcher. Python standard library only."""
import argparse
import hashlib
import json
import math
import re
import os
from pathlib import Path, PurePosixPath
import plistlib
import stat
import sys
import tempfile
import zipfile
from urllib.parse import urlsplit
from macho import MachO, PatchError, LC_CODE_SIGNATURE, LC_ID_DYLIB
from features import FEATURES

ROOT = Path(__file__).resolve().parent
PAYLOAD_NAME = 'RVPort.dylib'
CONFIG_NAME = 'RVPort.json'
STAMP_NAME = 'RVPortPatchManifest.json'
LOAD_PATH = '@executable_path/Frameworks/' + PAYLOAD_NAME
DEFAULTS = {'schema': 1, **{f: False for f in FEATURES}, 'video_ads': True,
            'background_playback': True, 'ad_strategy': 'response', 'default_speed': 1.0,
            'default_quality': 0, 'sponsor_categories': ['sponsor', 'selfpromo', 'interaction'],
            'feed_patterns': ['text_search_ad', 'ads_video_with_context', 'carousel_ad'],
            'shorts_patterns': ['shorts_shelf', 'shorts_video_cell'], 'diagnostics': True,
            'action_patterns': ['video_action_bar_download_button', 'video_action_bar_clip_button'],
            'flyout_patterns': ['quality_sheet_premium_upsell'], 'comment_patterns': ['comments_entry_point', 'comment_thread'],
            'layout_patterns': [], 'custom_speeds': [0.25,0.5,0.75,1,1.25,1.5,1.75,2,2.5,3,4],
            'double_tap_seconds': 0, 'overlay_opacity': 1.0, 'seekbar_color': '', 'theme': 'system',
            'start_page': '', 'thumbnail_frame': 2, 'app_name': '', 'client_version': '21.39.4',
            'screen_width_points':1920,'screen_height_points':1080,'form_factor':'tablet',
            'thumbnail_proxy_url':'', 'dearrow_url':'https://dearrow-thumb.ajay.app/api/v1/getThumbnail',
            'sponsor_behaviors':{}, 'sponsor_colors':{}, 'sponsor_min_duration':0.0,
            'thumbnail_modes':{}, 'theme_light_background':'', 'theme_dark_background':''}
DEFAULTS.update(wifi_quality=-1,cellular_quality=-1,miniplayer_min_dimension_points=0,miniplayer_overlay_opacity=1.0)

def sha(data): return hashlib.sha256(data).hexdigest()
def file_sha(path):
    with Path(path).open('rb') as stream:
        return hashlib.file_digest(stream,'sha256').hexdigest()

def profile(): return json.loads((ROOT / 'profiles/youtube-21.39.4.json').read_text(encoding='utf-8'))

def members(z):
    seen = set()
    total = 0
    for info in z.infolist():
        name = info.filename
        p = PurePosixPath(name)
        if not name or name.startswith('/') or '\\' in name or '..' in p.parts or ':' in name or '\0' in name:
            raise PatchError(f'Unsafe archive member: {name!r}')
        if name in seen:
            raise PatchError(f'Duplicate archive member: {name}')
        seen.add(name)
        if stat.S_ISLNK(info.external_attr >> 16):
            raise PatchError(f'Symlink archive member is unsupported: {name}')
        total += info.file_size
        if info.file_size > 2*1024**3 or total > 8*1024**3:
            raise PatchError('IPA exceeds supported uncompressed size')
    return seen

def read_app(z):
    names = members(z)
    plists = [n for n in names if len(PurePosixPath(n).parts)==3 and n.startswith('Payload/') and n.endswith('.app/Info.plist')]
    if len(plists)!=1:
        raise PatchError('Expected exactly one top-level Payload/*.app/Info.plist')
    app = plists[0].rsplit('/', 1)[0]
    try:
        info = plistlib.loads(z.read(plists[0]))
        exe = info['CFBundleExecutable']
    except (KeyError, ValueError, plistlib.InvalidFileException) as ex:
        raise PatchError('Invalid app Info.plist') from ex
    if not isinstance(exe, str) or '/' in exe or '\\' in exe or exe in ('.', '..'):
        raise PatchError('Unsafe executable name')
    binary = z.read(app+'/'+exe)
    return app, info, binary, MachO(binary)

def inspect(ipa):
    with zipfile.ZipFile(ipa) as z:
        app, info, binary, image = read_app(z)
        p = profile()
        compatible = (info.get('CFBundleIdentifier') == p['bundle'] and info.get('CFBundleShortVersionString') == p['version']
            and image.uuid == p['uuid'] and sha(binary) == p['executable_sha256'] and not image.encrypted)
        return {'app': app, 'bundle': info.get('CFBundleIdentifier'), 'version': info.get('CFBundleShortVersionString'),
                'uuid': image.uuid, 'executable_sha256': sha(binary), 'encrypted': image.encrypted,
                'supported_original': compatible, 'already_patched': app+'/'+STAMP_NAME in z.namelist(),
                'header_padding': image.first_content-image.end, 'dylibs': [n for _, n in image.dylibs()]}

def validate_config(config):
    if not isinstance(config, dict): raise PatchError('Configuration must be a JSON object')
    unknown = set(config)-set(DEFAULTS)
    if unknown: raise PatchError('Unknown configuration keys: '+', '.join(sorted(unknown)))
    c = dict(DEFAULTS, **config)
    if type(c['schema']) is not int or c['schema'] != 1: raise PatchError('Unsupported configuration schema')
    for f in (*FEATURES, 'diagnostics'):
        if type(c[f]) is not bool: raise PatchError(f'{f} must be true or false')
    if c['ad_strategy'] not in ('response', 'trigger', 'coordinator'): raise PatchError('Invalid ad strategy')
    if type(c['default_speed']) not in (int, float) or not math.isfinite(c['default_speed']) or not 0.25<=c['default_speed']<=4:
        raise PatchError('default_speed must be between 0.25 and 4')
    if type(c['default_quality']) is not int or c['default_quality'] not in (0,144,240,360,480,720,1080,1440,2160):
        raise PatchError('default_quality must be 0 (auto) or a supported resolution')
    for key in ('wifi_quality','cellular_quality'):
        if type(c[key]) is not int or c[key] not in (-1,0,144,240,360,480,720,1080,1440,2160):
            raise PatchError(key+' must be -1 (inherit), 0 (auto) or a supported resolution')
    allowed_categories = {'sponsor','selfpromo','interaction','intro','outro','preview','music_offtopic','filler','hook','poi_highlight'}
    for key in ('sponsor_categories', 'feed_patterns', 'shorts_patterns', 'action_patterns', 'flyout_patterns', 'comment_patterns', 'layout_patterns'):
        if not isinstance(c[key], list) or len(c[key])>64 or any(not isinstance(x,str) or not x or len(x)>128 for x in c[key]):
            raise PatchError(f'{key} must be a list of nonempty strings (up to 64)')
    if set(c['sponsor_categories'])-allowed_categories: raise PatchError('Unsupported SponsorBlock categories')
    behaviors=c['sponsor_behaviors']
    if not isinstance(behaviors,dict) or set(behaviors)-allowed_categories or any(v not in ('skip','skip-once','manual-skip','seekbar-only','ignore') for v in behaviors.values()):
        raise PatchError('sponsor_behaviors must map supported categories to skip, skip-once, manual-skip, seekbar-only or ignore')
    colors=c['sponsor_colors']
    if not isinstance(colors,dict) or set(colors)-allowed_categories or any(not isinstance(v,str) or not re.fullmatch(r'#[0-9A-Fa-f]{6}',v) for v in colors.values()):
        raise PatchError('sponsor_colors must map supported categories to #RRGGBB colors')
    if type(c['sponsor_min_duration']) not in (int,float) or not math.isfinite(c['sponsor_min_duration']) or not 0<=c['sponsor_min_duration']<=600:
        raise PatchError('sponsor_min_duration must be 0-600 seconds')
    if not isinstance(c['custom_speeds'],list) or not 1<=len(c['custom_speeds'])<=30 or any(type(x) not in (int,float) or not math.isfinite(x) or not .25<=x<=4 for x in c['custom_speeds']):
        raise PatchError('custom_speeds must contain 1-30 finite rates between 0.25 and 4')
    if type(c['double_tap_seconds']) not in (int,float) or not math.isfinite(c['double_tap_seconds']) or not 0<=c['double_tap_seconds']<=120:
        raise PatchError('double_tap_seconds must be 0 (unchanged) through 120')
    if type(c['overlay_opacity']) not in (int,float) or not math.isfinite(c['overlay_opacity']) or not 0<=c['overlay_opacity']<=1:
        raise PatchError('overlay_opacity must be between 0 and 1')
    if type(c['miniplayer_min_dimension_points']) not in (int,float) or not math.isfinite(c['miniplayer_min_dimension_points']) or not (c['miniplayer_min_dimension_points']==0 or 170<=c['miniplayer_min_dimension_points']<=480):
        raise PatchError('miniplayer_min_dimension_points must be 0 (native) or 170-480 points')
    if type(c['miniplayer_overlay_opacity']) not in (int,float) or not math.isfinite(c['miniplayer_overlay_opacity']) or not 0<=c['miniplayer_overlay_opacity']<=1:
        raise PatchError('miniplayer_overlay_opacity must be between 0 and 1')
    if not isinstance(c['seekbar_color'],str) or (c['seekbar_color'] and not re.fullmatch(r'#[0-9A-Fa-f]{6}',c['seekbar_color'])):
        raise PatchError('seekbar_color must be empty or #RRGGBB')
    for key in ('theme_light_background','theme_dark_background'):
        if not isinstance(c[key],str) or (c[key] and not re.fullmatch(r'#[0-9A-Fa-f]{6}',c[key])):raise PatchError(key+' must be empty or #RRGGBB')
    modes=c['thumbnail_modes']
    if not isinstance(modes,dict) or set(modes)-{'home','subscriptions','library','player','search'} or any(v not in ('original','stills','dearrow','dearrow-stills') for v in modes.values()):
        raise PatchError('thumbnail_modes must map home/subscriptions/library/player/search to original, stills, dearrow or dearrow-stills')
    if c['theme'] not in ('system','dark','light'): raise PatchError('Invalid theme')
    if c['start_page'] not in ('','FEwhat_to_watch','FEsubscriptions','FElibrary'): raise PatchError('Unsupported start_page')
    if type(c['thumbnail_frame']) is not int or c['thumbnail_frame'] not in (1,2,3): raise PatchError('thumbnail_frame must be 1, 2 or 3')
    if not isinstance(c['app_name'],str) or len(c['app_name'])>60 or any(ord(x)<32 for x in c['app_name']): raise PatchError('app_name must be up to 60 printable characters')
    if not isinstance(c['client_version'],str) or not re.fullmatch(r'\d{1,3}\.\d{1,3}\.\d{1,3}',c['client_version']): raise PatchError('client_version must be a three-part numeric version')
    for key in ('screen_width_points','screen_height_points'):
        if type(c[key]) is not int or not 1<=c[key]<=8192: raise PatchError(f'{key} must be between 1 and 8192')
    if c['form_factor'] not in ('phone','tablet'):raise PatchError('form_factor must be phone or tablet')
    proxy=c['thumbnail_proxy_url']
    if not isinstance(proxy,str) or len(proxy)>1024:raise PatchError('Invalid thumbnail_proxy_url')
    if proxy:
        parts=urlsplit(proxy)
        if parts.scheme!='https' or not parts.hostname or parts.username or parts.password or parts.fragment:
            raise PatchError('thumbnail_proxy_url must be an HTTPS endpoint without credentials or fragment')
    endpoint=c['dearrow_url']
    if not isinstance(endpoint,str) or len(endpoint)>1024:raise PatchError('Invalid dearrow_url')
    parts=urlsplit(endpoint)
    if parts.scheme!='https' or not parts.hostname or parts.username or parts.password or parts.fragment:
        raise PatchError('dearrow_url must be an HTTPS endpoint without credentials or fragment')
    return c

def payload(path):
    data = path.read_bytes()
    image = MachO(data)
    if image.filetype != 6 or image.encrypted or image.platform != 2:
        raise PatchError('Payload must be an unencrypted thin ARM64 iOS MH_DYLIB')
    if image.minimum_os is None or image.minimum_os > (17<<16):
        raise PatchError('Payload requires an unsupported minimum iOS version')
    if (LC_ID_DYLIB, LOAD_PATH) not in image.dylibs(): raise PatchError('Unexpected payload install name')
    for kind, name in image.dylibs():
        if kind != LC_ID_DYLIB and not name.startswith(('/System/Library/Frameworks/', '/usr/lib/')):
            raise PatchError(f'Non-system payload dependency is not packaged: {name}')
    return data

def patch(ipa, output, dylib, config, strip_extensions=False, branding=None):
    from macho import inject_library
    ipa, output, dylib = Path(ipa).resolve(), Path(output).resolve(), Path(dylib).resolve()
    if output == ipa: raise PatchError('Output must not replace the input IPA')
    if output.exists(): raise PatchError(f'Output already exists: {output}')
    c = validate_config(config)
    library = payload(dylib)
    p = profile()
    with zipfile.ZipFile(ipa) as z:
        app, info, binary, image = read_app(z)
        if app+'/'+STAMP_NAME in z.namelist(): raise PatchError('Already patched; use the original IPA')
        if image.encrypted: raise PatchError('Main executable is encrypted')
        if info.get('CFBundleIdentifier')!=p['bundle'] or info.get('CFBundleShortVersionString')!=p['version'] or image.uuid!=p['uuid'] or sha(binary)!=p['executable_sha256']:
            raise PatchError('Unsupported app version or executable hash; no output produced')
        binary = inject_library(binary, LOAD_PATH)
        marker = {'patcher_version': '0.3.10', 'profile': p['id'], 'bundle': p['bundle'], 'version': p['version'],
                  'uuid': p['uuid'], 'input_ipa_sha256': file_sha(ipa),
                  'original_executable_sha256': p['executable_sha256'], 'patched_executable_sha256': sha(binary),
                  'payload_sha256': sha(library), 'config': c, 'signing_required': True,
                  'device_validated': False, 'extensions_removed': strip_extensions}
        replaced = {app+'/'+info['CFBundleExecutable']: binary,
                    app+'/Frameworks/'+PAYLOAD_NAME: library,
                    app+'/'+CONFIG_NAME: json.dumps(c,indent=2).encode(),
                    app+'/'+STAMP_NAME: json.dumps(marker,indent=2).encode()}
        if c['app_name']:
            info['CFBundleDisplayName']=c['app_name']
            replaced[app+'/Info.plist']=plistlib.dumps(info,fmt=plistlib.FMT_BINARY)
        if branding:
            from branding import prepare
            resources=prepare(info,branding)
            replaced.update({app+'/'+name:data for name,data in resources.items()})
            if 'RVHeader.png' in resources:
                c['custom_header']=True
                replaced[app+'/'+CONFIG_NAME]=json.dumps(c,indent=2).encode()
            replaced[app+'/Info.plist']=plistlib.dumps(info,fmt=plistlib.FMT_BINARY)
        marker['resources']={name[len(app)+1:]:sha(data) for name,data in replaced.items() if name.endswith('.png') or name==app+'/Info.plist'}
        replaced[app+'/'+STAMP_NAME]=json.dumps(marker,indent=2).encode()
        output.parent.mkdir(parents=True,exist_ok=True)
        fd, temp = tempfile.mkstemp(prefix='.rvport-',suffix='.ipa',dir=output.parent)
        os.close(fd)
        try:
            with zipfile.ZipFile(temp,'w',compression=zipfile.ZIP_DEFLATED,compresslevel=6) as out:
                for entry in z.infolist():
                    name = entry.filename
                    if name in replaced: continue
                    parts = PurePosixPath(name).parts
                    if name.startswith(app+'/') and ('_CodeSignature' in parts or parts[-1]=='embedded.mobileprovision'):
                        continue
                    if strip_extensions and name.startswith(app+'/') and any(part.endswith('.appex') for part in parts):
                        continue
                    with z.open(entry) as source, out.open(entry,'w') as target:
                        while chunk := source.read(1024*1024): target.write(chunk)
                for name, data in replaced.items():
                    entry = zipfile.ZipInfo(name,date_time=(2026,10,3,0,0,0))
                    executable = name.endswith('/'+PAYLOAD_NAME) or name == app+'/'+info['CFBundleExecutable']
                    entry.external_attr = ((stat.S_IFREG | (0o755 if executable else 0o644))<<16)
                    entry.compress_type = zipfile.ZIP_DEFLATED
                    out.writestr(entry,data)
            verify(temp)
            # Atomic publication on the same filesystem, with no overwrite race.
            os.link(temp,output)
        finally:
            Path(temp).unlink(missing_ok=True)
    return marker

def verify(ipa):
    with zipfile.ZipFile(ipa) as z:
        app, info, binary, image = read_app(z)
        try:
            marker = json.loads(z.read(app+'/'+STAMP_NAME))
            c = json.loads(z.read(app+'/'+CONFIG_NAME))
            library = z.read(app+'/Frameworks/'+PAYLOAD_NAME)
        except (KeyError, ValueError) as ex: raise PatchError('Missing or invalid patch artifacts') from ex
        p = profile()
        if marker.get('profile') != p['id'] or info.get('CFBundleIdentifier') != p['bundle'] or info.get('CFBundleShortVersionString') != p['version'] or image.uuid != p['uuid']:
            raise PatchError('Profile/identity mismatch')
        if sha(binary)!=marker.get('patched_executable_sha256') or sha(library)!=marker.get('payload_sha256'):
            raise PatchError('Patched executable/payload integrity mismatch')
        c=validate_config(c)
        if c!=validate_config(marker.get('config')): raise PatchError('Config does not match patch manifest')
        for name, expected in marker.get('resources',{}).items():
            if name not in ('Info.plist','RVHeader.png','RVAppIcon@2x.png','RVAppIcon@3x.png','RVPadIcon@2x~ipad.png'):raise PatchError('Unknown branding resource')
            if sha(z.read(app+'/'+name))!=expected:raise PatchError('Branding resource integrity mismatch: '+name)
        if sum(name==LOAD_PATH for _, name in image.dylibs())!=1: raise PatchError('Missing or duplicate payload load command')
        if image.encrypted or any(cmd.kind==LC_CODE_SIGNATURE for cmd in image.commands):
            raise PatchError('Unexpected encrypted or signed main executable in unsigned patch artifact')
        payload_image = MachO(library)
        if payload_image.filetype != 6 or payload_image.platform != 2 or (LC_ID_DYLIB, LOAD_PATH) not in payload_image.dylibs():
            raise PatchError('Invalid payload image')
        return {'status':'verified', 'signing_required':True, 'device_validated':False,
                'profile':p['id'], 'features':{k:c[k] for k in FEATURES}, 'default_speed':c['default_speed'], 'default_quality':c['default_quality']}

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest='command',required=True)
    for name in ('inspect','verify'):
        sub=commands.add_parser(name);sub.add_argument('ipa',type=Path)
    sub=commands.add_parser('patch')
    sub.add_argument('ipa',type=Path);sub.add_argument('-o','--output',type=Path,required=True)
    sub.add_argument('--payload',type=Path,default=ROOT/'build/RVPort.dylib')
    sub.add_argument('--config',type=Path)
    sub.add_argument('--features',help='Comma-separated enabled features; replaces defaults')
    sub.add_argument('--speed',type=float)
    sub.add_argument('--quality',type=int)
    sub.add_argument('--ad-strategy',choices=['response','trigger','coordinator'])
    sub.add_argument('--all-features',action='store_true',help='Enable every feature candidate; experimental, device testing required')
    sub.add_argument('--app-name')
    sub.add_argument('--header-image',type=Path)
    sub.add_argument('--icon-120',type=Path);sub.add_argument('--icon-180',type=Path);sub.add_argument('--icon-152',type=Path)
    sub.add_argument('--strip-extensions',action='store_true')
    args=parser.parse_args()
    try:
        if args.command=='inspect':result=inspect(args.ipa)
        elif args.command=='verify':result=verify(args.ipa)
        else:
            c=json.loads(args.config.read_text()) if args.config else {}
            if args.all_features:c.update({f:True for f in FEATURES})
            if args.features is not None:
                enabled=set(filter(None,args.features.split(',')))
                if enabled-set(FEATURES):raise PatchError('Unknown features: '+', '.join(sorted(enabled-set(FEATURES))))
                c.update({f:f in enabled for f in FEATURES})
            if args.speed is not None:c['default_speed']=args.speed
            if args.quality is not None:c['default_quality']=args.quality
            if args.ad_strategy:c['ad_strategy']=args.ad_strategy
            if args.app_name is not None:c['app_name']=args.app_name
            branding={k:getattr(args,k) for k in ('header_image','icon_120','icon_180','icon_152') if getattr(args,k)}
            result=patch(args.ipa,args.output,args.payload,c,args.strip_extensions,branding)
        print(json.dumps(result,indent=2))
        return 0
    except (PatchError,OSError,zipfile.BadZipFile,KeyError,ValueError) as ex:
        print(f'Error: {ex}',file=sys.stderr);return 2

if __name__=='__main__':raise SystemExit(main())
