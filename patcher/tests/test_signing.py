import base64
import copy
from datetime import datetime, timedelta
import hashlib
import io
import os
from pathlib import Path
import plistlib
import tempfile
import unittest
import zipfile
from unittest.mock import patch

import cloud_download
import cloud_sign


def provision(pattern='io.example.youtube', prefix='LEGACYPREFIX', team='NEWTEAM'):
    return {'ApplicationIdentifierPrefix': [prefix], 'TeamIdentifier': [team], 'Platform': ['iOS'],
            'ExpirationDate': datetime.now() + timedelta(days=1), 'ProvisionedDevices': ['test-device'],
            'DeveloperCertificates': [b'test-certificate'],
            'Entitlements': {'application-identifier': prefix + '.' + pattern,
                             'com.apple.developer.team-identifier': team,
                             'keychain-access-groups': [prefix + '.*'], 'get-task-allow': True}}


class ProfileTests(unittest.TestCase):
    identity = hashlib.sha1(b'test-certificate').hexdigest()

    def test_legacy_prefix_is_separate_from_team(self):
        p = provision()
        self.assertEqual(cloud_sign.validate_profile(p, self.identity), 'NEWTEAM')
        entitlements = cloud_sign.signing_entitlements(p, 'io.example.youtube')
        self.assertEqual(entitlements['application-identifier'], 'LEGACYPREFIX.io.example.youtube')
        self.assertEqual(entitlements['com.apple.developer.team-identifier'], 'NEWTEAM')
        self.assertEqual(entitlements['keychain-access-groups'], ['LEGACYPREFIX.io.example.youtube'])
        self.assertEqual(p['Entitlements']['keychain-access-groups'], ['LEGACYPREFIX.*'])

    def test_expired_wrong_certificate_and_wrong_extension_team_refused(self):
        expired = provision()
        expired['ExpirationDate'] = datetime.now() - timedelta(seconds=1)
        for p, cert, team in [(expired, self.identity, None),
                              (provision(), '0' * 40, None),
                              (provision(), self.identity, 'OTHERTEAM')]:
            with self.subTest(profile=p, identity=cert), self.assertRaises(cloud_sign.SigningError):
                cloud_sign.validate_profile(p, cert, team)

    def test_app_store_profile_refused_for_direct_install(self):
        p = provision()
        del p['ProvisionedDevices']
        with self.assertRaises(cloud_sign.SigningError):cloud_sign.validate_profile(p, self.identity)
        p['ProvisionsAllDevices'] = True
        self.assertEqual(cloud_sign.validate_profile(p, self.identity), 'NEWTEAM')

    def test_bundle_match_requires_exact_or_dotted_prefix(self):
        p = provision('io.example.*')
        self.assertTrue(cloud_sign.profile_matches(p, 'io.example.youtube'))
        self.assertFalse(cloud_sign.profile_matches(p, 'io.examples.youtube'))
        self.assertFalse(cloud_sign.profile_matches(p, 'io.example'))
        self.assertFalse(cloud_sign.profile_matches(provision(), 'io.example.youtube.extension'))
        for bundle in ('../../app', 'io.example.*', 'io.example..youtube', 'io.example/one'):
            self.assertFalse(cloud_sign.profile_matches(provision('*'), bundle))

    def test_unresolved_capability_wildcards_fail_closed(self):
        p = provision()
        p['Entitlements']['com.apple.developer.icloud-container-identifiers'] = ['iCloud.*']
        with self.assertRaises(cloud_sign.SigningError):
            cloud_sign.signing_entitlements(p, 'io.example.youtube')
        with self.assertRaises(cloud_sign.SigningError):
            cloud_sign.signing_entitlements(provision(), 'io.other.youtube')

    def test_malformed_profile_and_secret_refused(self):
        other_platform = provision()
        other_platform['Platform'] = ['OSX']
        with self.assertRaises(cloud_sign.SigningError):
            cloud_sign.validate_profile(other_platform, self.identity)
        for identifier in ('OTHER.io.example.youtube', 'LEGACYPREFIX.io.ex*ample.youtube', 'LEGACYPREFIX.io.example.*.bad'):
            p = provision()
            p['Entitlements']['application-identifier'] = identifier
            with self.subTest(identifier=identifier), self.assertRaises(cloud_sign.SigningError):
                cloud_sign.profile_pattern(p)
        for value in ('', 'invalid!'):
            with patch.dict(os.environ, {'CERT_TEST': value}), self.assertRaises(cloud_sign.SigningError):
                cloud_sign.decode_secret('CERT_TEST')
        with patch.dict(os.environ, {'CERT_TEST': base64.b64encode(b'abc').decode()}):
            self.assertEqual(cloud_sign.decode_secret('CERT_TEST'), b'abc')
            with self.assertRaises(cloud_sign.SigningError):cloud_sign.decode_secret('CERT_TEST', 2)

    def test_secret_command_arguments_are_absent_from_failure(self):
        result = type('Result', (), {'returncode': 1, 'stdout': b'password', 'stderr': b'password'})()
        with patch('cloud_sign.subprocess.run', return_value=result), self.assertRaises(cloud_sign.SigningError) as error:
            cloud_sign.run(['security', 'import', '-P', 'super-secret-password'])
        self.assertNotIn('super-secret-password', str(error.exception))
        self.assertNotIn('password', str(error.exception))

    def test_signing_plan_excludes_bundle_executables_and_orders_nested_code(self):
        with tempfile.TemporaryDirectory() as temporary:
            app = Path(temporary) / 'YouTube.app'
            framework = app / 'Frameworks/Test.framework'
            extension = app / 'PlugIns/Share.appex'
            nested = extension / 'Frameworks/Inner.framework'
            for bundle, executable in ((app, 'YouTube'), (framework, 'Test'), (extension, 'Share'), (nested, 'Inner')):
                bundle.mkdir(parents=True, exist_ok=True)
                (bundle / 'Info.plist').write_bytes(plistlib.dumps({'CFBundleExecutable': executable}))
                (bundle / executable).write_bytes(b'\xcf\xfa\xed\xfe' + b'fixture')
            library = app / 'Frameworks/RVPort.dylib'
            library.write_bytes(b'\xcf\xfa\xed\xfe' + b'fixture')
            (app / 'resource.txt').write_text('not code')
            leaves, frameworks, extensions = cloud_sign.signing_plan(app)
            self.assertEqual(leaves, [library])
            self.assertEqual(frameworks, [nested, framework])
            self.assertEqual(extensions, [extension])
            (app / 'Info.plist').write_bytes(plistlib.dumps({'CFBundleExecutable': '../outside'}))
            with self.assertRaises(cloud_sign.SigningError):cloud_sign.signing_plan(app)

    def test_profile_zip_and_ipa_extraction_use_validated_zip_entries(self):
        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            source = work / 'input.ipa'
            with zipfile.ZipFile(source, 'w') as archive:
                archive.writestr('Payload/YouTube.app/Info.plist', plistlib.dumps({'CFBundleExecutable': 'YouTube'}))
                info = zipfile.ZipInfo('Payload/YouTube.app/YouTube')
                info.external_attr = 0o100755 << 16
                archive.writestr(info, b'\xcf\xfa\xed\xfe' + b'fixture')
            app = cloud_sign.extract_ipa(source, work / 'unpacked')
            self.assertEqual((app / 'YouTube').read_bytes(), b'\xcf\xfa\xed\xfe' + b'fixture')
            cloud_sign.signing_plan(app)
            profiles = io.BytesIO()
            with zipfile.ZipFile(profiles, 'w') as archive:
                archive.writestr('share.mobileprovision', b'signed-profile-fixture')
            environment = {'BUILD_PROVISION_PROFILE_BASE64': base64.b64encode(b'main-profile').decode(),
                           'EXTENSION_PROFILES_ZIP_BASE64': base64.b64encode(profiles.getvalue()).decode()}
            with patch.dict(os.environ, environment), patch('cloud_sign.run', return_value=plistlib.dumps(provision())):
                loaded = cloud_sign.load_profiles(work)
            self.assertEqual(len(loaded), 2)
            self.assertEqual(loaded[1][0].read_bytes(), b'signed-profile-fixture')


