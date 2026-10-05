"""Record 0.3.28 build artifacts without executing regression or smoke tests."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parent
VERSION = '0.3.28'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run', type=Path, required=True)
    parser.add_argument('--jobs', type=Path, required=True)
    args = parser.parse_args()
    run = json.loads(args.run.read_text(encoding='utf-8'))
    jobs = json.loads(args.jobs.read_text(encoding='utf-8'))
    native = json.loads((ROOT/'build/build-manifest.json').read_text(encoding='utf-8'))
    names = ['build.py', 'build/RVPort.dylib', 'dist/YouTube-iOS-Patcher.exe',
             'build/build-manifest.json', 'build/build.log',
             'native/RVPort.m', 'native/RVExtras.inc', 'native/RVSettingsUI.inc',
             'native/RVAuthentication.inc', 'native/RVAuthenticationSupport.h',
             'SPONSORBLOCK_SCHEME.md', 'profiles/sponsorblock-evidence.json',
             'profiles/sponsorblock-response-evidence.json', 'profiles/device-sponsorblock-0.3.5-failure.json',
             'coverage.json', 'COVERAGE.md', 'native/RVMiniplayer.inc',
             'native/RVFeatures.h', 'native/RVSettingsCatalog.h', 'MINIPLAYER_SCHEME.md',
             'profiles/miniplayer-controls-evidence.json', 'features.py', 'settings_catalog.py', 'native/RVNativeDislikes.inc',
             'NATIVE_DISLIKES_SCHEME.md', 'profiles/native-dislikes-evidence.json',
             'native/RVSettingsNativeUI.inc', 'native/RVSettingsRows.inc', 'native/RVSettingsIcon.inc',
             'native/assets/ReVancedSettings.png', 'native/RVDislikesUI.inc', 'SETTINGS_UI_FIX_SCHEME.md',
             'profiles/settings-ui-evidence.json', 'profiles/parallel-ui-snapshot-0.3.9.json',
             'profiles/ryd-native-vote-contracts.json', 'native/RVElementDislikes.inc', 'ELEMENT_DISLIKES_SCHEME.md', 'profiles/element-dislikes-evidence.json', 'profiles/device-0.3.9-ryd-display-failure.json', 'profiles/device-0.3.10-vote-display-failure.json', 'profiles/device-0.3.11-vote-display-failure.json', 'profiles/parallel-ui-snapshot-0.3.12.json', 'native/RVSettingsBridge.inc', 'native/RVSettingsNavigation.inc', 'native/RVDislikesDiagnostics.inc', 'RYD_CHECKPOINTS.md', 'profiles/ryd-checkpoints-evidence.json', 'profiles/device-report-541-ryd-render.json', 'profiles/device-report-615-ryd-mount.json', 'profiles/device-report-635-ryd-lifecycle.json', 'profiles/ryd-lifecycle-evidence.json', 'profiles/ryd-speed-polish-evidence.json', 'native/RVSpeed.inc', 'native/RVSpeedBridge.inc', 'profiles/device-report-728-startup.json', 'profiles/startup-bridge-evidence.json', 'PLAYBACK_SPEED_SCHEME.md', 'profiles/parallel-ui-snapshot-0.3.14.json', 'profiles/device-0.3.12-ryd-render-failure.json']
    names += ['native/RVSpeedDiagnostics.inc', 'SPEED_CHECKPOINTS.md', 'profiles/speed-checkpoints-evidence.json', 'profiles/device-report-755-speed.json']
    names += ['native/RVVoteLayoutCompatibility.inc', 'VOTE_LAYOUT_SCHEME.md', 'profiles/vote-layout-compatibility-evidence.json', 'profiles/device-report-823-vote-layout.json']
    names += ['native/RVVoteModel.inc', 'native/RVVoteLifecycle.inc', 'VOTE_COUNTER_FIX_SCHEME.md', 'VOTE_STARTUP_INVESTIGATION.md', 'profiles/vote-model-implementation-evidence.json', 'profiles/vote-startup-investigation.json', 'profiles/vote-counter-fix-scheme.json']
    names += ['configs/defaults.json', 'configs/expanded.json', 'configs/sideload-auth.json', 'configs/all-candidates.json']
    names += ['native/RVVoteFirstLaunch.inc', 'FIRST_LAUNCH_UI_SCHEME.md', 'profiles/first-launch-ui-evidence.json']
    names += [f'output/YouTube-21.39.4-RVPort-{VERSION}{suffix}-unsigned.ipa'
              for suffix in ('-merged', '-expanded-merged', '-SideStore-auth-merged', '-SideStore-auth-native-ads-merged')]
    names += ['native/RVAds.inc', 'native/RVAdFeed.inc', 'native/RVAdsDiagnostics.inc', 'configs/adblock-native.json', 'ADBLOCK_IMPLEMENTATION.md', 'ADBLOCK_PORT_SCHEME.md', 'profiles/adblock-port-evidence.json', 'profiles/adblock-implementation-evidence.json', 'profiles/adblock-merge-0.3.23.json']
    names += ['native/RVAdCoordinator.inc', 'ADBLOCK_HANDOFF_SCHEME.md', 'ADBLOCK_HANDOFF_IMPLEMENTATION.md', 'profiles/adblock-handoff-evidence.json', 'profiles/adblock-handoff-implementation-evidence.json', 'profiles/adblock-handoff-decompiled.txt', 'profiles/device-adblock-report-1251.json']
    names += ['native/RVAdDisplay.inc', 'native/RVAdDisplayDiagnostics.inc', 'ADBLOCK_DISPLAY_SCHEME.md', 'ADBLOCK_DISPLAY_IMPLEMENTATION.md', 'profiles/adblock-display-evidence.json', 'profiles/adblock-display-protobuf-fields.json', 'profiles/adblock-display-decompiled.txt', 'profiles/adblock-display-implementation-evidence.json']
    names += ['native/RVSponsorPrompt.inc', 'native/RVSponsorPromptUI.inc', 'SPONSORBLOCK_NATIVE_PROMPTS_SCHEME.md', 'SPONSORBLOCK_NATIVE_PROMPTS_IMPLEMENTATION.md', 'profiles/sponsor-prompt-evidence.json', 'profiles/sponsor-prompts-implementation-evidence.json', 'MERGED_DISPLAY_SPONSOR_IMPLEMENTATION.md', 'profiles/merged-display-sponsor-source.json', 'package_merged_release.py', 'build/cloud-build.json']
    artifacts = {}
    for name in names:
        path = ROOT/name
        with path.open('rb') as stream:
            digest = hashlib.file_digest(stream, 'sha256').hexdigest()
        artifacts[name] = {'sha256': digest, 'size': path.stat().st_size}
    receipt = {
        'version': VERSION, 'supported_youtube_version': '21.39.4',
        'feature_switches': 82, 'runtime_preferences': 116,
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
        'remaining_scope': '0.3.28 merges typed display-ad filtering and native companion clearing with SponsorBlock manual/confirmed-undo prompts from 0.3.26. Both checkpoint sections are included. Opaque Home/header ads and advertising replacement removal remain conservative passthrough. Native prompt appearance/hit testing/seek confirmation and combined device behavior remain unverified. The separate 0.3.27 Shorts worktree was still being edited and is not included. No tests run.'}
    encoded = json.dumps(receipt, indent=2)+'\n'
    for name in ('build/release-manifest.json', f'build/release-manifest-{VERSION}.json',
                 f'profiles/release-{VERSION}.json'):
        (ROOT/name).write_text(encoded, encoding='utf-8')
    print(json.dumps({'version': VERSION, 'artifacts_recorded': len(artifacts),
                      'tests_run': False, 'device_playback_verified': False}))


if __name__ == '__main__':
    main()
