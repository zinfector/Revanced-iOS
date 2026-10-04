"""Record 0.3.5 build artifacts without executing regression or smoke tests."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent
VERSION = '0.3.5'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run', type=Path, required=True)
    parser.add_argument('--jobs', type=Path, required=True)
    args = parser.parse_args()
    run = json.loads(args.run.read_text(encoding='utf-8'))
    jobs = json.loads(args.jobs.read_text(encoding='utf-8'))
    native = json.loads((ROOT/'build/build-manifest.json').read_text(encoding='utf-8'))
    names = ['build/RVPort.dylib', 'dist/YouTube-iOS-Patcher.exe',
             'build/build-manifest.json', 'build/build.log',
             'native/RVPort.m', 'native/RVExtras.inc', 'native/RVSettingsUI.inc',
             'native/RVAuthentication.inc', 'native/RVAuthenticationSupport.h',
             'SPONSORBLOCK_SCHEME.md', 'profiles/sponsorblock-evidence.json',
             'coverage.json', 'COVERAGE.md']
    names += [f'output/YouTube-21.39.4-RVPort-{VERSION}{suffix}-unsigned.ipa'
              for suffix in ('', '-expanded', '-SideStore-auth')]
    artifacts = {}
    for name in names:
        path = ROOT/name
        digest = hashlib.file_digest(path.open('rb'), 'sha256').hexdigest()
        artifacts[name] = {'sha256': digest, 'size': path.stat().st_size}
    receipt = {
        'version': VERSION, 'supported_youtube_version': '21.39.4',
        'feature_switches': 79, 'runtime_preferences': 113,
        'build_only': True, 'tests_run': False, 'tests_passed': None,
        'test_skip_reason': 'User explicitly instructed: Do not run tests.',
        'gui_smoke_test_run': False, 'static_hook_check_run': False,
        'standalone_archive_test_run': False,
        'packaging': 'IPAs created by the production patcher with its normal source/config/payload validation; no regression or integration test suite executed.',
        'device_playback_verified': False, 'signing_required': True,
        'authentication_login': 'Previously user-reported working; authentication implementation unchanged.',
        'source_commit': run['head_sha'],
        'ci': {'run_id': run['id'], 'url': run['html_url'],
               'conclusion': run['conclusion'],
               'jobs': [{'name': job['name'], 'conclusion': job['conclusion'],
                         'steps': [{'name': step['name'], 'conclusion': step['conclusion']}
                                   for step in job.get('steps', [])]}
                        for job in jobs['jobs']]},
        'native_build': native, 'artifacts': artifacts,
        'remaining_scope': 'SponsorBlock skip/marker behavior on the device has not been observed on this build. Earlier passing test receipts apply only to earlier versions.'}
    encoded = json.dumps(receipt, indent=2)+'\n'
    for name in ('build/release-manifest.json', f'build/release-manifest-{VERSION}.json',
                 f'profiles/release-{VERSION}.json'):
        (ROOT/name).write_text(encoded, encoding='utf-8')
    print(json.dumps({'version': VERSION, 'artifacts_recorded': len(artifacts),
                      'tests_run': False, 'device_playback_verified': False}))


if __name__ == '__main__':
    main()
