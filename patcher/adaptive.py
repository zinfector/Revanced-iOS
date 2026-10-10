"""Discover a target-bound profile. Addresses are evidence, never patch inputs.

Static ABI matches permit runtime preflight, not proof of unchanged behavior.
Private wire/template adapters remain bound to their audited original binary.
"""
from collections import Counter
import hashlib
import json
from pathlib import Path
import re
import zipfile

from adaptive_abi import abi
from adaptive_image import Image
from adaptive_descriptor import table
from macho import PatchError
from recipe_catalog import ROOT, canonical, digest, extract

SCHEMA = 1
PROFILE_NAME = 'RVPortProfile.json'
BASELINE_SHA = 'ca9e2f62bdd7fe612f9e7d9f14c395a3c50c9495b572328bd4abd981ed4ec5cf'
ACCEPTED = {'static_match', 'runtime_inherited', 'runtime_framework', 'runtime_descriptor'}
WIRE_FEATURES = {'paired_vote_buttons', 'first_launch_ui', 'dearrow_titles', 'dearrow_thumbnails'}
GPB_CLASSES = {'YTIPlayerResponse', 'YTIElementRenderer', 'YTIIosPlayerConfig',
               'YTIPlayerConfig', 'YTIGranularVariableSpeedConfig',
               'YTIGranularVariableSpeedConfig_PlaybackRateOption'}


def catalog():
    data = json.loads((ROOT / 'recipes.json').read_text(encoding='utf-8'))
    if data != extract() or data.get('unresolved'):
        raise PatchError('Recipe catalog is stale/unresolved; run recipe_catalog.py and rebuild')
    return data


def runtime_field_abi(field, selector):
    # GPBDataType from GPBRuntimeTypes.h. Map/repeated storage is an object.
    typ = {0:'B', 1:'I', 2:'i', 3:'f', 4:'Q', 5:'q', 6:'d', 7:'i',
           8:'q', 9:'i', 10:'q', 11:'I', 12:'Q', 13:'@', 14:'@',
           15:'@', 16:'@', 17:'i'}.get(field['data_type'])
    if field['flags'] & (2 | 0xf00): typ = '@'
    name = field['name']
    if selector == name: return typ + '@:' if typ else None
    if selector == 'set' + name[0].upper() + name[1:] + ':': return 'v@:' + typ if typ else None
    return None


def resolve(classes, contract, descriptors):
    name, seen = contract['class_name'], set()
    while name in classes and name not in seen:
        seen.add(name); cls = classes[name]
        methods = [m for m in cls['methods'] if (m['kind'], m['selector']) == (contract['kind'], contract['selector'])]
        if methods:
            if len(methods) != 1: return dict(status='ambiguous_metadata', candidates=methods)
            method = methods[0]
            return dict(status='static_match' if abi(method['encoding']) == contract['abi'] else 'abi_mismatch',
                        owner=name, observed_abi=abi(method['encoding']), **method)
        name = cls['superclass']
    descriptor = descriptors.get(contract['class_name'], {})
    if contract['kind'] == '-' and descriptor.get('status') == 'discovered':
        fields = [f for f in descriptor['fields'] if runtime_field_abi(f, contract['selector']) == contract['abi']]
        if len(fields) == 1: return dict(status='runtime_descriptor', field=fields[0], reason='Dynamic GPB accessor; runtime ABI required')
    name, seen = contract['class_name'], set()
    while name in classes and name not in seen:
        seen.add(name); cls = classes[name]
        if cls['external_superclass']:
            return dict(status='runtime_inherited', external_superclass=cls['external_superclass'],
                        reason='External ancestor; runtime class/method/ABI preflight required')
        name = cls['superclass']
    if contract['class_name'].startswith(('NS', 'UI', 'AV', 'CA', 'LA', 'WK', 'SK', 'SF', 'ASAuthorization')):
        return dict(status='runtime_framework', reason='System framework class; runtime ABI preflight required')
    return dict(status='missing', reason='No unique method or supported dynamic accessor')


