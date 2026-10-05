"""Package the merged Xcode payload with production input checks and no tests."""
import hashlib
import json
from pathlib import Path
import patcher

ROOT = Path(__file__).resolve().parent
WORKSPACE = ROOT.parents[4]
VERSION = '0.3.31'


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    native = json.loads((ROOT/'build/build-manifest.json').read_text(encoding='utf-8'))
    cloud = json.loads((ROOT/'build/cloud-build.json').read_text(encoding='utf-8'))
    if native['compiler_command'][:4] != ['xcrun', '--sdk', 'iphoneos', 'clang']:
        raise RuntimeError('Merged release requires Apple Xcode clang')
    if not cloud.get('xcode', '').startswith('Xcode 16.4') or 'iPhoneOS18.5.sdk' not in native['sdk']:
        raise RuntimeError('Merged release requires the established Xcode 16.4 / iPhoneOS18.5 SDK')
    library = ROOT/'build/RVPort.dylib'
    if digest(library) != native['payload_sha256']:
        raise RuntimeError('Payload changed after compilation')
    sources = {path.relative_to(ROOT/'native').as_posix(): digest(path)
               for path in sorted((ROOT/'native').rglob('*')) if path.is_file()}
    if sources != native['source_files']:
        raise RuntimeError('Native sources differ from the cloud build')
    # Existing user instruction: no archive self-check, regression or smoke tests.
    # The production patcher still validates original IPA, profile, config and dylib.
    patcher.verify = lambda output: None
    original = WORKSPACE/'ReVanced/Ghidra Project/com.google.ios.youtube_21.39.4_und3fined.ipa'
    destination = ROOT/'output'
    destination.mkdir(exist_ok=True)
    packages = []
    # One comprehensive SideStore delivery; never generate strategy/preset variants.
    for config_name, strip in [('adblock-native', True)]:
        output = destination/f'YouTube-21.39.4-RVPort-{VERSION}-unsigned.ipa'
        if output.exists():
            raise RuntimeError('Refusing to overwrite an existing release IPA')
        print('Packaging '+output.name, flush=True)
        config_path = ROOT/f'configs/{config_name}.json'
        config = json.loads(config_path.read_text(encoding='utf-8'))
        marker = patcher.patch(original, output, library, config, strip_extensions=strip)
        packages.append({'filename': output.name, 'size': output.stat().st_size,
                         'sha256': digest(output), 'config_sha256': digest(config_path),
                         'marker': marker})
        print('Created '+output.name, flush=True)
    receipt = {'version': VERSION, 'release_flavor': 'merged-elements-inline-adblock',
               'source_commit': cloud['head_sha'], 'cloud_build': cloud, 'native_build': native,
               'packages': packages, 'tests_run': False, 'archive_self_check_run': False,
               'gui_smoke_test_run': False, 'static_hook_check_run': False,
               'device_validated': False, 'signing_required': True,
               'merge': json.loads((ROOT/'profiles/merged-speed-shorts-source.json').read_text(encoding='utf-8')),
               'scope': 'Structural Elements display-ad removal and retained-cell fallback, verified inline-player ownership and checkpoint revision 5. Native speed configuration preservation and sheet checkpoints, merged with 0.3.29 ad ownership/display filtering and 0.3.27 Shorts model/header/pivot fixes. SponsorBlock prompts, authentication and native settings retained. One SideStore-compatible native-ad preset.'}
    (ROOT/'build/package-manifest-0.3.31.json').write_text(json.dumps(receipt, indent=2)+'\n', encoding='utf-8')
    print(json.dumps({'version': VERSION, 'packages': len(packages), 'tests_run': False}))


if __name__ == '__main__':
    main()
