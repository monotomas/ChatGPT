import unittest
from score import score
def case(truth):return dict(name='probe',family='manual-transcript',mode='poly',truth=truth)
def event(on,note,sample,feature_time=-1):return dict(on=on,note=note,sample=sample,feature_time=feature_time)
class TranscriptTests(unittest.TestCase):
    def test_actual_emission_not_feature_time(self):
        r=score(case([dict(note=60,on_sample=4800,off_sample=14400)]),[event(1,60,7200,.1),event(0,60,14400)],16000)
        self.assertEqual(r['latencies_ms'],[50]);self.assertEqual(r['matched'],1)
    def test_recovery_retains_wrong_first_and_octave(self):
        r=score(case([dict(note=60,on_sample=4800,off_sample=19200)]),[event(1,48,5000),event(0,48,6200),event(1,60,6400),event(0,60,19200)],24000)
        self.assertEqual((r['matched'],r['false_note_ons'],r['octave_errors'],r['wrong_before_correct']),(1,1,1,1))
    def test_same_note_rearticulation_is_not_one_long_note(self):
        r=score(case([dict(note=60,on_sample=0,off_sample=19200)]),[event(1,60,100),event(0,60,10000),event(1,60,10000),event(0,60,19200)],24000)
        self.assertEqual((r['false_note_ons'],r['same_pitch_retriggers'],r['early_offs']),(1,1,1))
    def test_real_lower_octave_voice_is_not_false(self):
        r=score(case([dict(note=48,on_sample=0,off_sample=19200),dict(note=60,on_sample=0,off_sample=19200)]),[event(1,48,100),event(1,60,100),event(0,48,19200),event(0,60,19200)],24000)
        self.assertEqual((r['matched'],r['false_note_ons'],r['octave_errors']),(2,0,0))
    def test_pre_on_held_pitch_cannot_be_backdated_into_reference(self):
        r=score(case([dict(note=60,on_sample=4800,off_sample=19200)]),[event(1,60,4000),event(0,60,19200)],24000)
        self.assertEqual((r['matched'],r['missed'],r['false_note_ons']),(0,1,1))
if __name__=='__main__':unittest.main()