class DownloadTests(unittest.TestCase):
    def test_non_https_credential_and_invalid_urls_refused(self):
        for url in ('http://example.test/app.ipa', 'https://user:pass@example.test/',
                    'https://example.test/#fragment', 'https://example.test/white space',
                    'https://example.test:bad/', 'file:///tmp/app.ipa'):
            with self.subTest(url=url), self.assertRaises(cloud_download.DownloadError):
                cloud_download.validate_url(url)
        self.assertEqual(cloud_download.validate_url('https://example.test/app?token=secret'), 'https://example.test/app?token=secret')

    def response(self, data):
        response = io.BytesIO(data)
        response.status = 200
        response.headers = {'Content-Length': str(len(data))}
        return response

    def test_download_requires_hash_and_preserves_existing_output(self):
        data = b'IPA fixture'
        digest = hashlib.sha256(data).hexdigest()
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / 'source.ipa'
            opener = type('Opener', (), {'open': lambda _self, *args, **kwargs: self.response(data)})()
            with patch('cloud_download.build_opener', return_value=opener):
                with self.assertRaises(cloud_download.DownloadError):
                    cloud_download.download('https://example.test/', output, '')
                self.assertFalse(output.exists())
                self.assertEqual(cloud_download.download('https://example.test/', output, digest), digest)
                self.assertEqual(output.read_bytes(), data)
                with self.assertRaises(cloud_download.DownloadError):
                    cloud_download.download('https://example.test/', output, digest)
                self.assertEqual(output.read_bytes(), data)

    def test_mismatch_and_oversize_remove_only_owned_download(self):
        data = b'fixture'
        opener = type('Opener', (), {'open': lambda _self, *args, **kwargs: self.response(data)})()
        with tempfile.TemporaryDirectory() as temporary, patch('cloud_download.build_opener', return_value=opener):
            output = Path(temporary) / 'source.ipa'
            for digest, limit in [('0' * 64, 100), (hashlib.sha256(data).hexdigest(), 1)]:
                with self.assertRaises(cloud_download.DownloadError):
                    cloud_download.download('https://example.test/', output, digest, limit)
                self.assertFalse(output.exists())


if __name__ == '__main__':
    unittest.main()
