#!/usr/bin/env python3
"""Publish CI proof and ready-to-install ZIP through Git, without an API client.
SPDX-License-Identifier: AGPL-3.0-or-later
The checked-out GitHub Actions credential is used normally by native Git.
"""
from __future__ import annotations
import argparse,datetime as dt,hashlib,json,os,re,subprocess,tempfile
from pathlib import Path
from zipfile import ZipFile

BRANCH='refs/heads/downloads/flysnapper-complete'
RESEARCH_BRANCH='refs/heads/validation/midi-only-ci'

def redact(text: str) -> str:
    text=re.sub(r'(?i)(authorization\s*[:=]\s*)(?:bearer\s+|basic\s+)?[^\s]+',r'\1[REDACTED]',text)
    text=re.sub(r'\b(?:github_pat_[A-Za-z0-9_]+|gh[pousr]_[A-Za-z0-9_]+)\b','[REDACTED]',text)
    text=re.sub(r'(https?://)[^/@\s]+:[^/@\s]+@',r'\1[REDACTED]@',text)
    return text

def installed_package_proof(path: Path | None,source: str,artifact_sha: str,build: dict,launcher_sha: str | None = None):
    if path is None or not path.is_file():raise ValueError('Actual Windows installation validation proof is missing')
    proof=json.loads(path.read_text(encoding='utf-8'))
    if proof.get('schema')!=1 or proof.get('status')!='passed' or proof.get('execution_platform')!='Windows':
        raise ValueError('Actual Windows installation validation did not pass')
    if proof.get('source_commit')!=source or proof.get('artifact_sha256')!=artifact_sha:
        raise ValueError('Installation proof belongs to a different source or ZIP')
    if proof.get('installer_exit_code')!=0 or proof.get('installed_host_exit_code')!=0 or not isinstance(proof.get('host_assertions'),int) or proof['host_assertions']<=0:
        raise ValueError('Actual installer or installed-module host test failed')
    if proof.get('installed_files_sha256')!=build.get('bundle_sha256') or proof.get('recovery_files_sha256')!=build.get('bundle_sha256'):
        raise ValueError('Installed/recovery bundle hashes differ from the Windows build')
    if proof.get('unicode_paths_verified') is not True or proof.get('unrelated_plugin_preserved') is not True:
        raise ValueError('Installation isolation and Unicode checks are incomplete')
    if build.get('version')=='0.4.0':
        if proof.get('bundle_name')!='Flysnapper MIDI.vst3' or proof.get('legacy_instrument_preserved') is not True:
            raise ValueError('MIDI-only installation identity/legacy preservation proof is missing')
    launcher_required=build.get('version')!='0.2.0' or proof.get('bat_launcher_verified') is True
    logs=[('installation-powershell.txt','powershell_log_sha256'),('installed-vst3-host.txt','host_log_sha256')]
    if launcher_required:
        if proof.get('bat_launcher_verified') is not True or proof.get('bat_launcher_exit_code')!=0:
            raise ValueError('Actual packaged BAT launcher did not pass')
        recorded_sha=proof.get('bat_launcher_sha256')
        if not isinstance(recorded_sha,str) or not re.fullmatch('[0-9a-f]{64}',recorded_sha) or launcher_sha is not None and recorded_sha!=launcher_sha:
            raise ValueError('BAT launcher hash differs from the verified package')
        marker='Installed Flysnapper '+str(build.get('version'))+':'
        if proof.get('bat_launcher_marker')!=marker or proof.get('launcher_recovery_files_sha256')!=build.get('bundle_sha256'):
            raise ValueError('BAT replacement/recovery proof is incomplete')
        logs.append(('installation-launcher.txt','launcher_log_sha256'))
    members={'validation/windows-installation.json':path.read_bytes()}
    for filename,key in logs:
        log=path.parent/filename
        if not log.is_file() or hashlib.sha256(log.read_bytes()).hexdigest()!=proof.get(key):raise ValueError('Installed-module verification log is missing or changed')
        members['validation/'+filename]=log.read_bytes()
    if launcher_required and marker.encode('ascii') not in (path.parent/'installation-launcher.txt').read_bytes():
        raise ValueError('BAT launcher completion marker is missing')
    text=(path.parent/'installed-vst3-host.txt').read_text(encoding='utf-8',errors='replace')
    count=re.search(r'RESULT actual VST3 host tests passed \((\d+) assertions\)\.',text)
    if not count or int(count.group(1))!=proof['host_assertions'] or 'PLATFORM VERIFIED: Windows x64 compiled VST3 loaded and executed by native test host.' not in text:
        raise ValueError('Installed VST3 host completion proof is inconsistent')
    return members

