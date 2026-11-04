#!/usr/bin/env python3
"""Guard independent acceptance against musically misleading event traces.
SPDX-License-Identifier: AGPL-3.0-or-later
These hand-authored event transcripts do not exercise or tune the DSP.
"""
import importlib.util,json,unittest
from pathlib import Path
HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('readiness_score',HERE/'score.py')
score=importlib.util.module_from_spec(spec);spec.loader.exec_module(score)
POLICY=json.loads((HERE/'policy.json').read_text())

def event(at,kind,note,voice=0,pitch=None):
    return dict(sample=str(at),type=kind,note=str(note),voice_id=str(voice),pitch='' if pitch is None else str(pitch))
def note(anchor,on,off,identifier='n0',technique='plucked_note_isolated'):
    return dict(anchor=anchor,on_sample=on,off_sample=off,id=identifier,technique=technique)
def evaluate(notes,events,curves=None,family='control',mode='mono'):
    case=dict(name='hand-labelled-control',record='control',family=family,mode=mode,snr_db=None,bend_enabled=True,reference_notes=notes)
    metric=dict(rate='48000',block='64',frames='24000',active_at_end='0',overflow='0',native_poly_bend='1')
    if curves is None:curves={('control',n['id']):([n['on_sample'],n['off_sample']-1],[n['anchor'],n['anchor']]) for n in notes}
    return score.evaluate(case,metric,events,curves,POLICY)

class IndependentScoringTests(unittest.TestCase):
    def test_one_sustained_bend_cannot_count_as_two_hammered_notes(self):
        refs=[note(60,0,4800,technique='hammer_on'),note(62,4800,9600,'n1','hammer_on')]
        result=evaluate(refs,[event(0,'N',60),event(4800,'P',60,pitch=62),event(9600,'F',60)])
        self.assertEqual((result['matched'],result['missed'],result['discrete_misses']),(1,1,1))
        self.assertIn('missed-discrete-articulation',result['failures'])

    def test_initial_octave_error_is_retained_after_correct_note_arrives(self):
        result=evaluate([note(60,0,9600)],[event(0,'N',48),event(480,'F',48),event(480,'N',60),event(9600,'F',60)])
        self.assertEqual((result['matched'],result['extra_note_ons'],result['octave_errors'],result['wrong_before_correct']),(1,1,1,1))
        self.assertFalse(result['passed'])

    def test_future_pitch_wheel_cannot_repair_preceding_wrong_pitch(self):
        refs=[note(60,0,4800)]
        curves={('control','n0'):([0,1200,2400,3600],[61,61,61,61])}
        result=evaluate(refs,[event(0,'N',60),event(2400,'P',60,pitch=61),event(4800,'F',60)],curves)
        self.assertEqual(result['pitch_error_cents']['maximum'],100)
        self.assertIn('pitch-max:n0',result['failures'])

    def test_legitimate_lower_octave_voice_is_not_a_false_octave(self):
        refs=[note(45,0,9600,'low'),note(57,0,9600,'high')]
        result=evaluate(refs,[event(0,'N',45,2),event(0,'N',57,5),event(9600,'F',45,2),event(9600,'F',57,5)],mode='poly')
        self.assertEqual((result['matched'],result['octave_errors'],result['extra_note_ons']),(2,0,0))
        self.assertTrue(result['passed'])

    def test_actual_extra_lower_octave_is_counted_with_legitimate_low_voice(self):
        refs=[note(45,0,9600,'low'),note(57,0,9600,'high')]
        result=evaluate(refs,[event(0,'N',45,2),event(0,'N',57,5),event(1000,'N',33,0),event(4000,'F',33,0),event(9600,'F',45,2),event(9600,'F',57,5)],mode='poly')
        self.assertEqual((result['matched'],result['octave_errors'],result['extra_note_ons']),(2,1,1))

    def test_pre_on_expression_and_real_release_order_are_preserved(self):
        events=[event(0,'P',60,pitch=60.1),event(0,'N',60),event(9600,'F',60)]
        curves={('control','n0'):([0,4800,9599],[60.1,60.1,60.1])}
        result=evaluate([note(60,0,9600)],events,curves)
        self.assertTrue(result['passed']);self.assertEqual(result['pitch_error_cents']['maximum'],0)

if __name__=='__main__':unittest.main()
