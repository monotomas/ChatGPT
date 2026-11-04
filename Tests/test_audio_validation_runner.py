# SPDX-License-Identifier: AGPL-3.0-or-later
"""Independent measurement provenance regressions using explicit fake evaluators."""
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class AudioMeasurementProvenanceTests(unittest.TestCase):
    def run_fixture(self, action):
        temporary = tempfile.TemporaryDirectory(prefix='flysnapper-measurement-')
        self.addCleanup(temporary.cleanup)
        root = Path(temporary.name) / 'source'
        tools = root / 'Tools'
        tools.mkdir(parents=True)
        shutil.copyfile(ROOT / 'Tools/audio_validation_run.py', tools / 'audio_validation_run.py')
        (tools / 'audio_validation_verify.py').write_text('pass\n', encoding='utf-8')
        (tools / 'audio_validation.cpp').write_text('// fixture evaluator source\n', encoding='utf-8')
        pitch = root / 'Source/Pitch'
        pitch.mkdir(parents=True)
        (pitch / 'Fixture.cpp').write_text('// original source\n', encoding='utf-8')
        (pitch / 'MpeOutputRouter.cpp').write_text('// separate MIDI core dependency\n', encoding='utf-8')
        (pitch / 'MpeOutputRouter.h').write_text('// output protocol, not audio detection\n', encoding='utf-8')
        fixtures = root / 'Tests/AudioFixtures'
        fixtures.mkdir(parents=True)
        (fixtures / 'fixtures.json').write_text('{}\n', encoding='utf-8')
        # Python's directory entry point is the fake external evaluator. It
        # measures no audio; only wrapper exit/provenance behavior is tested.
        script = '''from pathlib import Path
import sys
root = Path(__file__).resolve().parents[2]
out = Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)
(out / 'audio-summary.txt').write_text('explicit fake measurement fixture\\n')
'''
        if action == 'mutate':
            script += "(root / 'Source/Pitch/Fixture.cpp').write_text('// changed while evaluating\\n')\n"
        elif action == 'fail':
            script += 'raise SystemExit(7)\n'
        (fixtures / '__main__.py').write_text(script, encoding='utf-8')
        output = root / 'results'
        result = subprocess.run([sys.executable, str(tools / 'audio_validation_run.py'),
                                 '--output', str(output), '--executable', sys.executable],
                                stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True,
                                encoding='utf-8', errors='replace')
        return result, json.loads((output / 'measurement-manifest.json').read_text())

    def test_stable_external_evaluator_records_completed_measurement(self):
        result, manifest = self.run_fixture('stable')
        self.assertEqual(result.returncode, 0, result.stdout)
        self.assertFalse(manifest['source_changed_during_measurement'])
        self.assertEqual(manifest['runner_exit_code'], 0)
        self.assertTrue(manifest['measurement_not_acceptance'])
        self.assertIn('Source/Pitch/Fixture.cpp',manifest['source_sha256'])
        self.assertNotIn('Source/Pitch/MpeOutputRouter.cpp',manifest['source_sha256'])
        self.assertNotIn('Source/Pitch/MpeOutputRouter.h',manifest['source_sha256'])

    def test_source_change_invalidates_successful_evaluator_and_preserves_proof(self):
        result, manifest = self.run_fixture('mutate')
        self.assertNotEqual(result.returncode, 0, result.stdout)
        self.assertIn('mixed-source evidence is invalid', result.stdout)
        self.assertTrue(manifest['source_changed_during_measurement'])
        self.assertEqual(manifest['runner_exit_code'], 0)

    def test_failed_evaluator_exit_status_is_preserved(self):
        result, manifest = self.run_fixture('fail')
        self.assertEqual(result.returncode, 7, result.stdout)
        self.assertEqual(manifest['runner_exit_code'], 7)
        self.assertFalse(manifest['source_changed_during_measurement'])


if __name__ == '__main__':
    unittest.main()
