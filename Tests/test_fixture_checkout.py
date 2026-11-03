# SPDX-License-Identifier: AGPL-3.0-or-later
"""Pinned upstream bytes must survive an actual Git Windows-style checkout."""
import hashlib
from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class FixtureCheckoutTests(unittest.TestCase):
    def test_autocrlf_checkout_preserves_pinned_sources_and_measurement_proof(self):
        with tempfile.TemporaryDirectory(prefix='flysnapper-checkout-') as temp:
            source = Path(temp) / 'source'
            source.mkdir()
            fixture_names = (
                'Tests/AudioFixtures/tonejs/README.md',
                'Tests/AudioFixtures/holdout/tonejs/README.md',
                'docs/validation/audio-iteration14-holdout-full/measurement-manifest.json',
            )
            for name in fixture_names:
                path = source / name
                path.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(ROOT / name, path)
            shutil.copyfile(ROOT / '.gitattributes', source / '.gitattributes')
            (source / 'conversion-control.txt').write_bytes(b'unprotected text\n')

            def git(folder, *args):
                result = subprocess.run(['git', '-C', str(folder), *args],
                                        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
                self.assertEqual(result.returncode, 0, result.stdout.decode('utf-8', errors='replace'))

            git(source, 'init', '--quiet')
            git(source, 'config', 'core.autocrlf', 'false')
            git(source, 'add', '.')
            git(source, '-c', 'user.name=Fixture', '-c', 'user.email=fixture@example.invalid',
                'commit', '--quiet', '-m', 'fixture byte preservation')
            checkout = Path(temp) / 'Windows checkout'
            git(source, 'clone', '--quiet', '--no-checkout', str(source), str(checkout))
            git(checkout, 'config', 'core.autocrlf', 'true')
            git(checkout, 'checkout', '--quiet', '--force', 'HEAD')

            self.assertEqual((checkout / 'conversion-control.txt').read_bytes(), b'unprotected text\r\n')
            for name in fixture_names:
                with self.subTest(name=name):
                    self.assertEqual(hashlib.sha256((checkout / name).read_bytes()).hexdigest(),
                                     hashlib.sha256((ROOT / name).read_bytes()).hexdigest())


if __name__ == '__main__':
    unittest.main()
