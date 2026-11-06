import fcntl,hashlib,json,os,pathlib,signal,socket,subprocess,time
root=pathlib.Path('/workspace/ChatGPT');out=root/'build/guitar-v4-recorded-retrigger-baseline'
inputs=list((root/'Source').rglob('*'))+[root/'Tests/DynamicsProcessorTests.cpp',root/'CMakeLists.txt',root/'Tests/AudioFixtures/guitar_reliability_2026_11_06/original-source/Samples/green/stac/staccato_e4_rr3.wav']
binary=root/'build/native-release/FlysnapperDynamicsProcessorTests_artefacts/Release/FlysnapperDynamicsProcessorTests'
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def inventory():return {str(p.relative_to(root)):digest(p) for p in sorted(inputs) if p.is_file()}
with (root/'build/native-release/.flysnapper-build.lock').open('a') as lock:
 fcntl.flock(lock,fcntl.LOCK_EX)
 before=inventory();(out/'source-inputs-before.json').write_text(json.dumps(before,indent=2)+'\n')
 result={'binary':str(binary.relative_to(root)),'binary_sha256':digest(binary),'command':[str(binary),str(root/'Tests/AudioFixtures')],'cwd':str(out),'scope':'Actual processor, fresh test binary and unchanged already-built runtime library; selected project/input hashes, not full toolchain closure'}
 env=os.environ.copy();env['DISPLAY']=':98'
 xvfbLog=(out/'xvfb.txt').open('w');xvfb=subprocess.Popen(['Xvfb',':98','-screen','0','1280x1024x24','-nolisten','tcp','-ac'],stdout=xvfbLog,stderr=subprocess.STDOUT)
 try:
  for attempt in range(50):
   try:
    s=socket.socket(socket.AF_UNIX);s.connect('/tmp/.X11-unix/X98');s.close();break
   except OSError:
    if attempt==49:raise
    time.sleep(.1)
  with (out/'console.txt').open('w') as log:
   p=subprocess.run(result['command'],cwd=out,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180)
  result['exit_code']=p.returncode
 finally:
  xvfb.terminate()
  try:xvfb.wait(timeout=5)
  except subprocess.TimeoutExpired:xvfb.kill();xvfb.wait()
  xvfbLog.close()
 after=inventory();(out/'source-inputs-after.json').write_text(json.dumps(after,indent=2)+'\n');result['source_inputs_unchanged']=before==after
 (out/'result.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2));print((out/'console.txt').read_text())
