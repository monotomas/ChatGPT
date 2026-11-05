# SPDX-License-Identifier: AGPL-3.0-or-later
"""Transcript guards for the explicitly revised November5 measurement scope."""
import copy
import importlib.util
import json
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('nov5_score',ROOT/'Tools/guitar_evaluation_score.py')
score=importlib.util.module_from_spec(spec);spec.loader.exec_module(score)

class GuitarEvaluationScoreTests(unittest.TestCase):
    def setUp(self):
        self.policy=json.loads(score.POLICY.read_text())
        self.case=dict(name='two',record='two',mode='poly',family='static-chord',bend_enabled=False,snr_db=None,reference_notes=[dict(id='a',string=0,anchor=57,on_sample=0,off_sample=48000,technique='pluck'),dict(id='b',string=1,anchor=64,on_sample=0,off_sample=48000,technique='pluck')])
        self.curves={('two','a'):([0,48000],[57.,57.]),('two','b'):([0,48000],[64.,64.])}
        self.metric=dict(rate='48000',block='64',frames='60000',native_poly_bend='0',active_at_end='0',overflow='0',deadline_misses='0',cpu_p99_us='10',cpu_max_us='12')
    def event(self,kind,note,sample,voice=-1,pitch=''):
        return dict(type=kind,note=str(note),sample=str(sample),voice_id=str(voice),pitch=str(pitch))
    def result(self,events):return score.evaluate(self.case,self.metric,events,self.curves,self.policy)
    def test_static_poly_note_identity_preserves_overlapping_chord(self):
        events=[self.event('N',57,2400),self.event('N',64,2400),self.event('F',57,48000),self.event('F',64,48000)]
        before=copy.deepcopy(events);row=self.result(events)
        self.assertTrue(row['passed']);self.assertEqual(row['matched'],2);self.assertEqual(row['references'][0]['latency_ms'],50)
        self.assertEqual(events,before) # Preserve the original raw identity.
    def test_late_correct_note_is_reported_without_absolute_latency_gate(self):
        events=[self.event('N',57,24000),self.event('N',64,24000),self.event('F',57,48000),self.event('F',64,48000)]
        row=self.result(events);self.assertTrue(row['passed']);self.assertEqual(row['latency_ms']['maximum'],500)
        summary=score.summarize([row],[]);self.assertFalse(summary['absolute_latency_gate']);self.assertEqual(summary['latency_by_pitch'][0]['decision_ms']['maximum'],500)
    def test_missing_note_and_extra_octave_remain_failures(self):
        row=self.result([self.event('N',57,2400),self.event('N',76,2400),self.event('F',57,48000),self.event('F',76,48000)])
        self.assertFalse(row['passed']);self.assertEqual(row['missed'],1);self.assertEqual(row['extra_note_ons'],1);self.assertEqual(row['octave_errors'],1)
        self.assertFalse(row['references'][1]['matched']);self.assertIsNone(row['references'][1]['latency_ms'])
    def test_early_release_and_noise_only_notes_remain_failures(self):
        row=self.result([self.event('N',57,2400),self.event('N',64,2400),self.event('F',57,24000),self.event('F',64,48000)])
        self.assertFalse(row['passed']);self.assertEqual(row['early_offs'],1)
        self.case['reference_notes']=[]
        row=self.result([self.event('N',57,2400),self.event('F',57,48000)])
        self.assertFalse(row['passed']);self.assertEqual(row['noise_note_ons'],1)
    def test_poly_bend_is_explicitly_outside_scope_and_mono_is_retained(self):
        self.case['family']='continuous-bend';self.assertFalse(score.in_scope(self.case,self.policy))
        self.case['mode']='mono';self.assertTrue(score.in_scope(self.case,self.policy))
        self.case['reference_notes']=self.case['reference_notes'][:1];self.case['bend_enabled']=True
        self.curves[('two','a')]=([0,24000,48000],[57.,58.,59.])
        row=self.result([self.event('N',57,2400,0),self.event('F',57,48000,0)])
        self.assertFalse(row['passed']);self.assertIn('missing-continuous-expression:a',row['failures'])
    def test_static_poly_pitch_wheel_is_not_silently_accepted(self):
        row=self.result([self.event('N',57,2400),self.event('N',64,2400),self.event('P',57,3000,pitch=57.2),self.event('F',57,48000),self.event('F',64,48000)])
        self.assertFalse(row['passed']);self.assertIn('unexpected-poly-continuous-expression',row['failures'])

if __name__=='__main__':unittest.main()
