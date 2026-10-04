"""Audit already-built 0.3.4 artifacts and write their release metadata."""
import json
from pathlib import Path
import zipfile

from features import CATALOG, FEATURES
from patcher import file_sha, sha, validate_config

ROOT = Path(__file__).resolve().parent
VERSION = '0.3.4'


def read(name):
    return json.loads((ROOT / name).read_text(encoding='utf-8'))


def write(name, value):
    (ROOT / name).write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')


def require(condition, message):
    if not condition:
        raise SystemExit(message)


def main():
    build = read('build/build-manifest.json')
    payload_sha = file_sha(ROOT / 'build/RVPort.dylib')
    require(payload_sha == build['payload_sha256'], 'Stale payload build manifest')
    for name, expected in build['source_files'].items():
        require(file_sha(ROOT / 'native' / name) == expected, f'Stale native source: {name}')
    native = '\n'.join(p.read_text(encoding='utf-8') for p in (ROOT / 'native').iterdir()
                       if p.suffix in ('.m', '.inc'))
    require(all(f'@"{key}"' in native for key in FEATURES), 'Feature missing from native implementation')
    coverage = read('coverage.json')
    require(coverage['patcher_version'] == VERSION, 'Stale coverage')
    require(len({p['id'] for p in coverage['patches']}) == coverage['declaration_count'] == 113,
            'Incomplete or duplicate declaration coverage')
    require(sum(p['named_inventory'] for p in coverage['patches']) == 51, 'Incomplete named inventory')
    require(all(p['implementation'] and p['limits'] for p in coverage['patches']), 'Missing coverage detail')
    hooks = read('build/hook-check.json')
    require(hooks['counts'] == {'match': 81, 'mismatch': 0, 'runtime_resolution_required': 2},
            'Unexpected direct-literal hook results')
    ci = read('build/ci-evidence-0.3.4.json')
    require(ci['conclusion']=='success' and ci['tests_passed']==37 and ci['native_auth_harness_passed'] and ci['native_miniplayer_harness_passed'] and ci['native_settings_harness_passed'] and ci['windows_package_passed'], 'Missing passing authentication CI evidence')
    require(ci['payload_sha256']==payload_sha, 'CI payload mismatch')
    reports = ['build/hook-check.json', 'build/ci-evidence-0.3.4.json']
    for name in ('gui-smoke-0.3.4', 'packaged-smoke-0.3.4'):
        path = f'build/{name}.json'
        report = read(path)
        require(report['status'] == 'ok' and report['tk_interface_initialized']
                and report['payload_sha256'] == payload_sha and report['feature_switches'] == len(FEATURES),
                f'Stale/failed smoke check: {name}')
        reports.append(path)
    outputs = []
    for suffix, config in (('', 'defaults'), ('-expanded', 'expanded'), ('-SideStore-auth', 'sideload-auth')):
        name = f'output/YouTube-21.39.4-RVPort-0.3.4{suffix}-unsigned.ipa'
        verification = f'build/verification-0.3.4-{config}.json'
        report = read(verification)
        require(report['status'] == 'verified' and report['retained_members_identical'] == (11760 if config=='sideload-auth' else 13462)
                and report['removed_members'] == (1718 if config=='sideload-auth' else 16) and report['existing_load_commands_preserved']
                and report['section_and_linkedit_bytes_preserved'] and report['zip_crc_verified'],
                f'Failed archive check: {name}')
        require(file_sha(ROOT / name) == report['output_ipa_sha256'], f'Stale archive check: {name}')
        with zipfile.ZipFile(ROOT / name) as archive:
            base = 'Payload/YouTube.app/'
            marker = json.loads(archive.read(base + 'RVPortPatchManifest.json'))
            actual = json.loads(archive.read(base + 'RVPort.json'))
            require(marker['patcher_version'] == VERSION and marker['payload_sha256'] == payload_sha
                    and sha(archive.read(base + 'Frameworks/RVPort.dylib')) == payload_sha,
                    f'Wrong packaged payload: {name}')
            require(actual == marker['config'] == validate_config(read(f'configs/{config}.json')),
                    f'Wrong packaged config: {name}')
        outputs.append(name)
        reports.append(verification)
    source_ipa = ROOT.parent / 'Ghidra Project/com.google.ios.youtube_21.39.4_und3fined.ipa'
    source_sha = file_sha(source_ipa)
    require(source_sha == '37fd59f89d706fb7f93614e12ddb9c09fe3f4a18c1e4609c2b715db6e9f3d88c',
            'Original source IPA changed')
    artifact_names = ['dist/YouTube-iOS-Patcher.exe', 'build/RVPort.dylib', *outputs,
                      'coverage.json', 'COVERAGE.md', 'configs/defaults.json',
                      'configs/expanded.json', 'configs/all-candidates.json', 'configs/sideload-auth.json', 'AUTHENTICATION_SCHEME.md', 'MINIPLAYER_SCHEME.md', 'profiles/miniplayer-evidence.json', 'SETTINGS_SCHEME.md', 'profiles/settings-evidence.json', 'profiles/device-auth-login-success.json', 'README.md', 'DEVICE_TESTS.md']
    artifacts = {name: {'sha256': file_sha(ROOT / name), 'size': (ROOT / name).stat().st_size}
                 for name in artifact_names}
    target = read('build/device-test-target.json')
    manifest = {
        'version': VERSION, 'supported_version': '21.39.4', 'feature_switches': len(FEATURES),
        'expanded_enabled_switches': sum(read('configs/expanded.json')[key] for key in FEATURES),
        'auth_preset_enabled_switches':sum(read('configs/sideload-auth.json')[key] for key in FEATURES),
        'tests_passed': 37, 'test_scope': {'patcher_and_shared_helpers': 19, 'cloud_signing_and_download': 12, 'authentication_config_and_native_harness':2, 'miniplayer_config_and_native_harness':2, 'settings_catalog_and_native_harness':2}, 'ci':ci,
        'native_compilation': 'ARM64 iOS 17, warnings treated as errors',
        'hook_check': hooks['counts'], 'hook_check_scope': hooks['scope'],
        'coverage': {'declarations': coverage['declaration_count'], 'named_inventory': 51,
                     'statuses': coverage['status_counts']},
        'verification_files': reports, 'verification_sha256': {p: file_sha(ROOT / p) for p in reports},
        'artifacts': artifacts,
        'source_files': {str(p.relative_to(ROOT)).replace('\\', '/'): file_sha(p)
                         for p in [*ROOT.glob('*.py'), *sorted((ROOT / 'native').iterdir()),
                                   *sorted((ROOT / 'tests').glob('*.*'))] if p.is_file()},
        'original_ipa_sha256': source_sha, 'device_validated': False, 'signing_required': True,
        'device_test_target': target, 'live_contributions_tested': False,
        'remaining_scope': 'Per-patch partial behavior and blocked stream/header wrappers in COVERAGE.md; '
                           'The 0.3.1 auth IPA installs and its authentication hooks run but login failed; the 0.3.2 authentication adapter retained in 0.3.4 is user-reported to permit login; refresh, playback and new settings/UI behavior require device evidence.'}
    write('build/release-manifest-0.3.4.json', manifest)
    write('build/release-manifest.json', manifest)
    old_results = read('build/device-results-template.json')
    old_by_key = {test['key']: test for test in old_results['tests']}
    tests = [old_by_key.get(key, {'key': key, 'label': label, 'status': 'pending', 'notes': ''})
             for key, label in CATALOG.items()]
    extra = {'native_settings_entry':'YouTube Settings to ReVanced, grouped/linear menus, Back and fallback entry',
             'native_settings_preferences':'Search, value editors, export/import, reset isolation and cold-launch persistence',
             'miniplayer_geometry_and_opacity':'Minimum dimension/window bounds and circular-background opacity through fades and reopen',
             'miniplayer_transition_masks':'Square corners, transition masks and layout updates',
             'authentication_login_persistence':'Sign-in, authenticated operation, cold relaunch, token refresh and same-identity SideStore refresh',
             'authentication_diagnostics':'Redacted auth events, callback/error presence, numeric keychain failures and adapter toggles',
             'sponsor_category_policies': 'All five category policies, minimum duration and skip-once rewind',
             'sponsor_highlights': 'Point jumps at zero/nonzero time and undo',
             'sponsor_marker_geometry': 'Markers, category colors and player ownership across layouts',
             'network_quality_policy': 'Wi-Fi/cellular/unknown routes, explicit Auto and remembering',
             'thumbnail_context_policy': 'Five screen contexts and original/stills/DeArrow fallbacks',
             'palette_backgrounds': 'Light/dark colors on both palette classes',
             'diagnostic_export': 'Copy configuration and report; verify profile and hook outcomes'}
    tests.extend(old_by_key.get(key, {'key': key, 'label': label, 'status': 'pending', 'notes': ''})
                 for key, label in extra.items())
    write('build/device-results-template.json', {'target': target, 'patcher_version': VERSION,
          'instructions': 'pending means not executed, not passed. Save Copy diagnostic report and '
                          'actual device behavior. See DEVICE_TESTS.md.', 'tests': tests})
    audit = {'objective': 'Port additional feasible local ReVanced equivalents and rebuild verified artifacts.',
             'host_artifact_stage_complete': True, 'full_revanced_parity': False,
             'evidence': {'native_switches': len(FEATURES), 'tests_passed': 37,
                          'literal_and_getter_array_abi_matches': 81, 'covered_declarations': 113,
                          'named_inventory': 51, 'archive_reports': reports,
                          'source_original_preserved': True, 'payload_matches_sources_and_packages': True},
             'limitations': {'device_validated': False, 'signing_required': True,
                             'live_contributions_tested': False,
                             'remaining_behavior': manifest['remaining_scope']}}
    write('build/completion-audit.json', audit)
    print(json.dumps({'version': VERSION, 'feature_switches': len(FEATURES),
                      'artifacts_audited': len(artifacts), 'device_validated': False}))


if __name__ == '__main__':
    main()
