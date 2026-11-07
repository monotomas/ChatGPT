# SPDX-License-Identifier: AGPL-3.0-or-later
"""MIDI-only package/transaction protocol tests; fake PE is not a native plugin."""
from pathlib import Path
import contextlib,hashlib,importlib.util,io,json,os,shutil,subprocess,sys,tempfile,unittest
from unittest import mock
from zipfile import ZipFile
from Tools import midi_only_evidence as evidence
from Tools import package_windows as shipping
from Tools import publish_windows_delivery as publisher
ROOT=Path(__file__).resolve().parents[1]
def module(name,path):
 s=importlib.util.spec_from_file_location(name,path);m=importlib.util.module_from_spec(s);s.loader.exec_module(m);return m
fixture=module('midi_fixture_for_shipping',Path(__file__).with_name('midi_only_fixture.py'))
with mock.patch.dict(sys.modules,{'package_windows':shipping}):verifier=module('midi_installed_verifier',ROOT/'Tools/verify_windows_install.py')

class MidiWindowsPackageTests(unittest.TestCase):
 def setUp(self):
  t=tempfile.TemporaryDirectory();self.addCleanup(t.cleanup);self.base=Path(t.name);self.f=fixture.make(self.base,ROOT)
 def package(self):return shipping.build_package(self.f.root,self.f.dist,self.f.output)
 def extract(self):
  self.package();out=self.base/'Olav Lægreid (MIDI) & source'
  with ZipFile(self.f.output) as z:z.extractall(out)
  return next(out.iterdir())
 def test_midi_package_uses_new_identity_and_only_relevant_evidence(self):
  self.package();shipping.verify_package(self.f.output)
  with ZipFile(self.f.output) as z:
   prefix='Flysnapper-0.4.2-Windows-x64/'
   self.assertIn(prefix+evidence.BUNDLE+'/'+evidence.WINDOWS_BINARY,z.namelist())
   self.assertNotIn(prefix+'Flysnapper.vst3/'+shipping.BINARY,z.namelist())
   self.assertIn(prefix+'Validation/midi-validation/report-manifest.json',z.namelist())
   self.assertFalse(any('audio-validation/' in n or 'input-dynamics/' in n or n.endswith('INPUT_DYNAMICS.md') for n in z.namelist()))
   text=z.read(prefix+'LES_MEG.txt').decode('utf-8-sig');self.assertIn('VST3-instrument',text);self.assertNotIn('ingen audiobusser',text);self.assertNotIn('Learn / 20 seconds',text)
   release=json.loads(z.read(prefix+'release-manifest.json'));self.assertEqual(release['bundle_name'],evidence.BUNDLE);self.assertEqual(release['version'],'0.4.2')
   self.assertEqual(release['evaluation_scope']['chord_types'],52);self.assertEqual(release['evaluation_scope']['scales'],38)
   info=json.loads(z.read(prefix+evidence.BUNDLE+'/Contents/Resources/moduleinfo.json'))
   self.assertEqual(info['Classes'][0]['Version'],'0.4.2');self.assertEqual(info['Classes'][0]['Sub Categories'],['Instrument'])
 def test_legacy_bundle_audio_or_false_ableton_claim_cannot_masquerade_as_midi(self):
  original=self.f.manifest.copy()
  for key,value in [('version','0.4.0'),('version','0.4.1'),('bundle_name','Flysnapper.vst3'),('product','Flysnapper'),('pitch_detection_implemented',True),('audio_processing_implemented',True),('ableton_verified',True),('stage','complete'),('evaluation_scope',{**evidence.SCOPE,'chord_types':50}),('evaluation_scope',{**evidence.SCOPE,'scales':31}),('evaluation_scope',{**evidence.SCOPE,'host_device_role':'effect'}),('evaluation_scope',{**evidence.SCOPE,'silent_audio_output':False})]:
   with self.subTest(key=key):
    self.f.manifest={**original,key:value};fixture.save(self.f)
    with self.assertRaises(ValueError):self.package()
  self.f.manifest=original;fixture.save(self.f)
 def test_all_four_midi_tests_are_required_and_must_finish(self):
  for missing in sorted(evidence.REQUIRED_TESTS):
   with self.subTest(missing=missing):
    self.f.manifest['required_test_names']=sorted(evidence.REQUIRED_TESTS-{missing});fixture.save(self.f)
    with self.assertRaisesRegex(ValueError,'required validation'):self.package()
  self.f.manifest['required_test_names']=sorted(evidence.REQUIRED_TESTS);fixture.save(self.f)
  p=self.f.dist/'test-results.xml';p.write_text(p.read_text().replace('name="FlysnapperCore" status="run"','name="FlysnapperCore" status="disabled"'))
  with self.assertRaises(ValueError):self.package()
 def test_missing_or_rebound_invalid_native_report_cannot_ship(self):
  p=self.f.dist/evidence.DIRECTORY/evidence.SPECS[1][2];r=json.loads(p.read_text());r['audio_output_channels']=0;p.write_text(json.dumps(r))
  proof=json.loads((self.f.dist/evidence.DIRECTORY/evidence.MANIFEST).read_text());proof['executions'][evidence.SPECS[1][0]]['report_sha256']=evidence.sha(p.read_bytes());fixture.rebind_proof(self.f,proof)
  with self.assertRaises(ValueError):self.package()
 def test_actual_powershell_entrypoint_replaces_only_midi_and_preserves_old_instrument(self):
  shell=shutil.which('powershell.exe') if os.name=='nt' else shutil.which('pwsh')
  if not shell and Path('/workspace/.flysnapper-tools/powershell/pwsh').is_file():shell='/workspace/.flysnapper-tools/powershell/pwsh'
  if not shell:self.skipTest('PowerShell unavailable; Windows CI must exercise native PowerShell5')
  package=self.extract();destination=self.base/'Olav Lægreid (test) & routing/VST3';backup=self.base/'Olav Lægreid (test) & routing/Recovery'
  old=destination/'Flysnapper.vst3/keep-legacy.txt';old.parent.mkdir(parents=True);old.write_text('old instrument remains untouched')
  other=destination/'Other.vst3/keep.txt';other.parent.mkdir();other.write_text('unrelated plugin')
  env=os.environ.copy()
  if os.name!='nt':env.update(XDG_CACHE_HOME=str(self.base/'cache'),XDG_CONFIG_HOME=str(self.base/'config'),XDG_DATA_HOME=str(self.base/'data'))
  command=[shell,'-NoLogo','-NoProfile','-ExecutionPolicy','Bypass']+(['-STA'] if os.name=='nt' else [])+['-File',str(package/'Install-Flysnapper.ps1'),'-NoUI','-Destination',str(destination),'-BackupDirectory',str(backup)]
  for i in range(2):
   result=subprocess.run(command,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)
   self.assertEqual(result.returncode,0,result.stdout.decode('utf-8',errors='replace'))
   self.assertEqual(old.read_text(),'old instrument remains untouched');self.assertEqual(other.read_text(),'unrelated plugin')
  copies=list(backup.glob('*/Flysnapper MIDI.vst3'));self.assertEqual(len(copies),1)
  for path in (copies[0],destination/evidence.BUNDLE):
   self.assertEqual({n:hashlib.sha256(b).hexdigest() for n,b in shipping.files_below(path).items()},self.f.manifest['bundle_sha256'])
 def test_windows_bat_launcher_installs_the_separate_midi_bundle(self):
  if os.name!='nt':self.skipTest('Native cmd.exe/WindowsPowerShell5 required; portable PS test is separate')
  package=self.extract();dest=self.base/'Olav Lægreid (BAT) & MIDI/VST3';backup=self.base/'Olav Lægreid (BAT) & MIDI/Recovery'
  old=dest/'Flysnapper.vst3/keep.txt';old.parent.mkdir(parents=True);old.write_text('old instrument')
  cmd=Path(os.environ.get('SystemRoot','C:/Windows'))/'System32/cmd.exe'
  command=verifier.launcher_command(cmd,package/'INSTALL_FLYSNAPPER.bat',dest,backup)
  for i in range(2):
   r=subprocess.run(command,executable=str(cmd),cwd=package,stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=120)
   self.assertEqual(r.returncode,0,r.stdout.decode('utf-8',errors='replace'));self.assertEqual(old.read_text(),'old instrument')
  self.assertEqual(len(list(backup.glob('*/Flysnapper MIDI.vst3'))),1)
 def test_source_or_native_binary_mutation_blocks_midi_packaging(self):
  (self.f.root/'Source/PluginProcessor.cpp').write_text('changed after native tests')
  with self.assertRaisesRegex(ValueError,'source differs'):self.package()
 def test_midi_release_zip_is_deterministic(self):
  self.package();first=self.f.output.read_bytes();self.package();self.assertEqual(first,self.f.output.read_bytes())
 def test_workflow_research_artifacts_do_not_publish_a_user_release(self):
  text=(ROOT/'.github/workflows/windows.yml').read_text()
  self.assertIn('branches: [release/full-flysnapper, research/midi-only]',text)
  self.assertEqual(text.count("$env:GITHUB_REF -eq 'refs/heads/release/full-flysnapper'"),2)
  self.assertEqual(text.count("@('--research')"),2)
  self.assertIn('--result running @publication',text)
  self.assertIn('--installation-proof artifacts/installation-validation.json @publication',text)
  self.assertNotIn('python Tools/windows_audio_diagnostics.py',text)

 def test_actual_workflow_powershell_splat_preserves_argument_boundaries(self):
  shell=shutil.which('pwsh') or shutil.which('powershell.exe')
  portable=Path('/workspace/.flysnapper-tools/powershell/pwsh')
  if not shell and portable.is_file():shell=str(portable)
  if not shell:self.skipTest('PowerShell unavailable; Windows CI executes its workflow shell')
  # Execute both production selection statements in PowerShell. A one-item
  # pipeline result otherwise becomes a string and splats --research as chars.
  lines=[line.strip() for line in (ROOT/'.github/workflows/windows.yml').read_text().splitlines()
         if '$publication =' in line]
  self.assertEqual(len(lines),2)
  arguments=self.base/'workflow-arguments.py'
  arguments.write_text('import json,sys; print(json.dumps(sys.argv[1:]))\n',encoding='utf-8')
  env=os.environ.copy();env.update(FLYSNAPPER_WORKFLOW_PYTHON=sys.executable,FLYSNAPPER_WORKFLOW_ARGUMENTS=str(arguments))
  if os.name!='nt':env.update(XDG_CACHE_HOME=str(self.base/'cache'),XDG_CONFIG_HOME=str(self.base/'config'),XDG_DATA_HOME=str(self.base/'data'))
  for index,line in enumerate(lines):
   script=self.base/('workflow-splat-'+str(index)+'.ps1')
   script.write_text(line+'\n& $env:FLYSNAPPER_WORKFLOW_PYTHON $env:FLYSNAPPER_WORKFLOW_ARGUMENTS --result running @publication\nexit $LASTEXITCODE\n',encoding='utf-8-sig')
   for branch,expected in [('refs/heads/research/midi-only',['--result','running','--research']),
                           ('refs/heads/release/full-flysnapper',['--result','running'])]:
    with self.subTest(statement=index,branch=branch):
     env['GITHUB_REF']=branch
     result=subprocess.run([shell,'-NoLogo','-NoProfile','-File',str(script)],env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,timeout=30)
     self.assertEqual(result.returncode,0,result.stdout.decode('utf-8',errors='replace'))
     self.assertEqual(json.loads(result.stdout.decode('utf-8-sig')),expected)

 def test_research_git_checkpoint_never_changes_user_delivery_branch(self):
  remote=self.base/'remote.git';subprocess.run(['git','init','--quiet','--bare',str(remote)],check=True)
  def git(*args):return subprocess.check_output(['git','-C',str(self.f.root),*map(str,args)],stderr=subprocess.STDOUT)
  git('init','--quiet');git('config','user.name','Fixture');git('config','user.email','fixture@example.invalid')
  (self.f.root/'source-fixture.txt').write_text('research publication fixture');git('add','source-fixture.txt');git('commit','--quiet','-m','source fixture');git('remote','add','origin',remote)
  source=git('rev-parse','HEAD').decode().strip();git('push','--quiet','origin','HEAD:'+publisher.BRANCH)
  with mock.patch.dict(os.environ,{'GITHUB_SHA':source}),contextlib.redirect_stdout(io.StringIO()):
   publisher.publish(self.f.root,'running',None,None,None,None,research=True)
   publisher.publish(self.f.root,'failed',None,None,None,None,error='fixture failure',research=True)
  actual=subprocess.check_output(['git','--git-dir',str(remote),'rev-parse',publisher.BRANCH],text=True).strip();self.assertEqual(actual,source)
  status=json.loads(subprocess.check_output(['git','--git-dir',str(remote),'show',publisher.RESEARCH_BRANCH+':status.json']))
  self.assertEqual(status['publication_kind'],'research-checkpoint');self.assertEqual(status['status'],'failed');self.assertIs(status['ableton_verified'],False)
 def test_completed_research_checkpoint_retains_proof_but_omits_native_executables(self):
  remote=self.base/'research-remote.git';subprocess.run(['git','init','--quiet','--bare',str(remote)],check=True)
  def git(*args):return subprocess.check_output(['git','-C',str(self.f.root),*map(str,args)],stderr=subprocess.STDOUT)
  git('init','--quiet');git('config','user.name','Fixture');git('config','user.email','fixture@example.invalid')
  (self.f.root/'source-fixture.txt').write_text('research source fixture');git('add','source-fixture.txt');git('commit','--quiet','-m','research source fixture');git('remote','add','origin',remote)
  source=git('rev-parse','HEAD').decode().strip();self.f.manifest['source_commit']=source
  proof=json.loads((self.f.dist/evidence.DIRECTORY/evidence.MANIFEST).read_text());proof['source_commit']=source;fixture.rebind_proof(self.f,proof)
  self.package();install=fixture.installation_proof(self.f,self.base,source)
  with mock.patch.dict(sys.modules,{'package_windows':shipping}),mock.patch.dict(os.environ,{'GITHUB_SHA':source}),contextlib.redirect_stdout(io.StringIO()):
   publisher.publish(self.f.root,'completed',self.f.output,None,self.f.dist/'build-manifest.json',self.f.dist/'test-results.xml',installation_proof=install,research=True)
  def read(name):return subprocess.check_output(['git','--git-dir',str(remote),'show',publisher.RESEARCH_BRANCH+':'+name])
  status=json.loads(read('status.json'));self.assertEqual(status['status'],'completed');self.assertEqual(status['publication_kind'],'research-checkpoint')
  self.assertEqual(status['artifact']['path'],'artifacts/Flysnapper-MIDI-Windows-x64.zip')
  self.assertEqual(read(status['artifact']['path']),self.f.output.read_bytes())
  omitted=json.loads(read('validation/omitted-executables.json'));self.assertEqual(len(omitted['executables']),2)
  self.assertTrue(all(x['bytes']>64 and len(x['sha256'])==64 for x in omitted['executables']))
  files=subprocess.check_output(['git','--git-dir',str(remote),'ls-tree','-r','--name-only',publisher.RESEARCH_BRANCH],text=True).splitlines()
  self.assertFalse(any(x.endswith('.exe') for x in files));self.assertIn('validation/package/midi-validation/report-manifest.json',files)
  self.assertNotEqual(subprocess.run(['git','--git-dir',str(remote),'show-ref','--verify',publisher.BRANCH],capture_output=True).returncode,0)

 def test_research_cli_failure_fallback_stays_on_research_branch(self):
  with mock.patch.object(publisher,'publish',side_effect=[ValueError('fixture invalid artifact'),None]) as calls,contextlib.redirect_stderr(io.StringIO()):
   with self.assertRaises(SystemExit):publisher.main(['--result','completed','--research'])
  self.assertEqual(len(calls.call_args_list),2)
  self.assertTrue(all(call.kwargs.get('research') is True for call in calls.call_args_list))

if __name__=='__main__':unittest.main()
