import math
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

import patcher


class MiniplayerTests(unittest.TestCase):
    def test_configuration_bounds_and_native_defaults(self):
        config=patcher.validate_config({})
        self.assertEqual(config['miniplayer_min_dimension_points'],0)
        self.assertEqual(config['miniplayer_overlay_opacity'],1)
        for value in (0,170,192,300.5,480):
            self.assertEqual(patcher.validate_config({'miniplayer_min_dimension_points':value})['miniplayer_min_dimension_points'],value)
        for value in (0,.5,1):
            self.assertEqual(patcher.validate_config({'miniplayer_overlay_opacity':value})['miniplayer_overlay_opacity'],value)
        for key,values in {'miniplayer_min_dimension_points':(-1,1,169,481,True,'300',None,math.inf,math.nan),
                           'miniplayer_overlay_opacity':(-.1,1.1,True,'0.5',None,math.inf,math.nan)}.items():
            for value in values:
                with self.subTest(key=key,value=value),self.assertRaises(patcher.PatchError):patcher.validate_config({key:value})

    @unittest.skipUnless(sys.platform=='darwin','The actual Objective-C miniplayer harness requires macOS Foundation/QuartzCore')
    def test_actual_native_miniplayer_hooks(self):
        with tempfile.TemporaryDirectory() as folder:
            output=Path(folder)/'MiniplayerHarness'
            command=['xcrun','clang','-fobjc-arc','-fblocks','-Wall','-Wextra','-Werror','-Wno-unused-function',
                     '-O2',str(patcher.ROOT/'tests/miniplayer.m'),'-framework','Foundation','-framework','QuartzCore','-framework','CoreGraphics','-o',str(output)]
            result=subprocess.run(command,capture_output=True,text=True,timeout=60)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            result=subprocess.run([str(output)],capture_output=True,text=True,timeout=20)
            self.assertEqual(result.returncode,0,result.stdout+result.stderr)
            self.assertIn('checks passed',result.stdout)
