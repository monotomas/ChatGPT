#!/usr/bin/env python3
"""Independent transcript guards against optimistic oracle tracking scores.
SPDX-License-Identifier: AGPL-3.0-or-later
"""
from pathlib import Path
import importlib.util,json,unittest
HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('poly_tracking_score',HERE/'score.py');score=importlib.util.module_from_spec(spec);spec.loader.exec_module(score)
POLICY=json.loads((HERE/'policy.json').read_text())
def ref(identifier,pitch):return dict(sample='2879',voice_id=str(identifier),physical_string=str(1-identifier),pitch=str(pitch),scored='1')
def voice(identifier,pitch):return dict(sample='2879',id=str(identifier),pitch=str(pitch),phase_observable='1')
def evaluate(truth,voices):
    return score.score_case('transcript',truth,[dict(sample='2879',output_count=str(len(voices)),wall_us='1',cpu_us='1')],voices,POLICY)[0]
class TranscriptTests(unittest.TestCase):
    def test_identity_swap_does_not_pass_by_nearest_pitch(self):
        result=evaluate([ref(0,60),ref(1,64)],[voice(0,64),voice(1,60)])
        self.assertEqual(result['possible_identity_swap_frames'],2)
        self.assertEqual(result['error_cents']['maximum'],400)
        self.assertFalse(result['passed'])
    def test_missing_bad_voice_cannot_disappear_into_zero_error(self):
        result=evaluate([ref(0,60),ref(1,64)],[voice(0,60)])
        self.assertEqual(result['error_cents']['maximum'],0)
        self.assertEqual(result['missing_source_frames'],1)
        self.assertIn('missing-original-identity',result['failures'])
        self.assertFalse(result['passed'])
    def test_delayed_pitch_is_scored_against_current_truth(self):
        result=evaluate([ref(0,61)],[voice(0,60.5)])
        self.assertEqual(result['error_cents']['p95'],50)
        self.assertIn('pitch-p95',result['failures'])
    def test_duplicate_original_identity_is_retained(self):
        result=evaluate([ref(0,60),ref(1,64)],[voice(0,60),voice(0,60),voice(1,64)])
        self.assertEqual(result['duplicate_source_frames'],1)
        self.assertIn('duplicate-id',result['failures'])
if __name__=='__main__':unittest.main()