def discover_binary(info, binary):
    if info.get('CFBundleIdentifier') != 'com.google.ios.youtube' or info.get('CFBundleExecutable') != 'YouTube':
        raise PatchError('Expected the original YouTube app/executable')
    version = info.get('CFBundleShortVersionString')
    if not isinstance(version, str) or not re.fullmatch(r'\d+\.\d+\.\d+', version):
        raise PatchError('Invalid version')
    cat = catalog(); image = Image(binary)
    classes, errors, category_count = image.classes()
    if errors: raise PatchError('Incomplete ObjC metadata inventory: ' + json.dumps(errors[:3]))
    descriptors = {}
    for name in sorted(GPB_CLASSES):
        methods = [m for m in classes.get(name, {}).get('methods', []) if m['kind'] == '+' and m['selector'] == 'descriptor']
        try:
            if len(methods) != 1: raise ValueError('No unique +descriptor')
            descriptors[name] = dict(status='discovered', **table(image, methods[0]))
        except (ValueError, IndexError) as ex:
            descriptors[name] = dict(status='unresolved', reason=str(ex))
    exe_sha = hashlib.sha256(binary).hexdigest()
    records = []
    for c in cat['contracts']:
        evidence = resolve(classes, c, descriptors)
        private = any(s['file'].startswith(('RVDeArrow', 'RVVoteModel', 'RVSettingsNativeUI', 'RVSettingsBridge')) for s in c['sources'])
        if private and exe_sha != BASELINE_SHA:
            evidence = dict(status='semantic_adapter_required', reason='Private wire/template/settings adapter is audited only for the baseline executable', metadata=evidence)
        records.append(dict(**c, resolution=evidence))
    names = {c['class_name'] for c in cat['contracts']}
    ivars = {n: classes[n]['ivars'] for n in sorted(names & classes.keys()) if classes[n]['ivars']}
    counts = dict(Counter(r['resolution']['status'] for r in records))
    return dict(schema=SCHEMA, adapter_catalog=cat['adapter_catalog'], catalog_sha256=digest(cat),
                id='youtube-' + version + '-' + exe_sha[:16], bundle=info['CFBundleIdentifier'],
                executable=info['CFBundleExecutable'], version=version, uuid=image.uuid,
                executable_sha256=exe_sha, image_base=hex(image.base), pointer_formats=sorted(image.formats),
                contracts=records, descriptors=descriptors, ivar_evidence=ivars,
                required_capabilities=cat['required_capabilities'],
                disabled_features=sorted(WIRE_FEATURES if exe_sha != BASELINE_SHA else []),
                summary=dict(class_count=len(classes), categories=category_count, contracts=len(records), resolutions=counts),
                runtime_preflight_required=True, device_validated=False,
                policy='Optional hooks fail closed independently; typed callbacks never change ABI automatically; private semantic adapters require review')


def discover(ipa):
    from patcher import read_app, STAMP_NAME
    with zipfile.ZipFile(ipa) as z:
        app, info, binary, image = read_app(z)
        if app + '/' + STAMP_NAME in z.namelist(): raise PatchError('Discover using an original IPA')
        return discover_binary(info, binary)


def validate(p, info, image, binary_sha=None):
    cat = catalog()
    if not isinstance(p, dict) or p.get('schema') != SCHEMA or p.get('catalog_sha256') != digest(cat):
        raise PatchError('Adaptive profile schema/catalog mismatch')
    if any(p.get(k) != info.get(v) for k, v in [('bundle','CFBundleIdentifier'), ('version','CFBundleShortVersionString'), ('executable','CFBundleExecutable')]) or p.get('uuid') != image.uuid:
        raise PatchError('Adaptive profile target identity mismatch')
    if binary_sha is not None and p.get('executable_sha256') != binary_sha:
        raise PatchError('Adaptive profile executable hash mismatch')
    if [ {k:r.get(k) for k in c} for r,c in zip(p.get('contracts', []), cat['contracts']) ] != cat['contracts'] or len(p.get('contracts', [])) != len(cat['contracts']):
        raise PatchError('Adaptive profile contract mismatch')
    return p


def payload_tag(p): return ('RVPORT_ADAPTIVE_1:' + p['catalog_sha256']).encode()


def explanation(p):
    return dict(profile=p['id'], **p['summary'], disabled_features=p['disabled_features'],
                runtime_preflight_required=True, device_validated=False,
                blocked=[dict(id=c['id'], sources=c['sources'], **c['resolution']) for c in p['contracts'] if c['resolution']['status'] not in ACCEPTED])
