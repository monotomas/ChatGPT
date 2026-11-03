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

def redact(text: str) -> str:
    text=re.sub(r'(?i)(authorization\s*[:=]\s*)(?:bearer\s+|basic\s+)?[^\s]+',r'\1[REDACTED]',text)
    text=re.sub(r'\b(?:github_pat_[A-Za-z0-9_]+|gh[pousr]_[A-Za-z0-9_]+)\b','[REDACTED]',text)
    text=re.sub(r'(https?://)[^/@\s]+:[^/@\s]+@',r'\1[REDACTED]@',text)
    return text

def publish(root: Path,result: str,artifact: Path | None,log: Path | None,build_manifest: Path | None,report: Path | None,error: str | None = None):
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
    if error:status['error']=redact(error)
    members={}
    if log is not None and log.is_file():
        contents=redact(log.read_text(encoding='utf-8',errors='replace'))
        # Preserve beginning/tool versions and latest diagnostics when exceptionally large.
        if len(contents)>2_000_000:contents=contents[:250_000]+'\n[BOUNDED CI LOG: middle omitted]\n'+contents[-1_750_000:]
        name=f'logs/windows-{source}.txt';members[name]=contents.encode();status['log']=name
    if result=='completed':
        from package_windows import verify_package
        verify_package(artifact)
        if build_manifest is None or report is None or not build_manifest.is_file() or not report.is_file():
            raise ValueError('Windows validation proof is missing')
        build=json.loads(build_manifest.read_text(encoding='utf-8'))
        if build.get('source_commit')!=source:
            raise ValueError('Windows artifact proof belongs to a different source commit')
        with ZipFile(artifact) as archive:
            prefix=archive.namelist()[0].split('/',1)[0]+'/'
            embedded=json.loads(archive.read(prefix+'Validation/build-manifest.json'))
            if embedded!=build or archive.read(prefix+'Validation/test-results.xml')!=report.read_bytes():
                raise ValueError('External CI proof differs from packaged proof')
        name='downloads/Flysnapper-Windows-x64.zip';data=artifact.read_bytes();members[name]=data
        digest=hashlib.sha256(data).hexdigest()
        members['downloads/SHA256SUMS.txt']=f'{digest}  Flysnapper-Windows-x64.zip\n'.encode()
        status['artifact']={'path':name,'sha256':digest,'bytes':len(data)}
        for path,name in ((build_manifest,'validation/windows-build-manifest.json'),(report,'validation/windows-test-results.xml')):
            if path is None or not path.is_file():raise ValueError('Windows validation proof is missing')
            members[name]=path.read_bytes()
        members['README.md']=('''# Flysnapper Windows x64\n\n[Last ned ferdig installasjonspakke](downloads/Flysnapper-Windows-x64.zip?raw=1)\n\nPakk ut hele ZIP-filen og dobbeltklikk INSTALL_FLYSNAPPER.bat. Ingen Python, CMake eller Visual Studio kreves.\n\nstatus.json identifiserer kildecommit og den faktiske Windows-byggingen. Native automatiske tester og SHA256 er vedlagt. Målte audioegenskaper og eventuelle gjenværende begrensninger står i pakken; Windows-bygg er ikke i seg selv en Ableton-/lydkortmåling.\n''').encode()
    members['status.json']=(json.dumps(status,indent=2,ensure_ascii=False)+'\n').encode()
    for attempt in range(3):
        remote=git('ls-remote','origin',BRANCH).stdout.decode().strip()
        parent=remote.split()[0] if remote else None
        if parent:git('fetch','--quiet','--depth','1','origin',BRANCH)
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
            pushed=git('push','origin',f'{commit}:{BRANCH}',check=False)
            if pushed.returncode==0:
                print('Published Windows delivery state:',result,commit);return
            if attempt==2:raise RuntimeError('Could not append CI delivery state: '+redact(pushed.stderr.decode(errors='replace')))
    raise RuntimeError('Delivery publication did not complete')

def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    p.add_argument('--result',choices=('running','completed','failed'),required=True)
    p.add_argument('--artifact',type=Path);p.add_argument('--log',type=Path);p.add_argument('--build-manifest',type=Path);p.add_argument('--test-results',type=Path)
    a=p.parse_args(argv)
    try:publish(a.root,a.result,a.artifact,a.log,a.build_manifest,a.test_results)
    except (ValueError,OSError,RuntimeError,subprocess.CalledProcessError) as e:
        message=redact(str(e))
        if a.result=='completed':
            try:publish(a.root,'failed',None,a.log,None,None,error='Artifact verification failed: '+message)
            except (ValueError,OSError,RuntimeError,subprocess.CalledProcessError) as secondary:
                message+='; failure-state publication also failed: '+redact(str(secondary))
        p.exit(1,'Delivery publication failed: '+message+'\n')
    return 0
if __name__=='__main__':raise SystemExit(main())
