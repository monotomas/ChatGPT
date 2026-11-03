# SPDX-License-Identifier: AGPL-3.0-or-later
"""Shipping/transaction regressions. Fake PE bytes are fixtures, not a plugin test.
Actual compiled VST3 validation remains a separate mandatory CTest.
"""
from __future__ import annotations
import hashlib,importlib.util,json,os,shutil,subprocess,sys,tempfile,unittest
from pathlib import Path
from unittest import mock
from zipfile import ZipFile
ROOT=Path(__file__).resolve().parents[1]
def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path);result=importlib.util.module_from_spec(spec);spec.loader.exec_module(result);return result
shipping=module('windows_shipping_test',ROOT/'Tools/package_windows.py')
source=module('source_shipping_test',ROOT/'Tools/package_source.py')
publisher=module('publisher_shipping_test',ROOT/'Tools/publish_windows_delivery.py')

class WindowsPackageTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup)
        self.base=Path(self.temp.name);self.root=self.base/'source';self.root.mkdir()
        for name in source.REQUIRED_FILES:
            f=self.root/name;f.parent.mkdir(parents=True,exist_ok=True);f.write_text('fixture '+name+'\n')
        for name in ('Tools/package_source.py','Tools/windows_installer.ps1','INSTALL_FLYSNAPPER.bat'):
            f=self.root/name;f.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(ROOT/name,f)
        (self.root/'dependencies.lock.json').write_text(json.dumps({'project_version':'1.0.0'}))
        self.dist=self.root/'dist';self.bundle=self.dist/'Flysnapper.vst3'
        p=self.bundle/shipping.BINARY;p.parent.mkdir(parents=True)
        pe=bytearray(512);pe[:2]=b'MZ';pe[60:64]=(128).to_bytes(4,'little');pe[128:134]=b'PE\0\0\x64\x86';p.write_bytes(pe)
        (self.bundle/'Contents/Resources').mkdir();(self.bundle/'Contents/Resources/moduleinfo.json').write_text('{}')
        self.required=['FlysnapperCore','FlysnapperPluginSmoke','FlysnapperActualVst3','FlysnapperProcessorAudio','FlysnapperPitch','FlysnapperAudioQuality']
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
        summary={'gate':'FlysnapperAudioQuality','status':'passed','cases_checked':1,'cases_failed':0,'physical_live_latency_verified':False,'user_5_10ms_goals_verified':False}
        (evidence/'summary.json').write_text(json.dumps(summary))
        (evidence/'metrics.csv').write_text('fixture,not actual audio measurement\n')
        h=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
        self.audio_proof={'status':'passed','evaluator_exit_code':0,'summary_sha256':h(evidence/'summary.json'),'metrics_sha256':h(evidence/'metrics.csv'),
                         'source_sha256':{name:h(self.root/name) for name in audio_sources},
                         'policy_sha256':h(self.root/'Tests/AudioFixtures/quality_gate.json'),
                         'fixture_manifest_sha256':h(self.root/'Tests/AudioFixtures/fixtures.json'),
                         'fixture_inventory_sha256':h(self.root/'Tests/AudioFixtures/fixtures.tsv')}
        (evidence/'report-manifest.json').write_text(json.dumps(self.audio_proof))
    def save_manifest(self):
        (self.dist/'build-manifest.json').write_text(json.dumps(self.manifest))
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
            for n in ('INSTALL_FLYSNAPPER.bat','Install-Flysnapper.ps1','Flysnapper.vst3/'+shipping.BINARY,
                      'Source/Flysnapper-Source.zip','Validation/test-results.xml','Validation/build-manifest.json','Validation/audio-validation/summary.json'):
                self.assertIn(p+n,z.namelist())
            release=json.loads(z.read(p+'release-manifest.json'))
            for n,h in release['files'].items():self.assertEqual(hashlib.sha256(z.read(p+n)).hexdigest(),h)
    def test_linux_failed_or_incomplete_proof_is_rejected(self):
        for key,value in [('platform','Linux-x86_64'),('tests','failed'),('version','0.1.0'),('windows_verified',False),
                          ('toolchain_probe','missing'),('juceaide_unicode_probe','missing'),('pitch_detection_implemented',False),('stage','routing-midi-prototype')]:
            with self.subTest(key=key):
                old=self.manifest[key];self.manifest[key]=value;self.save_manifest()
                with self.assertRaises(ValueError):self.package()
                self.assertFalse(self.output.exists());self.manifest[key]=old
        self.save_manifest()
    def test_required_test_must_exist_and_run_without_skip_or_failure(self):
        for content in ('','<testcase name="FlysnapperAudioQuality"><skipped/></testcase>',
                        '<testcase name="FlysnapperAudioQuality"><failure/></testcase>',
                        '<testcase name="FlysnapperAudioQuality" status="disabled"/>'):
            self.report=''.join(f'<testcase name="{n}"/>' for n in self.required[:-1])+content;self.save_report()
            with self.assertRaises(ValueError):self.package()
    def test_tampered_or_extra_binary_bundle_file_blocks_package(self):
        (self.bundle/shipping.BINARY).write_bytes(b'tampered')
        with self.assertRaises(ValueError):self.package()
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
    def make_git_source(self):
        remote=self.base/'remote.git'
        subprocess.run(['git','init','--quiet','--bare',str(remote)],check=True)
        def git(*args):return subprocess.check_output(['git','-C',str(self.root),*map(str,args)],stderr=subprocess.STDOUT)
        git('init','--quiet');git('config','user.name','Fixture');git('config','user.email','fixture@example.invalid')
        (self.root/'source.txt').write_text('source fixture');git('add','source.txt');git('commit','--quiet','-m','fixture source');git('remote','add','origin',remote)
        return git,git('rev-parse','HEAD').decode().strip()
    def test_complete_publication_and_failed_next_build_preserve_verified_artifact(self):
        git,commit=self.make_git_source();self.manifest['source_commit']=commit;self.save_manifest();self.package()
        with mock.patch.dict(os.environ,{'GITHUB_SHA':commit}),mock.patch.dict(sys.modules,{'package_windows':shipping}):
            publisher.publish(self.root,'completed',self.output,None,self.dist/'build-manifest.json',self.dist/'test-results.xml')
        git('fetch','--quiet','origin',publisher.BRANCH)
        state=json.loads(git('show','FETCH_HEAD:status.json'))
        self.assertEqual(state['status'],'completed');self.assertEqual(state['source_commit'],commit)
        self.assertEqual(state['artifact']['sha256'],hashlib.sha256(self.output.read_bytes()).hexdigest())
        delivered=git('show','FETCH_HEAD:downloads/Flysnapper-Windows-x64.zip');self.assertEqual(delivered,self.output.read_bytes())
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
