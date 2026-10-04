from pathlib import Path
import json
import subprocess
import sys
import tempfile
import unittest

import patcher
import settings_catalog


class SettingsTests(unittest.TestCase):
    def test_catalog_covers_all_runtime_preferences_and_presets(self):
        catalog=settings_catalog.catalog()
        keys=[key for group in catalog['groups'] for key in group['keys']]
        self.assertEqual(set(keys),set(patcher.DEFAULTS)-{'schema','app_name'})
        self.assertEqual(len(keys),len(set(keys)))
        self.assertTrue(set(patcher.FEATURES)<=set(catalog['settings']))
        for key,rule in catalog['settings'].items():
            self.assertEqual(rule['default'],patcher.DEFAULTS[key])
            self.assertTrue(rule['title'])
        self.assertEqual(set(catalog['settings']['sponsor_behaviors']['map_keys']),set(catalog['settings']['sponsor_categories']['allowed']))
        self.assertEqual(catalog['settings']['wifi_quality']['choices'][0],-1)
        self.assertEqual(catalog['settings']['miniplayer_min_dimension_points']['min'],170)

    @unittest.skipUnless(sys.platform=='darwin','The actual native settings model/menu bridge harness requires macOS Foundation')
    def test_actual_native_settings_model_and_bridge(self):
        with tempfile.TemporaryDirectory() as directory:
            output=Path(directory)/'SettingsHarness'
            command=['xcrun','clang','-fobjc-arc','-fblocks','-Wall','-Wextra','-Werror','-Wno-unused-function','-O2',
                     str(patcher.ROOT/'tests/settings.m'),'-framework','Foundation','-framework','CoreFoundation','-o',str(output)]
            result=subprocess.run(command,capture_output=True,text=True,timeout=60)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            result=subprocess.run([str(output)],capture_output=True,text=True,timeout=20)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            self.assertIn('checks passed',result.stdout)
            exported=next(line.removeprefix('Settings export: ') for line in result.stdout.splitlines() if line.startswith('Settings export: '))
            config=json.loads(exported)
            self.assertEqual(patcher.validate_config(config),config)
            self.assertEqual(config['wifi_quality'],144)
            self.assertIs(type(config['screen_width_points']),int)
