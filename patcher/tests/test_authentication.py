import json
from pathlib import Path
import plistlib
import subprocess
import sys
import tempfile
import unittest

import patcher


class AuthenticationTests(unittest.TestCase):
    def test_auth_preset_is_explicit_and_independently_switchable(self):
        config=json.loads((patcher.ROOT/'configs/sideload-auth.json').read_text(encoding='utf-8'))
        self.assertTrue(patcher.validate_config(config)['sideload_auth_identity'])
        self.assertTrue(patcher.validate_config(config)['sideload_auth_keychain'])
        self.assertFalse(patcher.DEFAULTS['sideload_auth_identity'])
        self.assertFalse(patcher.DEFAULTS['sideload_auth_keychain'])
        self.assertFalse(patcher.validate_config(dict(config,sideload_auth_identity=False))['sideload_auth_identity'])
        for key in ('sideload_auth_identity','sideload_auth_keychain'):
            for value in (1,'true',None):
                with self.subTest(key=key,value=value),self.assertRaises(patcher.PatchError):
                    patcher.validate_config({key:value})

    @unittest.skipUnless(sys.platform=='darwin','The actual Objective-C auth harness requires macOS Foundation/Security')
    def test_actual_native_authentication_adapter(self):
        with tempfile.TemporaryDirectory() as folder:
            bundle=Path(folder)/'AuthHarness.app/Contents'
            output=bundle/'MacOS/AuthHarness';output.parent.mkdir(parents=True)
            info={'CFBundleIdentifier':'io.fixture.sideload','CFBundleExecutable':'AuthHarness','CFBundlePackageType':'APPL',
                  'CFBundleURLTypes':[{'CFBundleURLSchemes':['com.google.sso.755541669657-kbosfavg7pk7sr3849c3tf657hpi5jpd']}]}
            (bundle/'Info.plist').write_bytes(plistlib.dumps(info))
            args=['xcrun','clang','-fobjc-arc','-fblocks','-Wall','-Wextra','-Werror','-Wno-unused-function',
                  '-O2',str(patcher.ROOT/'tests/authentication.m'),'-framework','Foundation','-framework','Security','-o',str(output)]
            result=subprocess.run(args,capture_output=True,text=True,timeout=60)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            result=subprocess.run([str(output)],capture_output=True,text=True,timeout=20)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            self.assertIn('checks passed',result.stdout)
