# SPDX-License-Identifier: AGPL-3.0-or-later
"""Adversarial MIDI-only proof protocol tests, not native plugin acceptance."""
from pathlib import Path
import copy,hashlib,importlib.util,json,tempfile,unittest
from Tools import midi_only_evidence as e
ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('midi_only_fixture_for_evidence',Path(__file__).with_name('midi_only_fixture.py'));fixture=importlib.util.module_from_spec(spec);spec.loader.exec_module(fixture)

class MidiOnlyEvidenceTests(unittest.TestCase):
    def setUp(self):
        t=tempfile.TemporaryDirectory();self.addCleanup(t.cleanup);self.f=fixture.make(Path(t.name),ROOT)
    def verify(self):return e.verify(self.f.root,self.f.dist,self.f.manifest)
    def test_exact_fixture_bindings_pass_without_claiming_native_execution(self):
        self.assertEqual(set(self.verify()),{e.MANIFEST}|{n for s in e.SPECS for n in s[2:4]})
        self.assertEqual(e.VERSION,'0.4.2')
        self.assertEqual(e.SCOPE['scales'],38)
        self.assertEqual(e.SCOPE['chord_types'],52)
        self.assertEqual(e.SCOPE['host_device_role'],'instrument')
        self.assertIs(e.SCOPE['silent_audio_output'],True)
        self.assertIn('silent_stereo_audio_output',e.FEATURES)
        self.assertNotIn('zero_audio_buses',e.FEATURES)
        info=json.loads((self.f.bundle/'Contents/Resources/moduleinfo.json').read_text())
        self.assertEqual(info['Classes'][0]['Sub Categories'],['Instrument'])
        self.assertEqual(info['Classes'][0]['Version'],'0.4.2')
    def test_source_mutation_and_extra_source_are_rejected(self):
        p=self.f.root/'Source/Core/MidiEngine.cpp';p.write_text('changed source')
        with self.assertRaisesRegex(ValueError,'source differs'):self.verify()
    def test_changed_native_test_or_missing_test_cannot_ship(self):
        proof=json.loads((self.f.dist/e.DIRECTORY/e.MANIFEST).read_text());run=next(iter(proof['executions'].values()));p=self.f.root/run['binary'];p.write_bytes(p.read_bytes()+b'changed')
        with self.assertRaisesRegex(ValueError,'executable changed'):self.verify()
    def test_changed_report_or_events_require_exact_binding(self):
        path=self.f.dist/e.DIRECTORY/e.SPECS[0][3];path.write_bytes(path.read_bytes()+b'changed')
        with self.assertRaises(ValueError):self.verify()
    def test_rebound_false_host_claim_and_missing_feature_are_rejected(self):
        for key,value in [('ableton_verified',True),('verified_features',[]),('scale_count',19),('scale_count',31),('chord_type_count',50),('audio_input_channels',1),('audio_output_channels',0),('audio_output_channels',1),('audio_output_silent',False),('audio_output_silent',1),('host_device_role','effect')]:
            with self.subTest(key=key):
                p=self.f.dist/e.DIRECTORY/e.SPECS[0][2];before=p.read_bytes();r=json.loads(before);r[key]=value;p.write_text(json.dumps(r))
                proof=json.loads((self.f.dist/e.DIRECTORY/e.MANIFEST).read_text());proof['executions'][e.SPECS[0][0]]['report_sha256']=e.sha(p.read_bytes());fixture.rebind_proof(self.f,proof)
                with self.assertRaises(ValueError):self.verify()
                p.write_bytes(before);proof['executions'][e.SPECS[0][0]]['report_sha256']=e.sha(before);fixture.rebind_proof(self.f,proof)
    def test_silent_instrument_contract_required_in_each_native_report(self):
        for spec in e.SPECS:
            for key in ('audio_output_silent','host_device_role'):
                with self.subTest(report=spec[0],missing=key):
                    p=self.f.dist/e.DIRECTORY/spec[2];before=p.read_bytes();r=json.loads(before);del r[key];p.write_text(json.dumps(r))
                    proof=json.loads((self.f.dist/e.DIRECTORY/e.MANIFEST).read_text());proof['executions'][spec[0]]['report_sha256']=e.sha(p.read_bytes());fixture.rebind_proof(self.f,proof)
                    with self.assertRaises(ValueError):self.verify()
                    p.write_bytes(before);proof['executions'][spec[0]]['report_sha256']=e.sha(before);fixture.rebind_proof(self.f,proof)
            with self.subTest(report=spec[0],missing='silent_stereo_audio_output'):
                p=self.f.dist/e.DIRECTORY/spec[2];before=p.read_bytes();r=json.loads(before);r['verified_features'].remove('silent_stereo_audio_output');p.write_text(json.dumps(r))
                proof=json.loads((self.f.dist/e.DIRECTORY/e.MANIFEST).read_text());proof['executions'][spec[0]]['report_sha256']=e.sha(p.read_bytes());fixture.rebind_proof(self.f,proof)
                with self.assertRaises(ValueError):self.verify()
                p.write_bytes(before);proof['executions'][spec[0]]['report_sha256']=e.sha(before);fixture.rebind_proof(self.f,proof)
    def test_duplicate_cases_and_noncausal_event_times_rejected_even_with_rebound_hash(self):
        p=self.f.dist/e.DIRECTORY/e.SPECS[0][3];p.write_text(p.read_text().replace(',31,F,',',1,F,'))
        proof=json.loads((self.f.dist/e.DIRECTORY/e.MANIFEST).read_text());proof['executions'][e.SPECS[0][0]]['events_sha256']=e.sha(p.read_bytes());fixture.rebind_proof(self.f,proof)
        with self.assertRaisesRegex(ValueError,'times move backwards'):self.verify()
    def test_duplicate_pass_and_skip_required_ctest_is_rejected(self):
        p=self.f.dist/'test-results.xml';p.write_text(p.read_text().replace('</testsuite>','<testcase name="FlysnapperCore"><skipped/></testcase></testsuite>'))
        proof=json.loads((self.f.dist/e.DIRECTORY/e.MANIFEST).read_text());proof['ctest_sha256']=e.sha(p.read_bytes());fixture.rebind_proof(self.f,proof)
        with self.assertRaisesRegex(ValueError,'exactly once'):self.verify()
    def test_old_component_identity_fx_role_and_version_are_rejected(self):
        p=self.f.bundle/'Contents/Resources/moduleinfo.json';original=json.loads(p.read_text())
        for key,value in [('CID','ABCDEF019182FAEB466C797346736E31'),('Sub Categories',['Fx','Tools']),('Sub Categories',['Instrument','Fx']),('Sub Categories',['Instrument','Tools']),('Sub Categories',['Tools']),('Sub Categories',[]),('Version','0.4.0'),('Version','0.4.1')]:
            x=copy.deepcopy(original);x['Classes'][0][key]=value
            with self.subTest(key=key),self.assertRaises(ValueError):e.module_info(json.dumps(x).encode())
    def test_juce_trailing_commas_do_not_rewrite_quoted_values(self):
        p=self.f.bundle/'Contents/Resources/moduleinfo.json';x=json.loads(p.read_text());x['irrelevant']='comma,] stays in string'
        raw=json.dumps(x).replace('"Instrument"]','"Instrument",]').replace('}]','},]');self.assertEqual(e.module_info(raw.encode())['name'],'Flysnapper MIDI')
    def test_proof_inventory_and_unsafe_paths_are_rejected(self):
        (self.f.dist/e.DIRECTORY/'unexpected.txt').write_text('unexpected')
        with self.assertRaisesRegex(ValueError,'inventory differs'):self.verify()
        for path in ('../x','/x','x\\y','x//y','x/./y','C:x'):
            with self.subTest(path=path),self.assertRaises(ValueError):e.relative(path)
    def test_raw_configuration_case_inventory_is_explicit_and_exact(self):
        base=self.f.dist/e.DIRECTORY;report_path=base/e.SPECS[0][2];event_path=base/e.SPECS[0][3]
        report=json.loads(report_path.read_text());logical=report['cases'][0]['name'];raw=logical+'_instance_a'
        report['event_cases']=[raw];report_path.write_text(json.dumps(report));event_path.write_text(event_path.read_text().replace(logical,raw))
        proof=json.loads((base/e.MANIFEST).read_text());run=proof['executions'][e.SPECS[0][0]]
        run['report_sha256']=e.sha(report_path.read_bytes());run['events_sha256']=e.sha(event_path.read_bytes());fixture.rebind_proof(self.f,proof)
        self.verify()
        report['event_cases'].append('not actually emitted');report_path.write_text(json.dumps(report))
        run['report_sha256']=e.sha(report_path.read_bytes());fixture.rebind_proof(self.f,proof)
        with self.assertRaisesRegex(ValueError,'case inventory differs'):self.verify()

    def test_collect_rejects_changed_precompile_inputs(self):
        (self.f.root/'Source/Core/MidiEngine.cpp').write_text('changed after capture')
        with self.assertRaisesRegex(ValueError,'after compilation'):e.collect(self.f.root,self.f.build,self.f.dist,self.f.manifest)

if __name__=='__main__':unittest.main()
