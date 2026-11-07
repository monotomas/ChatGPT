# SPDX-License-Identifier: AGPL-3.0-or-later
"""Shipping/transaction regressions. Fake PE bytes are fixtures, not a plugin test.
Actual compiled VST3 validation remains a separate mandatory CTest.
"""
from __future__ import annotations
import csv,hashlib,importlib.util,io,json,os,shutil,subprocess,sys,tempfile,unittest
from pathlib import Path
from unittest import mock
from types import SimpleNamespace
from zipfile import ZipFile
ROOT=Path(__file__).resolve().parents[1]
def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path);result=importlib.util.module_from_spec(spec);spec.loader.exec_module(result);return result
shipping=module('windows_shipping_test',ROOT/'Tools/package_windows.py')
source=module('source_shipping_test',ROOT/'Tools/package_source.py')
publisher=module('publisher_shipping_test',ROOT/'Tools/publish_windows_delivery.py')
diagnostics=module('diagnostics_shipping_test',ROOT/'Tools/windows_audio_diagnostics.py')
dynamics=module('dynamics_shipping_test',ROOT/'Tools/input_dynamics_evidence.py')
with mock.patch.dict(sys.modules,{'package_windows':shipping}):
    installed_verifier=module('installed_verifier_shipping_test',ROOT/'Tools/verify_windows_install.py')

class WindowsPackageTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.base=Path(self.temp.name);self.root=self.base/'source';self.root.mkdir()
        # Legacy0.3 protocol fixtures remain independent of the current MIDI-only source inventory.
        legacy={'docs/INPUT_DYNAMICS.md','Tools/windows_audio_diagnostics.py','Tools/audio_validation_run.py',
                'Tools/audio_validation_verify.py','Tools/audio_validation_holdout.py','Tools/audio_validation_confirmation.py',
                *dynamics.EXTRA_INPUTS}
        for name in source.REQUIRED_FILES|legacy:
            f=self.root/name;f.parent.mkdir(parents=True,exist_ok=True);f.write_text('fixture '+name+'\n')
        for name in ('Tools/package_source.py','Tools/windows_installer.ps1','INSTALL_FLYSNAPPER.bat'):
            f=self.root/name;f.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(ROOT/name,f)
        (self.root/'dependencies.lock.json').write_text(json.dumps({'project_version':'1.0.0'}))
        self.dist=self.root/'dist';self.bundle=self.dist/'Flysnapper.vst3'
        p=self.bundle/shipping.BINARY;p.parent.mkdir(parents=True)
        pe=bytearray(512);pe[:2]=b'MZ';pe[60:64]=(128).to_bytes(4,'little');pe[128:134]=b'PE\0\0\x64\x86';p.write_bytes(pe)
        (self.bundle/'Contents/Resources').mkdir();(self.bundle/'Contents/Resources/moduleinfo.json').write_text('{}')
        self.required=['FlysnapperCore','FlysnapperPluginSmoke','FlysnapperActualVst3','FlysnapperProcessorAudio','FlysnapperPitch','FlysnapperYin','FlysnapperGuitarQuality','FlysnapperBendProcessor','FlysnapperNoise','FlysnapperInputDynamics','FlysnapperDynamicsProcessor','FlysnapperMpeOutput','FlysnapperMpeProcessor','FlysnapperMpeActualVst3','FlysnapperAudioQuality']
        self.manifest={'version':'1.0.0','stage':'complete','platform':'Windows-x64','tests':'passed','windows_verified':True,
                       'toolchain_probe':'passed','juceaide_unicode_probe':'passed','pitch_detection_implemented':True,
                       'required_test_names':self.required,'bundle_sha256':{n:hashlib.sha256(b).hexdigest() for n,b in shipping.files_below(self.bundle).items()},
                       'vst3_sha256':hashlib.sha256(pe).hexdigest(),'source_commit':'a'*40}
        self.save_manifest();self.report=''.join(f'<testcase name="{n}" status="run"/>' for n in self.required)
        self.save_report();(self.dist/'automated-tests.txt').write_text('fixture pass log, not actual plugin testing')
        self.output=self.base/'Windows.zip'
        evidence=self.dist/'audio-validation';evidence.mkdir()
        audio_sources=['Source/Pitch/PitchDetector.cpp','Source/Pitch/PitchDetector.h','Tools/audio_quality_gate.py','Tools/audio_validation.cpp']
        for name in audio_sources+['Tests/AudioFixtures/quality_gate.json','Tests/AudioFixtures/fixtures.json','Tests/AudioFixtures/fixtures.tsv']:
            path=self.root/name;path.parent.mkdir(parents=True,exist_ok=True)
            if not path.exists():path.write_text('fixture '+name)
        for fixture_dir,_,_,_ in diagnostics.SPECS.values():
            path=self.root/fixture_dir/'fixtures.json';path.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(ROOT/fixture_dir/'fixtures.json',path)
        summary={'gate':'FlysnapperAudioQuality','status':'passed','cases_checked':1,'cases_failed':0,'physical_live_latency_verified':False,'user_5_10ms_goals_verified':False}
        (evidence/'summary.json').write_text(json.dumps(summary))
        (evidence/'metrics.csv').write_text('fixture,not actual audio measurement\n')
        h=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
        self.audio_proof={'status':'passed','evaluator_exit_code':0,'evaluator_sha256':hashlib.sha256(b'fixture evaluator, not native Windows execution').hexdigest(),'summary_sha256':h(evidence/'summary.json'),'metrics_sha256':h(evidence/'metrics.csv'),
                         'source_sha256':{name:h(self.root/name) for name in audio_sources},
                         'policy_sha256':h(self.root/'Tests/AudioFixtures/quality_gate.json'),
                         'fixture_manifest_sha256':h(self.root/'Tests/AudioFixtures/fixtures.json'),
                         'fixture_inventory_sha256':h(self.root/'Tests/AudioFixtures/fixtures.tsv')}
        (evidence/'report-manifest.json').write_text(json.dumps(self.audio_proof))
        self.make_diagnostic_fixture()
        self.make_dynamics_fixture()
    def save_manifest(self):
        dynamics_path=self.dist/dynamics.DIRECTORY/dynamics.MANIFEST
        if dynamics_path.is_file():
            document=json.loads(dynamics_path.read_text());document['source_commit']=self.manifest['source_commit']
            dynamics_path.write_text(json.dumps(document))
            self.manifest['input_dynamics_evidence']={'path':dynamics.DIRECTORY+'/'+dynamics.MANIFEST,'sha256':hashlib.sha256(dynamics_path.read_bytes()).hexdigest()}
        (self.dist/'build-manifest.json').write_text(json.dumps(self.manifest))
        path=self.dist/'windows-audio-diagnostics'/diagnostics.MANIFEST
        if path.is_file():
            document=json.loads(path.read_text());document['source_commit']=self.manifest['source_commit'];path.write_text(json.dumps(document))
    def make_dynamics_fixture(self):
        # Protocol fixture only: actual compiled frontend execution is mandatory
        # in CTest/Windows CI and is never established by these synthetic bytes.
        build=self.root/'build/w-fixture';build.mkdir(parents=True,exist_ok=True)
        executable=build/'FlysnapperDynamicsProcessorTests_artefacts/Release/FlysnapperDynamicsProcessorTests.exe'
        executable.parent.mkdir(parents=True,exist_ok=True);executable.write_bytes((self.bundle/shipping.BINARY).read_bytes())
        for name in (*dynamics.READINESS_METADATA,*dynamics.READINESS_PCM):
            target=self.root/dynamics.READINESS/name;target.parent.mkdir(parents=True,exist_ok=True)
            shutil.copyfile(ROOT/dynamics.READINESS/name,target)
        report={'schema':1,'kind':dynamics.TEST,'status':'passed','checks':123,'physical_latency_measured':False,
                'cases':[{'name':'fixture','status':'passed'}],
                'fixture_inputs':{'fixture_root':dynamics.READINESS,'seal_sha256':dynamics.READINESS_SEAL,
                                  'files_sha256':dict(dynamics.READINESS_PCM),'hash_role':'Protocol fixture; no native execution'},
                'events_file':dynamics.EVENTS,'events_rows':2}
        events='case,rate,block,sample,type,note\nfixture,48000,64,2400,N,60\nfixture,48000,64,4800,F,60\n'
        for filename,digest in dynamics.READINESS_PCM.items():
            note=int(Path(filename).stem.split('_')[2])
            for bend,suffix in ((False,'mono_notes'),(True,'mono_bend')):
                name='sealed_'+Path(filename).stem+'_'+suffix
                report['cases'].append({'name':name,'status':'passed','fixture_file':filename,'expected_fixture_sha256':digest,
                                        'mono_bend':bend,'pitch':note,'reference_on_sample':24000,'reference_off_sample':74400,
                                        'decision_sample':25000,'actual_off_sample':76000,'latency_ms':1000/48})
                events+=f'{name},48000,64,25000,N,{note}\n{name},48000,64,76000,F,{note}\n'
        report['events_rows']=len(events.splitlines())-1
        (build/dynamics.REPORT).write_text(json.dumps(report))
        (build/dynamics.EVENTS).write_text(events)
        (build/dynamics.COMPILED_INPUTS).write_text(json.dumps(dynamics.source_hashes(self.root)))
        self.manifest['input_dynamics_evidence']=dynamics.collect(self.root,build,self.dist,self.manifest)
        self.save_manifest()
    def make_diagnostic_fixture(self):
        base=self.dist/'windows-audio-diagnostics';base.mkdir()
        fields=['case','family','mode','rate','block',*diagnostics.INTS,*diagnostics.LATENCY,*diagnostics.RELEASE,*diagnostics.CPU]
        source=self.audio_proof['source_sha256'];runner_source={n:d for n,d in source.items() if n.startswith('Source/Pitch/') or n=='Tools/audio_validation.cpp'}
        manifest={'schema':1,'kind':'WindowsAudioDiagnostics','status':'completed','measurement_only':True,'execution_platform':'Windows','source_commit':self.manifest['source_commit'],'evaluator_sha256':self.audio_proof['evaluator_sha256'],'basic_report_sha256':hashlib.sha256((self.dist/'audio-validation/report-manifest.json').read_bytes()).hexdigest(),'source_sha256':source,'orchestration_source_sha256':diagnostics.orchestration_sources(self.root),'datasets':{}}
        for name,(fixture_dir,count,role,_) in diagnostics.SPECS.items():
            destination=base/name;destination.mkdir();reference=json.loads((self.root/fixture_dir/'fixtures.json').read_text())['cases']
            with (destination/'audio-cases.csv').open('w',newline='') as mf,(destination/'audio-events.csv').open('w',newline='') as ef:
                metrics=csv.DictWriter(mf,fieldnames=fields);metrics.writeheader();events=csv.DictWriter(ef,fieldnames=['case','rate','block','on','note','velocity','sample','time_ms']);events.writeheader()
                for case in reference:
                    for rate in diagnostics.RATES:
                        for block in diagnostics.BLOCKS:
                            expected=len(case['truth']);row=dict.fromkeys(fields,'0');row.update(case=case['name'],family=case['family'],mode=case['mode'],rate=rate,block=block,expected=expected,matched=expected)
                            # A diagnostic is allowed to report musical failure.
                            extras=2 if name=='public' and case is reference[0] else 0;row['false_note_ons']=extras
                            for key in diagnostics.LATENCY:row[key]='5' if expected else ''
                            for key in diagnostics.RELEASE:row[key]='20' if expected else ''
                            for key in diagnostics.CPU:row[key]='1'
                            metrics.writerow(row)
                            for n in range(expected+extras):
                                sample=2400+n*500
                                for on,offset in ((1,0),(0,1000)):events.writerow({'case':case['name'],'rate':rate,'block':block,'on':on,'note':60,'velocity':100 if on else 0,'sample':sample+offset,'time_ms':f'{1000*(sample+offset)/rate:.6f}'})
            fixture_hash=hashlib.sha256((self.root/fixture_dir/'fixtures.json').read_bytes()).hexdigest()
            measurement={'runner_exit_code':0,'source_changed_during_measurement':False,'measurement_not_acceptance':True,'binary_sha256':manifest['evaluator_sha256'],'source_sha256':runner_source,'evaluation_role':role,'fixtures_sha256':fixture_hash,'configuration':'full:44100/48000/96000Hz x32/64/128samples','independent_holdout':name!='public','second_confirmation':name=='confirmation'}
            (destination/'measurement-manifest.json').write_text(json.dumps(measurement));(destination/'audio-summary.txt').write_text('Fixture measurements with intentional errors; not actual audio validation.');(destination/'runner-output.txt').write_text('Fixture process output; not native Windows execution.')
            manifest['datasets'][name]={'case_configurations':count*9,'reference_cases':count,'evaluation_role':role,'fixture_manifest_sha256':fixture_hash,'runner_exit_code':0}
        manifest['files_sha256']={n:hashlib.sha256(b).hexdigest() for n,b in diagnostics.inventory(base).items()}
        (base/diagnostics.MANIFEST).write_text(json.dumps(manifest))
    def save_report(self):
        (self.dist/'test-results.xml').write_text('<testsuite>'+self.report+'</testsuite>')
    def package(self):return shipping.build_package(self.root,self.dist,self.output)
    def extract(self):
        self.package();out=self.base/'Olav Lægreid/Extracted'
        with ZipFile(self.output) as z:z.extractall(out)
        return next(out.iterdir())
    def test_complete_package_contains_prebuilt_binary_installer_source_and_proof(self):
        self.package();shipping.verify_package(self.output)
        with ZipFile(self.output) as z:
            p='Flysnapper-1.0.0-Windows-x64/'
            for n in ('INSTALL_FLYSNAPPER.bat','Install-Flysnapper.ps1','BRUKERVEILEDNING.md','INPUT_DYNAMICS.md','Flysnapper.vst3/'+shipping.BINARY,
                      'Source/Flysnapper-Source.zip','Validation/test-results.xml','Validation/build-manifest.json','Validation/audio-validation/summary.json',
                      'Validation/input-dynamics/report-manifest.json','Validation/input-dynamics/Flysnapper-dynamics-evaluation.json',
                      'Validation/input-dynamics/Flysnapper-dynamics-events.csv',
                      'Validation/windows-audio-diagnostics/diagnostics-manifest.json',
                      'Validation/windows-audio-diagnostics/public/audio-cases.csv',
                      'Validation/windows-audio-diagnostics/holdout/audio-events.csv',
                      'Validation/windows-audio-diagnostics/confirmation/measurement-manifest.json'):
                self.assertIn(p+n,z.namelist())
            release=json.loads(z.read(p+'release-manifest.json'))
            for n,h in release['files'].items():self.assertEqual(hashlib.sha256(z.read(p+n)).hexdigest(),h)
            readme=z.read(p+'LES_MEG.txt').decode('utf-8-sig')
            self.assertIn('Learn / 20 seconds',readme);self.assertIn('i 17 sekunder',readme)
            self.assertIn('definerte halvtoner uten polyfonisk bend',readme)
    def test_linux_failed_or_incomplete_proof_is_rejected(self):
        for key,value in [('platform','Linux-x86_64'),('tests','failed'),('version','0.1.0'),('windows_verified',False),
                          ('toolchain_probe','missing'),('juceaide_unicode_probe','missing'),('pitch_detection_implemented',False),('stage','routing-midi-prototype'),('stage','musical-readiness-research-prototype')]:
            with self.subTest(key=key):
                old=self.manifest[key];self.manifest[key]=value;self.save_manifest()
                with self.assertRaises(ValueError):self.package()
                self.assertFalse(self.output.exists());self.manifest[key]=old
        self.save_manifest()
    def test_030_requires_explicit_dated_guitar_evaluation_scope(self):
        self.manifest['version']='0.3.0';self.save_manifest()
        with self.assertRaisesRegex(ValueError,'evaluation stage'):shipping.verified_payload(self.dist,'0.3.0')
        self.manifest['stage']=dynamics.EVALUATION_STAGE;self.manifest['evaluation_scope']=dict(dynamics.EVALUATION_SCOPE)
        self.save_manifest();shipping.verified_payload(self.dist,'0.3.0')
        for key,value in (('revision_date','2026-11-04'),('polyphonic_bending',True),('real_guitar_test_pending',False),('latency','fixed-5ms')):
            with self.subTest(key=key):
                self.manifest['evaluation_scope'][key]=value;self.save_manifest()
                with self.assertRaisesRegex(ValueError,'evaluation scope'):shipping.verified_payload(self.dist,'0.3.0')
                self.manifest['evaluation_scope']=dict(dynamics.EVALUATION_SCOPE)
    def test_required_test_must_exist_and_run_without_skip_or_failure(self):
        for content in ('','<testcase name="FlysnapperAudioQuality"><skipped/></testcase>',
                        '<testcase name="FlysnapperAudioQuality"><failure/></testcase>',
                        '<testcase name="FlysnapperAudioQuality" status="disabled"/>'):
            self.report=''.join(f'<testcase name="{n}"/>' for n in self.required[:-1])+content;self.save_report()
            with self.assertRaises(ValueError):self.package()
    def test_guitar_noise_dynamics_and_mpe_groups_are_required_for_shipping(self):
        for missing in ('FlysnapperGuitarQuality','FlysnapperBendProcessor','FlysnapperNoise','FlysnapperInputDynamics','FlysnapperDynamicsProcessor','FlysnapperMpeOutput','FlysnapperMpeProcessor','FlysnapperMpeActualVst3'):
            with self.subTest(missing=missing):
                self.manifest['required_test_names']=[name for name in self.required if name!=missing]
                self.save_manifest()
                with self.assertRaisesRegex(ValueError,'required validation'):self.package()
    def test_tampered_or_extra_binary_bundle_file_blocks_package(self):
        (self.bundle/shipping.BINARY).write_bytes(b'tampered')
        with self.assertRaises(ValueError):self.package()
    def test_missing_or_changed_full_processor_dynamics_evidence_blocks_shipping(self):
        path=self.dist/dynamics.DIRECTORY/dynamics.EVENTS;data=path.read_bytes()
        path.unlink()
        with self.assertRaisesRegex(ValueError,'dynamics evidence'):self.package()
        path.write_bytes(data+b'changed')
        with self.assertRaises(ValueError):self.package()
        path.write_bytes(data)
        (self.root/'Source/Pitch/InputDynamics.h').write_text('changed after the measured build')
        with self.assertRaisesRegex(ValueError,'source differs'):self.package()
    def test_extra_resource_cannot_escape_bundle_manifest(self):
        (self.bundle/'Contents/Resources/unexpected.txt').write_text('extra')
        with self.assertRaises(ValueError):self.package()
    def test_32bit_pe_is_rejected_even_with_matching_hashes(self):
        p=self.bundle/shipping.BINARY;b=bytearray(p.read_bytes());b[132:134]=b'\x4c\x01';p.write_bytes(b)
        h=hashlib.sha256(b).hexdigest();self.manifest['vst3_sha256']=h;self.manifest['bundle_sha256'][shipping.BINARY]=h;self.save_manifest()
        with self.assertRaisesRegex(ValueError,'x64'):self.package()
    def test_unsafe_windows_paths_are_rejected(self):
        for name in ('../x','/x','a//b','a/../b','a\\b','C:x','CON','a/COM1.txt','a./x','x:stream'):
            with self.subTest(name=name),self.assertRaises(ValueError):shipping.safe_relative(name)
    def test_native_windows_provenance_paths_normalize_without_changing_hashes(self):
        proof=self.audio_proof.copy();proof['source_sha256']={name.replace('/','\\'):digest for name,digest in self.audio_proof['source_sha256'].items()}
        path=self.dist/'audio-validation/report-manifest.json';path.write_text(json.dumps(proof))
        diagnostic_path=self.dist/'windows-audio-diagnostics'/diagnostics.MANIFEST
        document=json.loads(diagnostic_path.read_text());document['basic_report_sha256']=hashlib.sha256(path.read_bytes()).hexdigest();diagnostic_path.write_text(json.dumps(document))
        before=path.read_bytes();self.package();self.assertEqual(path.read_bytes(),before)
        shipping.verify_package(self.output)
    def test_full_diagnostics_are_measurements_allowing_errors_but_require_all_corpora(self):
        files=diagnostics.verify_saved(self.root,self.dist,self.manifest)
        document=json.loads(files[diagnostics.MANIFEST]);self.assertTrue(document['measurement_only'])
        rows=list(csv.DictReader(files['public/audio-cases.csv'].decode().splitlines()))
        self.assertTrue(any(int(row['false_note_ons'])>0 for row in rows))
        self.package()
        shutil.rmtree(self.dist/'windows-audio-diagnostics/confirmation')
        with self.assertRaisesRegex(ValueError,'inventory'):self.package()
    def test_stale_or_failed_diagnostic_runner_cannot_ship(self):
        base=self.dist/'windows-audio-diagnostics';path=base/'holdout/measurement-manifest.json';original=path.read_bytes()
        manifest_path=base/diagnostics.MANIFEST;document=json.loads(manifest_path.read_text())
        for key,value in (('source_changed_during_measurement',True),('runner_exit_code',1),('binary_sha256','0'*64),('evaluation_role','first-blind')):
            data=json.loads(original);data[key]=value;path.write_text(json.dumps(data));document['files_sha256']['holdout/measurement-manifest.json']=hashlib.sha256(path.read_bytes()).hexdigest();manifest_path.write_text(json.dumps(document))
            with self.subTest(key=key),self.assertRaises(ValueError):self.package()
        path.write_bytes(original)
    def test_truncated_or_nonfinite_full_matrix_cannot_be_hidden_by_fresh_checksum(self):
        base=self.dist/'windows-audio-diagnostics';path=base/'public/audio-cases.csv';original=path.read_bytes();manifest_path=base/diagnostics.MANIFEST
        reader=csv.DictReader(io.StringIO(original.decode(),newline=''));rows=list(reader);rows[0]['cpu_median_us']='nan'
        invalid=io.StringIO(newline='');writer=csv.DictWriter(invalid,fieldnames=reader.fieldnames);writer.writeheader();writer.writerows(rows)
        for payload in (b''.join(original.splitlines(keepends=True)[:-1]),invalid.getvalue().encode()):
            path.write_bytes(payload);document=json.loads(manifest_path.read_text());document['files_sha256']['public/audio-cases.csv']=hashlib.sha256(payload).hexdigest();manifest_path.write_text(json.dumps(document))
            with self.assertRaises(ValueError):self.package()
    def test_diagnostic_driver_runs_three_explicit_roles_with_bound_fixture_evaluator(self):
        # This tests orchestration using explicit fixture PE bytes/processes;
        # only actual Windows CI can establish native measurements.
        base=self.dist/'windows-audio-diagnostics'
        saved={name:{n:(base/name/n).read_bytes() for n in diagnostics.FILES if n!='runner-output.txt'} for name in diagnostics.SPECS}
        shutil.rmtree(base)
        evaluator=self.root/'build/w-fixture/FlysnapperAudioValidation.exe';evaluator.parent.mkdir(parents=True,exist_ok=True);shutil.copyfile(self.bundle/shipping.BINARY,evaluator)
        self.audio_proof['evaluator_sha256']=hashlib.sha256(evaluator.read_bytes()).hexdigest()
        (self.dist/'audio-validation/report-manifest.json').write_text(json.dumps(self.audio_proof))
        calls=[]
        def fake_run(command,**kwargs):
            destination=Path(command[command.index('--output')+1]);name=destination.name;calls.append(command)
            self.assertEqual(command[command.index('--executable')+1],str(evaluator.resolve()))
            self.assertEqual(command[command.index('--evaluation-role')+1],diagnostics.SPECS[name][2]);self.assertIn('--full',command)
            for filename,data in saved[name].items():
                if filename=='measurement-manifest.json':
                    document=json.loads(data);document['binary_sha256']=self.audio_proof['evaluator_sha256'];data=json.dumps(document).encode()
                (destination/filename).write_bytes(data)
            kwargs['stdout'].write(b'Fixture process protocol only; no native audio execution.\n')
            return subprocess.CompletedProcess(command,0)
        with mock.patch.dict(sys.modules,{'package_windows':shipping}),mock.patch.object(diagnostics,'os',SimpleNamespace(name='nt',environ=os.environ.copy())),mock.patch.object(diagnostics.platform,'system',return_value='Windows'),mock.patch.object(diagnostics.subprocess,'run',side_effect=fake_run):
            result=diagnostics.run(self.root,self.dist,self.manifest['source_commit'])
        self.assertEqual(result['status'],'completed');self.assertTrue(result['measurement_only']);self.assertEqual(len(calls),3)
        self.assertIn('--holdout',calls[1]);self.assertIn('--confirmation',calls[2])
        diagnostics.verify_saved(self.root,self.dist,self.manifest)
    def test_duplicate_or_unsafe_windows_provenance_paths_are_rejected(self):
        path=self.dist/'audio-validation/report-manifest.json'
        proof=self.audio_proof.copy();proof['source_sha256']=self.audio_proof['source_sha256'].copy()
        proof['source_sha256']['Source\\Pitch\\PitchDetector.cpp']=proof['source_sha256']['Source/Pitch/PitchDetector.cpp']
        path.write_text(json.dumps(proof))
        with self.assertRaisesRegex(ValueError,'Duplicate'):self.package()
        proof['source_sha256']=self.audio_proof['source_sha256'].copy();proof['source_sha256']['..\\outside.cpp']='0'*64
        path.write_text(json.dumps(proof))
        with self.assertRaisesRegex(ValueError,'Unsafe'):self.package()
    def test_failed_stale_or_tampered_audio_report_cannot_ship(self):
        evidence=self.dist/'audio-validation'
        original=(evidence/'summary.json').read_bytes()
        (evidence/'summary.json').write_text(json.dumps({'gate':'FlysnapperAudioQuality','status':'failed','cases_checked':30,'cases_failed':1}))
        with self.assertRaises(ValueError):self.package()
        (evidence/'summary.json').write_bytes(original)
        (self.root/'Source/Pitch/PitchDetector.cpp').write_text('changed after passing measurement')
        with self.assertRaisesRegex(ValueError,'different source'):self.package()
    def test_duplicate_required_pass_and_skip_cannot_ship(self):
        self.report+='<testcase name="FlysnapperAudioQuality"><skipped/></testcase>';self.save_report()
        with self.assertRaisesRegex(ValueError,'exactly once'):self.package()
    def test_package_is_deterministic(self):
        self.package();first=self.output.read_bytes();self.package();self.assertEqual(first,self.output.read_bytes())
    def installation_proof(self,commit):
        ps_log=self.base/'installation-powershell.txt';ps_log.write_text('fixture PS transaction result, not actual Windows proof')
        host_log=self.base/'installed-vst3-host.txt';host_log.write_text('RESULT actual VST3 host tests passed (123 assertions).\nPLATFORM VERIFIED: Windows x64 compiled VST3 loaded and executed by native test host.\n')
        launcher_log=self.base/'installation-launcher.txt';marker=f'Installed Flysnapper {self.manifest["version"]}:';launcher_log.write_text(marker+' fixture destination\n')
        proof={'schema':1,'status':'passed','execution_platform':'Windows','source_commit':commit,
               'artifact_sha256':hashlib.sha256(self.output.read_bytes()).hexdigest(),'installer_exit_code':0,
               'installed_host_exit_code':0,'host_assertions':123,'installed_files_sha256':self.manifest['bundle_sha256'],
               'recovery_files_sha256':self.manifest['bundle_sha256'],'unicode_paths_verified':True,'unrelated_plugin_preserved':True,
               'powershell_log_sha256':hashlib.sha256(ps_log.read_bytes()).hexdigest(),'host_log_sha256':hashlib.sha256(host_log.read_bytes()).hexdigest(),
               'bat_launcher_verified':True,'bat_launcher_exit_code':0,'bat_launcher_marker':marker,
               'bat_launcher_sha256':hashlib.sha256((self.root/'INSTALL_FLYSNAPPER.bat').read_bytes()).hexdigest(),
               'launcher_recovery_files_sha256':self.manifest['bundle_sha256'],'launcher_log_sha256':hashlib.sha256(launcher_log.read_bytes()).hexdigest()}
        path=self.base/'installation-validation.json';path.write_text(json.dumps(proof));return path
    def test_installed_module_proof_is_mandatory_and_bound_to_exact_zip_and_source(self):
        self.package();proof=self.installation_proof('a'*40)
        publisher.installed_package_proof(proof,'a'*40,hashlib.sha256(self.output.read_bytes()).hexdigest(),self.manifest)
        with self.assertRaisesRegex(ValueError,'missing'):publisher.installed_package_proof(None,'a'*40,'0'*64,self.manifest)
        with self.assertRaisesRegex(ValueError,'different source or ZIP'):publisher.installed_package_proof(proof,'b'*40,'0'*64,self.manifest)
        original=json.loads(proof.read_text())
        for key,value in [('installer_exit_code',1),('installed_host_exit_code',1),('host_assertions',0),('unicode_paths_verified',False),('bat_launcher_verified',False),('bat_launcher_exit_code',1),('launcher_recovery_files_sha256',{})]:
            document=original.copy();document[key]=value;proof.write_text(json.dumps(document))
            with self.subTest(key=key),self.assertRaises(ValueError):publisher.installed_package_proof(proof,'a'*40,hashlib.sha256(self.output.read_bytes()).hexdigest(),self.manifest)
        document=original.copy();document['execution_platform']='Linux';proof.write_text(json.dumps(document))
        with self.assertRaisesRegex(ValueError,'did not pass'):publisher.installed_package_proof(proof,'a'*40,'0'*64,self.manifest)
    def test_legacy_020_install_proof_remains_valid_without_bat_claim(self):
        self.package();proof=self.installation_proof('a'*40);document=json.loads(proof.read_text())
        for key in list(document):
            if key.startswith('bat_launcher') or key.startswith('launcher_'):document.pop(key)
        proof.write_text(json.dumps(document));build={**self.manifest,'version':'0.2.0'}
        publisher.installed_package_proof(proof,'a'*40,hashlib.sha256(self.output.read_bytes()).hexdigest(),build)
    def test_cmd_launcher_quotes_unicode_and_shell_metacharacters(self):
        cmd=Path('C:/Windows/System32/cmd.exe');bat=Path('D:/Olav Lægreid (test) & routing/INSTALL_FLYSNAPPER.bat');dest=Path('D:/Separate VST3 (test) & routing');backup=Path('D:/Recovery outside scan')
        command=installed_verifier.launcher_command(cmd,bat,dest,backup)
        self.assertEqual(command,f'"{cmd}" /d /s /c ""{bat}" -NoUI -Destination "{dest}" -BackupDirectory "{backup}""')
        with self.assertRaises(ValueError):installed_verifier.launcher_command(cmd,Path('D:/bad"name.bat'),dest,backup)
    def test_packaged_bat_launcher_native_windows_fixture_transaction(self):
        if os.name!='nt':self.skipTest('The actual BAT command processor is exercised on native Windows CI')
        package=self.extract();destination=self.base/'Olav Lægreid (test) & routing/Separate VST3';backup=self.base/'Olav Lægreid (test) & routing/Recovery outside scan'
        cmd=Path(os.environ.get('SystemRoot','C:/Windows'))/'System32/cmd.exe'
        command=installed_verifier.launcher_command(cmd,package/'INSTALL_FLYSNAPPER.bat',destination,backup)
        # Real BAT/PS5 entry points, with deliberately fake PE fixture bytes.
        # Actual compiled DLL loading remains the post-build installation gate.
        for _ in range(2):
            result=subprocess.run(command,executable=str(cmd),cwd=package,stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)
            self.assertEqual(result.returncode,0,result.stdout.decode('utf-8',errors='replace'))
            self.assertIn(b'Installed Flysnapper 1.0.0:',result.stdout)
            self.assertEqual({n:hashlib.sha256(b).hexdigest() for n,b in shipping.files_below(destination/'Flysnapper.vst3').items()},self.manifest['bundle_sha256'])
        copies=list(backup.glob('*/Flysnapper.vst3'));self.assertEqual(len(copies),1)
        self.assertEqual({n:hashlib.sha256(b).hexdigest() for n,b in shipping.files_below(copies[0]).items()},self.manifest['bundle_sha256'])
    def test_packaged_ps_entrypoint_accepts_private_recovery_directory(self):
        shell=shutil.which('powershell.exe') if os.name=='nt' else shutil.which('pwsh')
        portable=Path('/workspace/.flysnapper-tools/powershell/pwsh')
        if not shell and portable.is_file():shell=str(portable)
        if not shell:self.skipTest('PowerShell unavailable; native Windows CI runs this entrypoint')
        package=self.extract();destination=self.base/'Olav Lægreid (test) & routing/Separate VST3';backup=self.base/'Olav Lægreid (test) & routing/Recovery outside scan'
        env=os.environ.copy()
        if os.name!='nt':env.update(XDG_CACHE_HOME=str(self.base/'xdg-cache'),XDG_CONFIG_HOME=str(self.base/'xdg-config'),XDG_DATA_HOME=str(self.base/'xdg-data'))
        prefix=[shell,'-NoLogo','-NoProfile','-ExecutionPolicy','Bypass']+(['-STA'] if os.name=='nt' else [])+['-File',str(package/'Install-Flysnapper.ps1')]
        # Exercise the native -File entry point with both root forms. The
        # explicit first installation distinguishes default-root resolution
        # from destination/backup handling in a failing native PS5 log.
        for explicit_root in (True,False):
            with self.subTest(explicit_package_root=explicit_root):
                command=prefix+(['-PackageRoot',str(package)] if explicit_root else [])+['-NoUI','-Destination',str(destination),'-BackupDirectory',str(backup)]
                result=subprocess.run(command,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)
                self.assertEqual(result.returncode,0,result.stdout.decode('utf-8',errors='replace'))
        copies=list(backup.glob('*/Flysnapper.vst3'));self.assertEqual(len(copies),1)
        self.assertEqual({n:hashlib.sha256(b).hexdigest() for n,b in shipping.files_below(copies[0]).items()},self.manifest['bundle_sha256'])
        for parameter in ('PackageRoot','Destination','BackupDirectory'):
            with self.subTest(reject_relative_parameter=parameter):
                arguments={'PackageRoot':str(package),'Destination':str(destination),'BackupDirectory':str(backup)}
                arguments[parameter]='relative-installer-test'
                command=prefix+['-NoUI']+[item for name,value in arguments.items() for item in ('-'+name,value)]
                result=subprocess.run(command,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)
                self.assertNotEqual(result.returncode,0)
                self.assertIn(b'Rejected value: [relative-installer-test]',result.stdout)
                self.assertIn(b'Installer call stack:',result.stdout)
                self.assertEqual({n:hashlib.sha256(b).hexdigest() for n,b in shipping.files_below(destination/'Flysnapper.vst3').items()},self.manifest['bundle_sha256'])
    def test_installed_module_host_must_execute_nonzero_tests_on_windows(self):
        for assertions in (1,271828):
            good=f'RESULT actual VST3 host tests passed ({assertions} assertions).\nPLATFORM VERIFIED: Windows x64 compiled VST3 loaded and executed by native test host.'
            self.assertEqual(installed_verifier.passed_host_output(good,0),assertions)
        for text,code in [(good,1),('',0),(good.replace(str(assertions),'0'),0),(good.replace('Windows x64','native Linux'),0)]:
            with self.assertRaises(ValueError):installed_verifier.passed_host_output(text,code)
    def test_host_discovery_refuses_stale_or_ambiguous_builds(self):
        with self.assertRaises(ValueError):installed_verifier.find_host(self.root)
        host=self.root/'build/w-0123456789ab/FlysnapperVst3HostTests_artefacts/Release/FlysnapperVst3HostTests.exe'
        host.parent.mkdir(parents=True);host.write_bytes(b'fixture')
        self.assertEqual(installed_verifier.find_host(self.root),host)
        other=self.root/'build/w-111111111111/FlysnapperVst3HostTests_artefacts/Release/FlysnapperVst3HostTests.exe'
        other.parent.mkdir(parents=True);other.write_bytes(b'fixture')
        with self.assertRaises(ValueError):installed_verifier.find_host(self.root)
    def make_git_source(self):
        remote=self.base/'remote.git'
        subprocess.run(['git','init','--quiet','--bare',str(remote)],check=True)
        def git(*args):return subprocess.check_output(['git','-C',str(self.root),*map(str,args)],stderr=subprocess.STDOUT)
        git('init','--quiet');git('config','user.name','Fixture');git('config','user.email','fixture@example.invalid')
        (self.root/'source.txt').write_text('source fixture');git('add','source.txt');git('commit','--quiet','-m','fixture source');git('remote','add','origin',remote)
        return git,git('rev-parse','HEAD').decode().strip()
    def test_complete_publication_and_failed_next_build_preserve_verified_artifact(self):
        (self.root/'dependencies.lock.json').write_text(json.dumps({'project_version':'0.3.0'}))
        self.manifest.update(version='0.3.0',stage=dynamics.EVALUATION_STAGE,evaluation_scope=dict(dynamics.EVALUATION_SCOPE))
        self.make_dynamics_fixture()
        git,commit=self.make_git_source();self.manifest['source_commit']=commit;self.save_manifest();self.package()
        with mock.patch.dict(os.environ,{'GITHUB_SHA':commit}),mock.patch.dict(sys.modules,{'package_windows':shipping}):
            publisher.publish(self.root,'completed',self.output,None,self.dist/'build-manifest.json',self.dist/'test-results.xml',installation_proof=self.installation_proof(commit))
        git('fetch','--quiet','origin',publisher.BRANCH)
        state=json.loads(git('show','FETCH_HEAD:status.json'))
        self.assertEqual(state['status'],'completed');self.assertEqual(state['source_commit'],commit)
        self.assertEqual(state['stage'],dynamics.EVALUATION_STAGE);self.assertEqual(state['evaluation_scope'],dynamics.EVALUATION_SCOPE)
        self.assertIn('for gitarprøve',git('show','FETCH_HEAD:README.md').decode('utf-8'))
        self.assertEqual(state['artifact']['sha256'],hashlib.sha256(self.output.read_bytes()).hexdigest())
        delivered=git('show','FETCH_HEAD:downloads/Flysnapper-Windows-x64.zip');self.assertEqual(delivered,self.output.read_bytes())
        invalid=dict(self.manifest);invalid['evaluation_scope']=dict(dynamics.EVALUATION_SCOPE,real_guitar_test_pending=False)
        (self.dist/'build-manifest.json').write_text(json.dumps(invalid))
        with mock.patch.dict(os.environ,{'GITHUB_SHA':commit}),mock.patch.dict(sys.modules,{'package_windows':shipping}),self.assertRaisesRegex(ValueError,'evaluation scope'):
            publisher.publish(self.root,'completed',self.output,None,self.dist/'build-manifest.json',self.dist/'test-results.xml',installation_proof=self.installation_proof(commit))
        self.save_manifest()
        (self.root/'source.txt').write_text('next source');git('add','source.txt');git('commit','--quiet','-m','next fixture source')
        next_commit=git('rev-parse','HEAD').decode().strip()
        with mock.patch.dict(os.environ,{'GITHUB_SHA':next_commit}):publisher.publish(self.root,'failed',None,None,None,None)
        git('fetch','--quiet','origin',publisher.BRANCH)
        state=json.loads(git('show','FETCH_HEAD:status.json'))
        self.assertEqual(state['status'],'failed');self.assertIsNone(state['artifact']);self.assertEqual(state['source_commit'],next_commit)
        self.assertEqual(git('show','FETCH_HEAD:downloads/Flysnapper-Windows-x64.zip'),delivered)
    def test_bad_completed_provenance_publishes_failed_state_instead_of_staying_running(self):
        git,commit=self.make_git_source();self.package()
        with mock.patch.dict(os.environ,{'GITHUB_SHA':commit}),mock.patch.dict(sys.modules,{'package_windows':shipping}),self.assertRaises(SystemExit) as failure:
            publisher.main(['--root',str(self.root),'--result','completed','--artifact',str(self.output),
                            '--build-manifest',str(self.dist/'build-manifest.json'),'--test-results',str(self.dist/'test-results.xml')])
        self.assertEqual(failure.exception.code,1)
        git('fetch','--quiet','origin',publisher.BRANCH)
        state=json.loads(git('show','FETCH_HEAD:status.json'))
        self.assertEqual(state['status'],'failed');self.assertIn('different source commit',state['error'])
        self.assertIsNone(state['artifact'])
    def test_packaged_installer_driver_protocol_with_fixture_payload(self):
        shell=shutil.which('powershell.exe') if os.name=='nt' else shutil.which('pwsh')
        portable=Path('/workspace/.flysnapper-tools/powershell/pwsh')
        if not shell and portable.is_file():shell=str(portable)
        if not shell:self.skipTest('PowerShell unavailable; native Windows CI runs this protocol')
        package=self.extract();script=self.base/'actual-driver-fixture.ps1';script.write_text(installed_verifier.PS_DRIVER,encoding='utf-8-sig')
        result_file=self.base/'driver-result.json';env=os.environ.copy()
        env.update(FLYSNAPPER_VERIFY_PACKAGE=str(package),FLYSNAPPER_VERIFY_DEST=str(self.base/'Olav Lægreid/Separate VST3'),
                   FLYSNAPPER_VERIFY_BACKUP=str(self.base/'Olav Lægreid/Recovery'),FLYSNAPPER_VERIFY_RESULT=str(result_file))
        if os.name!='nt':env.update(XDG_CACHE_HOME=str(self.base/'xdg-cache'),XDG_CONFIG_HOME=str(self.base/'xdg-config'),XDG_DATA_HOME=str(self.base/'xdg-data'))
        command=[shell,'-NoLogo','-NoProfile','-ExecutionPolicy','Bypass']+(['-STA'] if os.name=='nt' else [])+['-File',str(script)]
        result=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,env=env)
        self.assertEqual(result.returncode,0,result.stdout.decode('utf-8',errors='replace'))
        document=json.loads(result_file.read_text(encoding='utf-8-sig'))
        self.assertEqual(document['installed_files_sha256'],self.manifest['bundle_sha256'])
        self.assertEqual(document['recovery_files_sha256'],self.manifest['bundle_sha256'])
        self.assertTrue(document['unrelated_plugin_preserved'])
        self.assertIn('Olav Lægreid',document['installed_bundle'])
    def test_installer_transaction_integrity_unicode_recovery_and_rollback(self):
        shell=shutil.which('powershell.exe') if os.name=='nt' else shutil.which('pwsh')
        portable=Path('/workspace/.flysnapper-tools/powershell/pwsh')
        if not shell and portable.is_file():shell=str(portable)
        if not shell:self.skipTest('PowerShell runtime unavailable; Windows CI must run this test')
        package=self.extract();destination=self.base/'Olav Lægreid/Plugins';backups=self.base/'Recovery'
        script=self.base/'installer-regression.ps1'
        # Keep script text literal: values enter through environment variables,
        # not through PowerShell/shell interpolation of test paths.
        script.write_text(r'''
$ErrorActionPreference = 'Stop'
. $env:FLYSNAPPER_INSTALLER_SOURCE -LibraryOnly
function Get-FileHash { throw 'Installer must not depend on external Get-FileHash module discovery' }
function Assert-Test($Condition, $Message) { if (-not $Condition) { throw $Message } }
if ($env:OS -eq 'Windows_NT') {
    $dialog = New-FlysnapperFolderDialog
    try {
        Assert-Test ($dialog -is [System.Windows.Forms.FolderBrowserDialog]) 'Windows folder dialog factory failed'
        Assert-Test $dialog.ShowNewFolderButton 'Folder creation UI missing'
        Assert-Test ($dialog.SelectedPath.EndsWith('VST3')) 'Default Windows VST3 path missing'
    } finally { $dialog.Dispose() }
}
$package = $env:FLYSNAPPER_TEST_PACKAGE
$dest = $env:FLYSNAPPER_TEST_DEST
$backup = $env:FLYSNAPPER_TEST_BACKUP
$first = Install-FlysnapperPackage $package $dest $backup
Assert-Test (Test-Path -LiteralPath $first.Installed) 'First install missing'
Assert-Test ([string]::IsNullOrEmpty($first.Backup)) 'First install created false old backup'
$other = Join-Path $dest 'Buffkiller.vst3/keep.txt'
[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($other)) | Out-Null
[IO.File]::WriteAllText($other, 'unchanged Buffkiller')
$old = Join-Path $first.Installed 'old-user-file.txt'
[IO.File]::WriteAllText($old, 'old installation file')
$second = Install-FlysnapperPackage $package $dest $backup
Assert-Test (-not (Test-Path -LiteralPath $old)) 'New bundle contains old files'
Assert-Test ([IO.File]::ReadAllText((Join-Path $second.Backup 'old-user-file.txt')) -eq 'old installation file') 'Recovery copy incomplete'
Assert-Test ([IO.File]::ReadAllText($other) -eq 'unchanged Buffkiller') 'Other plugin altered'
Assert-Test (-not (Test-FlysnapperWithin $second.Backup $dest)) 'Backup remains in scan root'
$before = Get-FlysnapperInventory $second.Installed
foreach ($bad in @((Join-Path $package 'NestedVST3'), (Join-Path $dest 'Flysnapper.vst3'), ([IO.Path]::GetDirectoryName($package)))) {
    $failed = $false
    try { Install-FlysnapperPackage $package $bad $backup | Out-Null } catch { $failed = $true }
    Assert-Test $failed 'Unsafe destination was accepted'
}
$failed = $false
try { Install-FlysnapperPackage $package $dest (Join-Path $dest 'Backups') | Out-Null } catch { $failed = $true }
Assert-Test $failed 'Recovery copy was allowed inside scan root'
Assert-FlysnapperMatchingInventory (Get-FlysnapperInventory $second.Installed) $before
# Exercise failed final rename; the original is restored transactionally.
function Move-FlysnapperDirectory([string]$Source, [string]$Target) {
    if ([IO.Path]::GetFileName($Source).StartsWith('.Flysnapper-install-')) { throw 'injected final rename failure' }
    [IO.Directory]::Move($Source, $Target)
}
$failed = $false
try { Install-FlysnapperPackage $package $dest $backup | Out-Null } catch { $failed = $true }
Assert-Test $failed 'Injected rename failure was ignored'
Assert-FlysnapperMatchingInventory (Get-FlysnapperInventory $second.Installed) $before
Assert-Test ([IO.File]::ReadAllText($other) -eq 'unchanged Buffkiller') 'Rollback changed other plugin'
# Tampered package must not touch the installed plugin.
$binary = Join-Path $package 'Flysnapper.vst3/Contents/x86_64-win/Flysnapper.vst3'
[IO.File]::WriteAllText($binary, 'corrupt binary')
$failed = $false
try { Install-FlysnapperPackage $package $dest $backup | Out-Null } catch { $failed = $true }
Assert-Test $failed 'Tampered payload was accepted'
Assert-FlysnapperMatchingInventory (Get-FlysnapperInventory $second.Installed) $before
$remaining = @(Get-ChildItem -LiteralPath $dest -Force | Where-Object { $_.Name.StartsWith('.Flysnapper-install-') -or $_.Name.StartsWith('.Flysnapper-rollback-') })
Assert-Test ($remaining.Count -eq 0) 'Failed transaction left a duplicate scan bundle'
Write-Host 'PASS PowerShell installer: Unicode, hashes, first install, replace, backup, unrelated plugin, failed rename rollback, tamper rejection'
''',encoding='utf-8-sig')
        env=os.environ.copy();env.update(FLYSNAPPER_INSTALLER_SOURCE=str(ROOT/'Tools/windows_installer.ps1'),
             FLYSNAPPER_TEST_PACKAGE=str(package),FLYSNAPPER_TEST_DEST=str(destination),FLYSNAPPER_TEST_BACKUP=str(backups))
        if os.name!='nt':
            env.update(XDG_CACHE_HOME=str(self.base/'xdg-cache'),XDG_CONFIG_HOME=str(self.base/'xdg-config'),XDG_DATA_HOME=str(self.base/'xdg-data'))
        command=[shell,'-NoLogo','-NoProfile','-ExecutionPolicy','Bypass']+(['-STA'] if os.name=='nt' else [])+['-File',str(script)]
        result=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,env=env)
        self.assertEqual(result.returncode,0,result.stdout)
        print(result.stdout.strip())

