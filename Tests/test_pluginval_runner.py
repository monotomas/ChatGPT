# SPDX-License-Identifier: AGPL-3.0-or-later
"""Validator result protocol regression tests, not claims of plugin validation."""
import importlib.util
from pathlib import Path
import unittest
script=Path(__file__).resolve().parents[1]/'Tools/run_pluginval.py'
spec=importlib.util.spec_from_file_location('pluginval_runner_test',script)
runner=importlib.util.module_from_spec(spec);spec.loader.exec_module(runner)

class PluginvalProtocolTests(unittest.TestCase):
    def setUp(self):
        # Actual official 1.0.4 console grammar: plural 'Starting tests in'.
        self.console='Starting tests in: pluginval / Open plugin (cold)...\nCompleted tests in pluginval / Open plugin (cold)\nStarting tests in: pluginval / Audio processing...\nCompleted tests in pluginval / Audio processing\nINFO: Skipping vst3 validator as validator path hasn\'t been set\nSUCCESS\n'
    def test_real_official_plural_protocol_counts_executed_groups(self):
        parsed=runner.parse_output(self.console)
        self.assertEqual(parsed['test_starts'],2)
        self.assertTrue(parsed['success_marker'])
        self.assertEqual(len(parsed['reported_skips']),1)
        self.assertTrue(runner.completed_output(self.console,0,False,True))
    def test_zero_group_run_cannot_pass_from_success_word_only(self):
        self.assertFalse(runner.completed_output('SUCCESS\n',0,False,True))
    def test_failure_timeout_or_modified_bundle_cannot_pass(self):
        for code,timeout,unchanged in [(1,False,True),(0,True,True),(0,False,False)]:
            with self.subTest(code=code,timeout=timeout,unchanged=unchanged):
                self.assertFalse(runner.completed_output(self.console,code,timeout,unchanged))
    def test_incomplete_or_embedded_success_text_does_not_pass(self):
        text=self.console.replace('SUCCESS','An early success word is not the completion marker')
        self.assertFalse(runner.completed_output(text,0,False,True))
        self.assertFalse(runner.completed_output(self.console.replace('SUCCESS','FAILURE'),1,False,True))

if __name__=='__main__':unittest.main()