def publish(root: Path,result: str,artifact: Path | None,log: Path | None,build_manifest: Path | None,report: Path | None,error: str | None = None,installation_proof: Path | None = None,research: bool = False):
    branch=RESEARCH_BRANCH if research else BRANCH
    def git(*args,input=None,env=None,check=True):
        return subprocess.run(['git','-C',str(root),*map(str,args)],input=input,stdout=subprocess.PIPE,stderr=subprocess.PIPE,env=env,check=check)
    source=git('rev-parse','HEAD').stdout.decode().strip()
    context_sha=os.environ.get('GITHUB_SHA',source)
    if source!=context_sha:raise ValueError('Checked-out source differs from CI source context')
    if not re.fullmatch(r'[0-9a-f]{40}',source):raise ValueError('Invalid source SHA')
    if result=='completed' and (artifact is None or not artifact.is_file()):raise ValueError('Passed Windows artifact is missing')
    status={'schema':1,'source_commit':source,'status':result,'run_id':os.environ.get('GITHUB_RUN_ID'),
            'run_attempt':os.environ.get('GITHUB_RUN_ATTEMPT'), 'repository':os.environ.get('GITHUB_REPOSITORY'),
            'updated_at':dt.datetime.now(dt.timezone.utc).isoformat(),'artifact':None}
    if research:status.update(publication_kind='research-checkpoint',ableton_verified=False)
    if error:status['error']=redact(error)
    members={}
    if log is not None and log.is_file():
        contents=redact(log.read_text(encoding='utf-8',errors='replace'))
        # Preserve beginning/tool versions and latest diagnostics when exceptionally large.
        if len(contents)>2_000_000:contents=contents[:250_000]+'\n[BOUNDED CI LOG: middle omitted]\n'+contents[-1_750_000:]
        name=f'logs/windows-{source}.txt';members[name]=contents.encode();status['log']=name
    if result=='failed' and installation_proof is not None and installation_proof.is_file():
        name=f'validation/windows-installation-failed-{source}.json'
        members[name]=installation_proof.read_bytes()
        status['installation_validation']={'path':name,'status':'failed'}
        for filename in ('installation-powershell.txt','installed-vst3-host.txt','installation-launcher.txt'):
            diagnostic=installation_proof.parent/filename
            if diagnostic.is_file():
                contents=redact(diagnostic.read_text(encoding='utf-8',errors='replace'))
                members[f'logs/{source}-{filename}']=contents[-2_000_000:].encode('utf-8')
    if result=='completed':
        from package_windows import verify_package,dynamics_evidence_module,midi_evidence_module
        verify_package(artifact)
        if build_manifest is None or report is None or not build_manifest.is_file() or not report.is_file():
            raise ValueError('Windows validation proof is missing')
        build=json.loads(build_manifest.read_text(encoding='utf-8'))
        if build.get('source_commit')!=source:
            raise ValueError('Windows artifact proof belongs to a different source commit')
        if build.get('version')=='0.3.0':
            dynamics=dynamics_evidence_module()
            if build.get('stage')!=dynamics.EVALUATION_STAGE or build.get('evaluation_scope')!=dynamics.EVALUATION_SCOPE:
                raise ValueError('Flysnapper 0.3.0 is missing its qualified real-guitar evaluation scope')
        if build.get('version')=='0.4.0':
            midi=midi_evidence_module()
            if (build.get('stage')!=midi.STAGE or build.get('evaluation_scope')!=midi.SCOPE
                    or build.get('bundle_name')!=midi.BUNDLE or build.get('pitch_detection_implemented') is not False):
                raise ValueError('Flysnapper MIDI is missing its native MIDI-only validation scope')
        status['stage']=build.get('stage');status['evaluation_scope']=build.get('evaluation_scope')
        with ZipFile(artifact) as archive:
            prefix=archive.namelist()[0].split('/',1)[0]+'/'
            embedded=json.loads(archive.read(prefix+'Validation/build-manifest.json'))
            if embedded!=build or archive.read(prefix+'Validation/test-results.xml')!=report.read_bytes():
                raise ValueError('External CI proof differs from packaged proof')
            launcher_sha=hashlib.sha256(archive.read(prefix+'INSTALL_FLYSNAPPER.bat')).hexdigest()
        name='artifacts/Flysnapper-MIDI-Windows-x64.zip' if research else 'downloads/Flysnapper-Windows-x64.zip'
        data=artifact.read_bytes();members[name]=data
        digest=hashlib.sha256(data).hexdigest()
        members.update(installed_package_proof(installation_proof,source,digest,build,launcher_sha))
        status['installation_validation']={'path':'validation/windows-installation.json','sha256':hashlib.sha256(installation_proof.read_bytes()).hexdigest()}
        checksum_path='artifacts/SHA256SUMS.txt' if research else 'downloads/SHA256SUMS.txt'
        members[checksum_path]=f'{digest}  {Path(name).name}\n'.encode()
        status['artifact']={'path':name,'sha256':digest,'bytes':len(data)}
        for path,name in ((build_manifest,'validation/windows-build-manifest.json'),(report,'validation/windows-test-results.xml')):
            if path is None or not path.is_file():raise ValueError('Windows validation proof is missing')
            members[name]=path.read_bytes()
        members['README.md']=('''# Flysnapper Windows x64\n\n[Last ned ferdig installasjonspakke](downloads/Flysnapper-Windows-x64.zip?raw=1)\n\nPakk ut hele ZIP-filen og dobbeltklikk INSTALL_FLYSNAPPER.bat. Ingen Python, CMake eller Visual Studio kreves.\n\nstatus.json identifiserer kildecommit og den faktiske Windows-byggingen. Native automatiske tester og SHA256 er vedlagt. Målte audioegenskaper og eventuelle gjenværende begrensninger står i pakken; Windows-bygg er ikke i seg selv en Ableton-/lydkortmåling.\n''').encode()
        if build.get('version')=='0.3.0':
            text=members['README.md'].decode('utf-8').replace('[Last ned ferdig installasjonspakke]',
                '[Last ned Flysnapper 0.3.0 for gitarprøve]')
            text+='\nDenne leveransen er klargjort for praktisk gitarprøve etter de automatiske kontrollene for kravene av 5. november. Poly sender definerte halvtoner uten polyfonisk bending; mono har fortsatt valgfri bending. Begynn med den veiledede 20-sekunders spilleprøven i Input dynamics og juster kontrollene ved behov. De målte begrensningene følger pakken.\n'
            members['README.md']=text.encode('utf-8')
        if build.get('version')=='0.4.0':
            members['README.md']=('''# Flysnapper MIDI Windows x64

[Last ned MIDI-installasjonspakken](downloads/Flysnapper-Windows-x64.zip?raw=1)

Pakk ut hele ZIP-filen og kjor INSTALL_FLYSNAPPER.bat. Ingen kompilator eller Python kreves.
Den nye Flysnapper MIDI.vst3 har egen komponentidentitet og erstatter ikke den eldre Flysnapper.vst3.

status.json identifiserer kildecommit, Windows-bygg, native MIDI-/VST3-tester og faktisk installasjon/erstatning/gjenoppretting.
Dette dokumenterer testverten og installerpakken; en praktisk Ableton Live-test er ikke utfort av CI.
''').encode('utf-8')
        if research:
            with ZipFile(artifact) as archive:
                prefix=archive.namelist()[0].split('/',1)[0]+'/'
                for member in archive.namelist():
                    if member.startswith(prefix+'Validation/'):
                        members['validation/package/'+member[len(prefix+'Validation/'):]]=archive.read(member)
            if build.get('version')=='0.4.0':
                proof=json.loads(members['validation/package/midi-validation/report-manifest.json'])
                omitted={'schema':1,'kind':'OmittedWindowsTestExecutables','source_commit':source,
                         'scope':'Native executables rehashed in CI before packaging, intentionally omitted from research Git; offline audits cannot independently rehash omitted binaries',
                         'source_proof':'validation/package/midi-validation/report-manifest.json',
                         'executables':[]}
                for test,execution in sorted(proof['executions'].items()):
                    omitted['executables'].append({'role':test,'compiled_path':execution['binary'],
                        'sha256':execution['binary_sha256'],'bytes':execution['binary_bytes'],
                        'report_sha256':execution['report_sha256'],'events_sha256':execution['events_sha256'],
                        'source_binding':'source_sha256 in '+omitted['source_proof']})
                data=(json.dumps(omitted,indent=2)+'\n').encode('utf-8')
                members['validation/omitted-executables.json']=data
                status['omitted_executables']={'path':'validation/omitted-executables.json','sha256':hashlib.sha256(data).hexdigest()}
            members['README.md']=('''# Flysnapper MIDI — internal Windows research checkpoint

This branch is not a user release. The package and validation records are retained for source/module/installer review.
Read status.json for the exact source commit and current run state. Prior artifacts may remain after a failed later run; do not confuse them with the current state.
Native Windows and compiled VST3-host checks do not certify Ableton Live12.4.6 routing or MIDI-effect placement. ableton_verified remains false.
Official user delivery remains on the separate downloads/flysnapper-complete branch only after explicit release promotion.
''').encode('utf-8')
    members['status.json']=(json.dumps(status,indent=2,ensure_ascii=False)+'\n').encode()
    for attempt in range(3):
        remote=git('ls-remote','origin',branch).stdout.decode().strip()
        parent=remote.split()[0] if remote else None
        if parent:git('fetch','--quiet','--depth','1','origin',branch)
        with tempfile.TemporaryDirectory(prefix='flysnapper-ci-index-') as temp:
            env=os.environ.copy();env['GIT_INDEX_FILE']=str(Path(temp)/'index')
            if parent:git('read-tree',parent,env=env)
            else:git('read-tree','--empty',env=env)
            for name,data in sorted(members.items()):
                blob=git('hash-object','-w','--stdin',input=data,env=env).stdout.decode().strip()
                git('update-index','--add','--cacheinfo',f'100644,{blob},{name}',env=env)
            tree=git('write-tree',env=env).stdout.decode().strip()
            env.update(GIT_AUTHOR_NAME='Flysnapper Windows builder',GIT_AUTHOR_EMAIL='41898282+github-actions[bot]@users.noreply.github.com',
                       GIT_COMMITTER_NAME='Flysnapper Windows builder',GIT_COMMITTER_EMAIL='41898282+github-actions[bot]@users.noreply.github.com')
            args=['commit-tree',tree]+(['-p',parent] if parent else [])
            commit=git(*args,input=f'Windows {result}: {source}\n'.encode(),env=env).stdout.decode().strip()
            pushed=git('push','origin',f'{commit}:{branch}',check=False)
            if pushed.returncode==0:
                print('Published Windows '+('research' if research else 'delivery')+' state:',result,commit);return
            if attempt==2:raise RuntimeError('Could not append CI delivery state: '+redact(pushed.stderr.decode(errors='replace')))
    raise RuntimeError('Delivery publication did not complete')

def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    p.add_argument('--result',choices=('running','completed','failed'),required=True)
    p.add_argument('--artifact',type=Path);p.add_argument('--log',type=Path);p.add_argument('--build-manifest',type=Path);p.add_argument('--test-results',type=Path);p.add_argument('--installation-proof',type=Path)
    p.add_argument('--research',action='store_true',help='Save only to the isolated validation/midi-only-ci branch; never the user delivery branch')
    a=p.parse_args(argv)
    try:publish(a.root,a.result,a.artifact,a.log,a.build_manifest,a.test_results,installation_proof=a.installation_proof,research=a.research)
    except (ValueError,OSError,RuntimeError,subprocess.CalledProcessError) as e:
        message=redact(str(e))
        if a.result=='completed':
            try:publish(a.root,'failed',None,a.log,None,None,error='Artifact verification failed: '+message,installation_proof=a.installation_proof,research=a.research)
            except (ValueError,OSError,RuntimeError,subprocess.CalledProcessError) as secondary:
                message+='; failure-state publication also failed: '+redact(str(secondary))
        p.exit(1,'Delivery publication failed: '+message+'\n')
    return 0
if __name__=='__main__':raise SystemExit(main())