class GitDeliveryTests(unittest.TestCase):
    def test_log_redaction(self):
        text='Authorization: Bearer abc123\nhttps://user:password@example.com ghp_AAAAA github_pat_BBBBB'
        scrubbed=publisher.redact(text)
        for secret in ('abc123','password','ghp_AAAAA','github_pat_BBBBB'):self.assertNotIn(secret,scrubbed)
    def test_running_and_failed_states_append_without_overwriting_previous_payload(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp)/'source';remote=Path(temp)/'remote.git';root.mkdir()
            def git(*args):return subprocess.check_output(['git','-C',str(root),*map(str,args)],stderr=subprocess.STDOUT)
            subprocess.run(['git','init','--quiet','--bare',str(remote)],check=True)
            git('init','--quiet');git('config','user.name','Fixture');git('config','user.email','fixture@example.invalid')
            (root/'source.txt').write_text('source');git('add','source.txt');git('commit','--quiet','-m','fixture source');git('remote','add','origin',remote)
            commit=git('rev-parse','HEAD').decode().strip()
            with mock.patch.dict(os.environ,{'GITHUB_SHA':commit,'GITHUB_RUN_ID':'123'}):
                publisher.publish(root,'running',None,None,None,None)
                first=git('ls-remote','origin',publisher.BRANCH).decode().split()[0]
                log=Path(temp)/'log.txt';log.write_text('fixture failure\nAuthorization: Bearer secret')
                publisher.publish(root,'failed',None,log,None,None)
            git('fetch','--quiet','origin',publisher.BRANCH)
            status=json.loads(git('show','FETCH_HEAD:status.json'))
            self.assertEqual(status['source_commit'],commit);self.assertEqual(status['status'],'failed')
            self.assertEqual(git('show','-s','--format=%P','FETCH_HEAD').decode().strip(),first)
            self.assertNotIn('secret',git('show','FETCH_HEAD:'+status['log']).decode())
            self.assertEqual(git('rev-parse','HEAD').decode().strip(),commit)

if __name__=='__main__':unittest.main()
