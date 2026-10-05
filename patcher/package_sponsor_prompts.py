"""Package the Apple Xcode 0.3.26 payload and record evidence, without tests."""
import hashlib
import json
from datetime import datetime, timezone
from pathlib import Path

import patcher

ROOT = Path(__file__).resolve().parent
WORKSPACE = ROOT.parents[4]
VERSION = '0.3.26'


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    native = json.loads((ROOT/'build/build-manifest.json').read_text(encoding='utf-8'))
    cloud = json.loads((ROOT/'build/cloud-build.json').read_text(encoding='utf-8'))
    if native['compiler_command'][:4] != ['xcrun', '--sdk', 'iphoneos', 'clang']:
        raise RuntimeError('Release packaging requires Apple Xcode clang; Zig payloads are development-only')
    if not cloud.get('xcode', '').startswith('Xcode 16.4') or 'iPhoneOS18.5.sdk' not in native['sdk']:
        raise RuntimeError('Release payload must use the established Xcode 16.4 / iPhoneOS18.5 toolchain')
    library = ROOT/'build/RVPort.dylib'
    # Build provenance guards, not archive/regression/device tests.
    if digest(library) != native['payload_sha256']:
        raise RuntimeError('Payload changed since compilation')
    sources = {p.relative_to(ROOT/'native').as_posix(): digest(p)
               for p in sorted((ROOT/'native').rglob('*')) if p.is_file()}
    if sources != native['source_files']:
        raise RuntimeError('Native sources changed since compilation')
    baseline = json.loads((ROOT/'profiles/sponsor-prompts-baseline.json').read_text(encoding='utf-8'))
    preserved = {name: value for name, value in sources.items()
                 if baseline['native_sources'].get(name) == value}
    changed = {name: value for name, value in sources.items()
               if baseline['native_sources'].get(name) != value}
    original = WORKSPACE/'ReVanced/Ghidra Project/com.google.ios.youtube_21.39.4_und3fined.ipa'
    destination = WORKSPACE/'ReVanced/patcher/output'
    # Explicit user instruction: no tests. Keep normal production input/config/
    # payload validation, but do not execute patcher's archive self-check.
    patcher.verify = lambda output: None
    packages = []
    for suffix, config_name in (('-SideStore-auth', 'sideload-auth'),
                                ('-SideStore-auth-native-ads', 'adblock-native')):
        config_path = ROOT/f'configs/{config_name}.json'
        output = destination/f'YouTube-21.39.4-RVPort-{VERSION}{suffix}-unsigned.ipa'
        print(f'Packaging {output.name}', flush=True)
        config = json.loads(config_path.read_text(encoding='utf-8'))
        marker = patcher.patch(original, output, library, config, strip_extensions=True)
        packages.append({'path': str(output), 'sha256': digest(output),
                         'size': output.stat().st_size, 'config_sha256': digest(config_path),
                         'bundled_ad_strategy': marker['config']['ad_strategy'], 'marker': marker})
        print(f'Created {output.name} ({output.stat().st_size:,} bytes)', flush=True)
    receipt = {
        'version': VERSION, 'created_utc': datetime.now(timezone.utc).isoformat(),
        'source_checkout': str(ROOT.parent), 'baseline': baseline['source'],
        'build_only': True, 'tests_run': False, 'archive_self_check_run': False,
        'gui_smoke_test_run': False, 'static_hook_check_run': False,
        'device_validated': False, 'signing_required': True,
        'test_skip_reason': 'User explicitly instructed: Do not run tests.',
        'packaging': 'Production input/config/payload validation; archive self-check disabled.',
        'diagnostics_revision': 'sponsor-prompts-1', 'native_build': native, 'cloud_build': cloud,
        'preserved_native_sources': preserved, 'changed_or_added_native_sources': changed,
        'packages': packages,
        'limits': ['Apple Xcode payload replaces the crashing Zig delivery; device crash resolution unverified.',
                   'Frosted-glass style-provider experiments remain unmapped.',
                   'Saved preferences override bundled defaults, including ad strategy.']}
    encoded = json.dumps(receipt, indent=2)+'\n'
    for target in (ROOT/f'build/package-manifest-{VERSION}.json',
                   ROOT/'profiles/sponsor-prompts-implementation-evidence.json',
                   destination/f'RVPort-{VERSION}-build-receipt.json'):
        target.write_text(encoded, encoding='utf-8')
    print(json.dumps({'version': VERSION, 'packages': len(packages),
                      'preserved_native_files': len(preserved),
                      'changed_or_added': sorted(changed), 'tests_run': False}), flush=True)


if __name__ == '__main__':
    main()
