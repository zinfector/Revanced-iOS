"""Record production artifacts; do not execute tests or smoke checks."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent
VERSION = '0.3.43'


def digest(path):
    with path.open('rb') as stream:
        return hashlib.file_digest(stream, 'sha256').hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run', type=Path, required=True)
    parser.add_argument('--jobs', type=Path, required=True)
    args = parser.parse_args()
    run = json.loads(args.run.read_text(encoding='utf-8'))
    jobs = json.loads(args.jobs.read_text(encoding='utf-8'))
    package = json.loads((ROOT/f'build/package-manifest-{VERSION}.json').read_text(encoding='utf-8'))
    native = package['native_build']
    names = ['build.py', 'package_merged_release.py', 'release_build_receipt.py',
             'build/RVPort.dylib', 'build/build-manifest.json', 'build/build.log',
             'build/cloud-build.json', f'build/package-manifest-{VERSION}.json',
             'dist/YouTube-iOS-Patcher.exe', 'dist/YouTube-USB-Diagnostics.exe',
             'usb_diagnostics.py', 'profiles/usb-diagnostics-contract.json', 'coverage.json', 'features.py',
             'settings_catalog.py', 'configs/adblock-native.json',
             'profiles/dearrow-adaptive-contract.json', 'profiles/dearrow-live-card-contract.json',
             'profiles/dearrow-live-capture-contract.json']
    names += ['native/'+name for name in native['source_files']]
    names += ['output/'+item['filename'] for item in package['packages']]
    artifacts = {}
    for name in names:
        path = ROOT/name
        artifacts[name] = {'sha256': digest(path), 'size': path.stat().st_size}
    receipt = {
        'version': VERSION, 'supported_youtube_version': '21.39.4',
        'source_commit': package['source_commit'], 'build_only': True,
        'tests_run': False, 'tests_passed': None,
        'test_skip_reason': 'User explicitly instructed: Do not run tests.',
        'gui_smoke_test_run': False, 'static_hook_check_run': False,
        'standalone_archive_test_run': False, 'device_playback_verified': False,
        'device_dearrow_verified': False, 'signing_required': True,
        'wired_device_discovery_verified': True, 'device_bridge_verified': False,
        'prior_0_3_41_usb_report_delivery_verified': True, 'persistent_reconnect_device_verified': False,
        'scope': package['scope'], 'native_build': native,
        'ci': {'run_id': run['id'], 'url': run['html_url'],
               'conclusion': run['conclusion'],
               'jobs': [{'name': job['name'], 'conclusion': job['conclusion'],
                         'steps': [{'name': step['name'], 'conclusion': step['conclusion']}
                                   for step in job.get('steps', [])]}
                        for job in jobs['jobs']]},
        'packages': package['packages'], 'artifacts': artifacts,
        'interaction_issue': 'User reports watch-page controls now work. Latest 0.3.41 USB report has no active watch page/tap events; no causal claim or native interaction-hook changes in this release.'}
    encoded = json.dumps(receipt, indent=2)+'\n'
    for name in ('build/release-manifest.json', f'build/release-manifest-{VERSION}.json',
                 f'profiles/release-{VERSION}.json'):
        (ROOT/name).write_text(encoded, encoding='utf-8')
    print(json.dumps({'version': VERSION, 'artifacts_recorded': len(artifacts),
                      'tests_run': False, 'device_playback_verified': False}))


if __name__ == '__main__':
    main()
