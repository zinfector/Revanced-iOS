"""Re-sign a verified RVPort IPA on macOS using user-supplied Apple credentials.

Only the signed IPA and a credential-free report are published. Temporary keychains,
certificates, profiles and entitlement files remain in RUNNER_TEMP and are cleaned.
"""
import argparse
import base64
import copy
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import plistlib
import re
import secrets
import shutil
import stat
import subprocess
import sys
import tempfile
import zipfile

from patcher import file_sha, members, verify


class SigningError(RuntimeError):
    pass


def run(args):
    result = subprocess.run(args, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    if result.returncode:
        # security commands include passwords in argv. Never format args or print their output.
        raise SigningError(f'{Path(args[0]).name} failed (exit {result.returncode}); check signing credentials/profile compatibility')
    return result.stdout


def decode_secret(name, maximum=2 * 1024**2):
    value = os.environ.get(name, '')
    if not value:
        raise SigningError(f'Missing GitHub Actions secret: {name}')
    try:
        raw = base64.b64decode(''.join(value.split()), validate=True)
    except (ValueError, UnicodeError):
        raise SigningError(f'{name} must contain Base64 file bytes') from None
    if not raw or len(raw) > maximum:
        raise SigningError(f'{name} is empty or too large')
    return raw


def bundle_valid(bundle):
    return isinstance(bundle, str) and len(bundle) <= 255 and bool(re.fullmatch(r'[A-Za-z0-9-]+(?:\.[A-Za-z0-9-]+)+', bundle))


def profile_pattern(profile):
    value = profile.get('Entitlements', {}).get('application-identifier')
    prefixes = profile.get('ApplicationIdentifierPrefix', [])
    if not isinstance(value, str) or not isinstance(prefixes, list):
        raise SigningError('Provisioning profile has no application identifier/prefix')
    for prefix in prefixes:
        if isinstance(prefix, str) and value.startswith(prefix + '.'):
            pattern = value[len(prefix) + 1:]
            if pattern == '*' or bundle_valid(pattern) or pattern.endswith('.*') and pattern.count('*') == 1 and bundle_valid(pattern[:-1] + 'placeholder'):
                return prefix, pattern
    raise SigningError('Unsupported provisioning application identifier')


def profile_matches(profile, bundle):
    if not bundle_valid(bundle):
        return False
    _, pattern = profile_pattern(profile)
    return pattern == '*' or (pattern.endswith('.*') and bundle.startswith(pattern[:-1])) or pattern == bundle


def validate_profile(profile, identity, team=None):
    if 'iOS' not in profile.get('Platform', []):
        raise SigningError('An iOS provisioning profile is required')
    expiry = profile.get('ExpirationDate')
    if not isinstance(expiry, datetime):
        raise SigningError('Provisioning profile is missing an expiration date')
    if expiry.replace(tzinfo=timezone.utc) <= datetime.now(timezone.utc):
        raise SigningError('Provisioning profile has expired')
    teams = profile.get('TeamIdentifier')
    if not isinstance(teams, list) or len(teams) != 1 or not isinstance(teams[0], str):
        raise SigningError('Provisioning profile has no unambiguous signing team')
    if team is not None and teams[0] != team:
        raise SigningError('Retained extension profile belongs to a different team')
    allowed = profile.get('DeveloperCertificates', [])
    if not any(isinstance(cert, bytes) and hashlib.sha1(cert).hexdigest().upper() == identity.upper() for cert in allowed):
        raise SigningError('Imported signing certificate is not authorized by a provisioning profile')
    if not profile.get('ProvisionedDevices') and not profile.get('ProvisionsAllDevices'):
        raise SigningError('Use a development, Ad Hoc or enterprise profile for direct installation; App Store profiles are refused')
    profile_pattern(profile)
    return teams[0]


def signing_entitlements(profile, bundle):
    if not profile_matches(profile, bundle):
        raise SigningError('Bundle identifier does not match the provisioning profile')
    prefix, _ = profile_pattern(profile)
    result = copy.deepcopy(profile['Entitlements'])
    result['application-identifier'] = prefix + '.' + bundle
    result['com.apple.developer.team-identifier'] = profile['TeamIdentifier'][0]
    for key in ('keychain-access-groups',):
        if key in result:
            groups = result[key]
            if not isinstance(groups, list) or not all(isinstance(group, str) for group in groups):
                raise SigningError('Malformed provisioning keychain groups')
            result[key] = [prefix + '.' + bundle if group == prefix + '.*' else group for group in groups]
    # Remaining wildcard capabilities need an explicit profile; do not guess iCloud/group identifiers.
    def has_wildcard(value):
        if isinstance(value, str):return '*' in value
        if isinstance(value, list):return any(has_wildcard(v) for v in value)
        if isinstance(value, dict):return any(has_wildcard(v) for v in value.values())
        return False
    if has_wildcard(result):
        raise SigningError('Profile contains unresolved wildcard capabilities; use explicit app/extension profiles')
    return result


def extract_ipa(source, destination):
    with zipfile.ZipFile(source) as archive:
        members(archive)
        for info in archive.infolist():
            path = destination.joinpath(*info.filename.rstrip('/').split('/'))
            if info.is_dir():
                path.mkdir(parents=True, exist_ok=True)
                continue
            path.parent.mkdir(parents=True, exist_ok=True)
            with archive.open(info) as original, path.open('xb') as output:
                shutil.copyfileobj(original, output, 1024 * 1024)
            mode = (info.external_attr >> 16) & 0o777
            path.chmod(mode or 0o644)
    apps = list((destination / 'Payload').glob('*.app'))
    if len(apps) != 1:
        raise SigningError('Expected one main app in the IPA')
    return apps[0]


def macho_file(path):
    with path.open('rb') as stream:
        return stream.read(4) in (b'\xcf\xfa\xed\xfe', b'\xce\xfa\xed\xfe', b'\xfe\xed\xfa\xcf',
                                 b'\xfe\xed\xfa\xce', b'\xca\xfe\xba\xbe', b'\xbe\xba\xfe\xca',
                                 b'\xca\xfe\xba\xbf', b'\xbf\xba\xfe\xca')


def change_bundle(info, old, new):
    result = copy.deepcopy(info)
    result['CFBundleIdentifier'] = new
    # Extension-host references are identifiers, not Google OAuth URL schemes.
    attributes = result.get('NSExtension', {}).get('NSExtensionAttributes', {})
    if attributes.get('WKAppBundleIdentifier') == old:
        attributes['WKAppBundleIdentifier'] = new
    return result


def signing_plan(app):
    """Sign nested code first, each extension with its own profile, then the main app."""
    extensions = sorted(app.rglob('*.appex'), key=lambda p: len(p.parts), reverse=True)
    frameworks = sorted(app.rglob('*.framework'), key=lambda p: len(p.parts), reverse=True)
    containers = extensions + [app]
    bundle_executables = set()
    for bundle in containers + frameworks:
        info_path = bundle / 'Info.plist'
        if info_path.is_file():
            info = plistlib.loads(info_path.read_bytes())
            exe = info.get('CFBundleExecutable')
            if not isinstance(exe, str) or '/' in exe or '\\' in exe or exe in ('.', '..'):
                raise SigningError('Bundle has an unsafe/missing executable name')
            executable = bundle / exe
            if not executable.is_file():
                raise SigningError('Bundle executable is missing')
            bundle_executables.add(executable)
    leaves = sorted((p for p in app.rglob('*') if p.is_file() and p not in bundle_executables and macho_file(p)),
                    key=lambda p: len(p.parts), reverse=True)
    return leaves, frameworks, extensions


def load_profiles(work):
    paths = [work / 'main.mobileprovision']
    paths[0].write_bytes(decode_secret('BUILD_PROVISION_PROFILE_BASE64'))
    if os.environ.get('EXTENSION_PROFILES_ZIP_BASE64'):
        archive_path = work / 'extensions.zip'
        archive_path.write_bytes(decode_secret('EXTENSION_PROFILES_ZIP_BASE64', 16 * 1024**2))
        with zipfile.ZipFile(archive_path) as archive:
            members(archive)
            entries = archive.infolist()
            profiles = [entry for entry in entries if not entry.is_dir()]
            if len(profiles) > 20 or any(not entry.filename.endswith('.mobileprovision') or entry.file_size > 2 * 1024**2 for entry in profiles):
                raise SigningError('Extension ZIP must contain at most 20 provisioning profiles only')
            for index, entry in enumerate(profiles):
                path = work / f'extension-{index}.mobileprovision'
                path.write_bytes(archive.read(entry))
                paths.append(path)
    return [(path, plistlib.loads(run(['security', 'cms', '-D', '-i', str(path)]))) for path in paths]


def sign(source, output, requested_bundle='', strip_extensions=True):
    if sys.platform != 'darwin':
        raise SigningError('IPA signing requires macOS security and codesign tools')
    if output.exists() or source.resolve() == output.resolve():
        raise SigningError('Refusing to overwrite an output or the unsigned source')
    verify(source)
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='rvport-sign-', dir=os.environ.get('RUNNER_TEMP')) as temporary:
        work = Path(temporary)
        keychain = work / 'signing.keychain-db'
        created = False
        try:
            profiles = load_profiles(work)
            main_profile = profiles[0][1]
            _, pattern = profile_pattern(main_profile)
            bundle = requested_bundle or (pattern if '*' not in pattern else '')
            if not bundle_valid(bundle):
                raise SigningError('Set bundle_id to an identifier covered by the main provisioning profile')
            certificate = work / 'certificate.p12'
            certificate.write_bytes(decode_secret('BUILD_CERTIFICATE_BASE64'))
            keychain_password = secrets.token_urlsafe(32)
            run(['security', 'create-keychain', '-p', keychain_password, str(keychain)])
            created = True
            run(['security', 'set-keychain-settings', '-lut', '3600', str(keychain)])
            run(['security', 'unlock-keychain', '-p', keychain_password, str(keychain)])
            run(['security', 'import', str(certificate), '-k', str(keychain), '-P', os.environ.get('P12_PASSWORD', ''),
                 '-T', '/usr/bin/codesign', '-T', '/usr/bin/security'])
            run(['security', 'set-key-partition-list', '-S', 'apple-tool:,apple:,codesign:', '-s', '-k', keychain_password, str(keychain)])
            listing = run(['security', 'find-identity', '-v', '-p', 'codesigning', str(keychain)]).decode()
            identities = re.findall(r'\b([A-Fa-f0-9]{40})\b', listing)
            if len(identities) != 1:
                raise SigningError('P12 must contain exactly one valid Apple code-signing identity with its private key')
            identity = identities[0].upper()
            team = validate_profile(main_profile, identity)
            tree = work / 'ipa'
            tree.mkdir()
            app = extract_ipa(source, tree)
            old_bundle = plistlib.loads((app / 'Info.plist').read_bytes())['CFBundleIdentifier']
            extensions = sorted(app.rglob('*.appex'), key=lambda p: len(p.parts), reverse=True)
            if strip_extensions:
                for extension in extensions:
                    shutil.rmtree(extension)
            for directory in sorted(app.rglob('_CodeSignature'), key=lambda p: len(p.parts), reverse=True):
                if directory.is_dir():shutil.rmtree(directory)
            for old_profile in app.rglob('embedded.mobileprovision'):
                old_profile.unlink()
            leaves, frameworks, extensions = signing_plan(app)
            selected = [(app, bundle, profiles[0])]
            for extension in extensions:
                old_id = plistlib.loads((extension / 'Info.plist').read_bytes())['CFBundleIdentifier']
                if not isinstance(old_id, str) or not old_id.startswith(old_bundle + '.'):
                    raise SigningError('Retained extension identifier cannot be mapped to the new main bundle')
                new_id = bundle + old_id[len(old_bundle):]
                matches = [(path, p) for path, p in profiles if profile_matches(p, new_id)]
                matches.sort(key=lambda pair: len(profile_pattern(pair[1])[1]), reverse=True)
                if not matches:
                    raise SigningError('A retained extension needs a matching profile; supply extension profiles or strip extensions')
                validate_profile(matches[0][1], identity, team)
                selected.append((extension, new_id, matches[0]))
            entitlement_paths = {}
            for index, (container, new_id, (profile_path, p)) in enumerate(selected):
                info_path = container / 'Info.plist'
                info = plistlib.loads(info_path.read_bytes())
                info_path.write_bytes(plistlib.dumps(change_bundle(info, old_bundle, new_id)))
                shutil.copyfile(profile_path, container / 'embedded.mobileprovision')
                entitlements = work / f'entitlements-{index}.plist'
                entitlements.write_bytes(plistlib.dumps(signing_entitlements(p, new_id)))
                entitlement_paths[container] = entitlements
            command = ['codesign', '--force', '--sign', identity, '--keychain', str(keychain), '--timestamp=none', '--generate-entitlement-der']
            for leaf in leaves:
                leaf.chmod(leaf.stat().st_mode | stat.S_IXUSR)
                run(command + [str(leaf)])
            for framework in frameworks:
                run(command + [str(framework)])
            for container in extensions + [app]:
                executable = container / plistlib.loads((container / 'Info.plist').read_bytes())['CFBundleExecutable']
                executable.chmod(executable.stat().st_mode | 0o111)
                run(command + ['--entitlements', str(entitlement_paths[container]), str(container)])
                run(['codesign', '--verify', '--deep', '--strict', str(container)])
            # Publish only after every signature verifies. ZipFile preserves Unix executable modes.
            with output.open('xb') as handle:
                try:
                    with zipfile.ZipFile(handle, 'w', zipfile.ZIP_DEFLATED, compresslevel=6) as archive:
                        for path in sorted(tree.rglob('*')):
                            archive.write(path, path.relative_to(tree).as_posix())
                except BaseException:
                    handle.close()
                    output.unlink(missing_ok=True)
                    raise
            with zipfile.ZipFile(output) as archive:
                if archive.testzip() is not None:
                    output.unlink()
                    raise SigningError('Signed archive CRC verification failed')
            report = {'status': 'codesign_verified', 'signed_ipa_sha256': file_sha(output),
                      'unsigned_ipa_sha256': file_sha(source), 'bundle_identifier': bundle,
                      'extensions_retained': len(extensions), 'macho_leaves_signed': len(leaves),
                      'frameworks_signed': len(frameworks), 'device_validated': False,
                      'installation_requires_profile_device_authorization': True}
            output.with_suffix('.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
            print('Signed IPA created; codesign and archive verification passed. Device installation is untested.')
        finally:
            if created:
                result = subprocess.run(['security', 'delete-keychain', str(keychain)], capture_output=True)
                if result.returncode:
                    print('Temporary keychain deletion failed; GitHub-hosted runner teardown removes the remaining workspace.', file=sys.stderr)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--bundle-id', default=os.environ.get('SIGN_BUNDLE_ID', ''))
    parser.add_argument('--keep-extensions', action='store_true')
    args = parser.parse_args()
    try:
        sign(args.source, args.output, args.bundle_id, not args.keep_extensions)
    except (RuntimeError, ValueError, OSError, zipfile.BadZipFile) as error:
        raise SystemExit(str(error)) from None
