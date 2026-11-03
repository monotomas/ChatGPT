# SPDX-License-Identifier: AGPL-3.0-or-later
"""Check the retained audio evidence used by the guitar behaviour regressions."""
from pathlib import Path
import subprocess
import sys
import unittest


ROOT = Path(__file__).resolve().parents[1]


class GuitarControlIntegrity(unittest.TestCase):
    def test_retained_waveforms_annotations_and_original_baseline(self):
        result = subprocess.run(
            [sys.executable, str(ROOT / "Tests/AudioFixtures/guitar_controls/verify.py")],
            cwd=ROOT, capture_output=True, text=True, check=False,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
