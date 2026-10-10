from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import patcher


class VoteCountTests(unittest.TestCase):
    @unittest.skipUnless(sys.platform == 'darwin', 'Native vote-count harness requires macOS Foundation')
    def test_response_identity_and_native_size_reservation(self):
        with tempfile.TemporaryDirectory() as folder:
            output = Path(folder) / 'VoteCountsHarness'
            command = ['xcrun', 'clang', '-fobjc-arc', '-fblocks', '-Wall', '-Wextra', '-Werror',
                       str(patcher.ROOT / 'tests/vote_counts.m'), '-framework', 'Foundation', '-o', str(output)]
            result = subprocess.run(command, capture_output=True, text=True, timeout=60)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            result = subprocess.run([str(output)], capture_output=True, text=True, timeout=20)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            self.assertIn('checks passed', result.stdout)
