#!/usr/bin/env python3
"""Run independently built, pinned Tracktion pluginval against a compiled VST3.
SPDX-License-Identifier: AGPL-3.0-or-later
This validates host/API stability, not Ableton or musical pitch accuracy.
"""
from __future__ import annotations
import argparse,datetime as dt,hashlib,json,os,re,shutil,signal,socket,subprocess,time
from pathlib import Path
PLUGINVAL_COMMIT='ed19c2c16b57a6d94db391bea3ef4a80b769d5bf'
PLUGINVAL_JUCE_COMMIT='5179f4e720d8406ebd1b5401c86aea8da6cc83c9'

def digest(path: Path):return hashlib.sha256(path.read_bytes()).hexdigest()
def verify_source(root: Path):
    def git(path,*args):return subprocess.check_output(['git','-C',str(path),*args],text=True).strip()
    if git(root,'rev-parse','HEAD')!=PLUGINVAL_COMMIT or git(root,'status','--porcelain'):
        raise ValueError('Expected unchanged official pluginval v1.0.4 source')
    juce=root/'modules/juce'
    if git(juce,'rev-parse','HEAD')!=PLUGINVAL_JUCE_COMMIT or git(juce,'status','--porcelain'):
        raise ValueError('Expected unchanged pluginval-pinned JUCE submodule')

def inventory(plugin: Path):
    if plugin.is_symlink():raise ValueError('Linked plugin root')
    paths=sorted(plugin.rglob('*')) if plugin.is_dir() else [plugin]
    result={}
    for p in paths:
        if p.is_symlink():raise ValueError('Linked plugin member')
        if p.is_file():result[p.relative_to(plugin).as_posix() if plugin.is_dir() else p.name]=digest(p)
    if not result:raise ValueError('Plugin bundle missing')
    return result

def parse_output(text: str):
    return {'test_starts':len(re.findall(r'^Starting (?:tests in|test):',text,re.MULTILINE)),
            'success_marker':bool(re.search(r'^SUCCESS\s*$',text,re.MULTILINE)),
            'reported_skips':re.findall(r'^.*INFO: Skipping.*$',text,re.MULTILINE)}

def completed_output(text: str,returncode: int,timed_out: bool,unchanged: bool):
    parsed=parse_output(text)
    return returncode==0 and not timed_out and unchanged and parsed['success_marker'] and parsed['test_starts']>0

def run(tool: Path,source: Path,plugin: Path,output: Path,level: int,timeout: int):
    verify_source(source)
    hashes=inventory(plugin)
    output.mkdir(parents=True,exist_ok=False)
    env=os.environ.copy()
    if os.name!='nt':
        for key,folder in [('XDG_CONFIG_HOME','config'),('XDG_DATA_HOME','data'),('XDG_CACHE_HOME','cache')]:
            env[key]=str(output/'runtime'/folder)
            Path(env[key]).mkdir(parents=True,exist_ok=True)
    env['LC_ALL']='C.UTF-8' if os.name!='nt' else env.get('LC_ALL','')
    server=None;server_log=None
    try:
        if os.name!='nt' and not env.get('DISPLAY'):
            executable=shutil.which('Xvfb')
            if not executable:raise ValueError('Xvfb or an existing DISPLAY is required for complete GUI validation')
            for number in range(98,120):
                path=Path(f'/tmp/.X11-unix/X{number}')
                if path.exists():continue
                server_log=(output/'xvfb.txt').open('wb')
                server=subprocess.Popen([executable,f':{number}','-screen','0','1280x1024x24','-nolisten','tcp','-ac'],stdout=server_log,stderr=subprocess.STDOUT)
                for attempt in range(50):
                    if server.poll() is not None:raise ValueError('Xvfb failed to start; see xvfb.txt')
                    try:
                        with socket.socket(socket.AF_UNIX) as s:s.connect(str(path))
                        break
                    except OSError:
                        if attempt==49:raise ValueError('Virtual display readiness timed out')
                        time.sleep(.1)
                env['DISPLAY']=f':{number}';break
            else:raise ValueError('No free local validation display')
        version=subprocess.check_output([str(tool),'--version'],env=env,stderr=subprocess.STDOUT,timeout=30).decode('utf-8',errors='replace').strip()
        command=[str(tool),'--strictness-level',str(level),'--random-seed','0x5eed1234','--timeout-ms','60000','--output-dir',str(output/'logs'),'--validate',str(plugin)]
        started=dt.datetime.now(dt.timezone.utc).isoformat();clock=time.monotonic()
        kwargs={'start_new_session':True} if os.name!='nt' else {'creationflags':subprocess.CREATE_NEW_PROCESS_GROUP}
        process=subprocess.Popen(command,env=env,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,**kwargs)
        timed_out=False
        try:stdout=process.communicate(timeout=timeout)[0]
        except subprocess.TimeoutExpired:
            timed_out=True
            if os.name!='nt':os.killpg(process.pid,signal.SIGKILL)
            else:subprocess.run(['taskkill','/PID',str(process.pid),'/T','/F'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL)
            stdout=process.communicate()[0]
        text=stdout.decode('utf-8',errors='replace');(output/'console.txt').write_text(text,encoding='utf-8')
        parsed=parse_output(text)
        unchanged=inventory(plugin)==hashes
        completed=completed_output(text,process.returncode,timed_out,unchanged)
        proof={'validator':'Tracktion pluginval','tool_version':version,'tool_commit':PLUGINVAL_COMMIT,'tool_juce_commit':PLUGINVAL_JUCE_COMMIT,
               'tool_binary_sha256':digest(tool),'plugin_path':str(plugin),'plugin_files_sha256':hashes,'plugin_unchanged':unchanged,
               'strictness_level':level,'seed':'0x5eed1234','gui_tests_enabled':True,'disabled_tests':[],
               'reported_skips':parsed['reported_skips'],'test_starts':parsed['test_starts'],'success_marker':parsed['success_marker'],
               'process_exit_code':process.returncode,'timed_out':timed_out,'status':'passed' if completed and unchanged else 'failed',
               'started_at_utc':started,'duration_seconds':time.monotonic()-clock,'ableton_verified':False,'audio_accuracy_validated':False}
        (output/'manifest.json').write_text(json.dumps(proof,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
        return 0 if proof['status']=='passed' else 1
    finally:
        if server is not None:
            server.terminate()
            try:server.wait(timeout=5)
            except subprocess.TimeoutExpired:server.kill();server.wait()
        if server_log is not None:server_log.close()

def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--tool',type=Path,required=True);p.add_argument('--source',type=Path,required=True)
    p.add_argument('--plugin',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--level',type=int,choices=range(1,11),default=5);p.add_argument('--timeout',type=int,default=1800)
    a=p.parse_args(argv)
    try:return run(a.tool.resolve(strict=True),a.source.resolve(strict=True),a.plugin.resolve(strict=True),a.output.absolute(),a.level,a.timeout)
    except (ValueError,OSError,subprocess.CalledProcessError,subprocess.TimeoutExpired) as e:p.exit(2,'pluginval runner stopped: '+str(e)+'\n')
if __name__=='__main__':raise SystemExit(main())
