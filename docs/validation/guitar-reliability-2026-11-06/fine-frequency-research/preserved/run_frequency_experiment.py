from pathlib import Path
import csv,datetime,hashlib,json,shutil,subprocess
H=Path(__file__).resolve().parent
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def instrument(name):
 s=(H/(name+'.cpp')).read_text().replace('#include <vector>','#include <vector>\n#include <chrono>')
 s=s.replace(' std::cout<<"case,cents,timbre,dynamics,sample,type,note\\n";',' std::cout<<"case,cents,timbre,dynamics,sample,type,note\\n";\n std::cerr<<"case,cents,timbre,dynamics,callbacks,deadline_misses,cpu_median_us,cpu_p95_us,cpu_p99_us,cpu_max_us\\n";')
 old=' for(int at=0;at<sr;at+=block){flysnapper::pitch::Output out;det->process(input.data()+at,std::min(block,sr-at),out);'
 new=' std::vector<double> timings;int deadlineMisses=0;\n for(int at=0;at<sr;at+=block){flysnapper::pitch::Output out;const auto before=std::chrono::steady_clock::now();det->process(input.data()+at,std::min(block,sr-at),out);const double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-before).count();timings.push_back(us);deadlineMisses+=us>1e6*std::min(block,sr-at)/sr;'
 assert old in s;s=s.replace(old,new)
 old=' }\n}\n';assert s.endswith(old)
 s=s[:-len(old)]+''' std::sort(timings.begin(),timings.end());
 std::cerr<<c<<','<<cents<<','<<timbre<<','<<dynamics<<','<<timings.size()<<','<<deadlineMisses;
 for(double q:{.5,.95,.99,1.})std::cerr<<','<<timings[size_t(std::ceil(q*(timings.size()-1)))];
 std::cerr<<'\\n';
 }
}
'''
 p=H/(name+'-timed.cpp');p.write_text(s);return p
drivers={'330':instrument('ablation_probe'),'132':instrument('counter_probe')}
compiler=shutil.which('g++')
for variant in ['observed-f0','base']:
 snap=H/variant;obj=snap/'objects';obj.mkdir(exist_ok=False)
 inputs=sorted(p for p in (snap/'Source').rglob('*') if p.is_file())
 hashes={p.relative_to(snap).as_posix():sha(p) for p in inputs}
 (snap/'source-inventory.json').write_text(json.dumps({'base_git_commit':'e235c8a098dadce6e0a4b8bd6ca9de789a5d174d','actual_compiled_source_is_private_snapshot':True,'files_sha256':hashes},indent=2)+'\n')
 with (obj/'compile.txt').open('w') as log:
  for source in sorted((snap/'Source/Pitch').glob('*.cpp')):
   if source.name=='MpeOutputRouter.cpp':continue
   subprocess.run([compiler,'-std=c++17','-O2','-DNDEBUG','-I',str(snap/'Source'),'-c',str(source),'-o',str(obj/(source.stem+'.o'))],stdout=log,stderr=subprocess.STDOUT,check=True)
  for suite,driver in drivers.items():
   exe=snap/('capture'+suite)
   command=[compiler,'-std=c++17','-O2','-DNDEBUG','-I',str(snap/'Source'),str(driver),*[str(p) for p in sorted(obj.glob('*.o'))],'-o',str(exe)]
   subprocess.run(command,stdout=log,stderr=subprocess.STDOUT,check=True)
 assert hashes=={p.relative_to(snap).as_posix():sha(p) for p in inputs}
 for suite,driver in drivers.items():
  out=snap/('runs'+suite);out.mkdir(exist_ok=False);exe=snap/('capture'+suite)
  with (out/'events.csv').open('w') as stdout,(out/'timing.csv').open('w') as stderr:
   result=subprocess.run([str(exe)],stdout=stdout,stderr=stderr)
  rows=list(csv.DictReader((out/'timing.csv').open()))
  assert len(rows)==int(suite)
  manifest={'recorded_at_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'variant':variant,'base_git_commit':'e235c8a098dadce6e0a4b8bd6ca9de789a5d174d','source_snapshot':str(snap),'files_sha256':hashes,'source_changed':hashes!={p.relative_to(snap).as_posix():sha(p) for p in inputs},'source_is_private_research_not_root_current_commit':True,'driver_sha256':sha(driver),'original_driver_sha256':sha(H/('ablation_probe.cpp' if suite=='330' else 'counter_probe.cpp')),'driver_changes':'Adds actual steady_clock callback measurements outside detector, per-case stderr only; unchanged audio generation/settings/output/sample timestamps','binary_sha256':sha(exe),'command':[str(exe)],'runner_exit_code':result.returncode,'capture_configurations':len(rows),'settings':'48000Hz64samples; poly fixed semitones; dynamics off/default as explicit rows','frequency_evidence':'cleaned pre-dynamics FFT only for retuning; processed waveform fit/admission unchanged' if variant=='observed-f0' else 'original fixed MIDI-frequency basis','physical_latency_measured':False,'reserved_audio_used':False,'max_callback_us':max(float(r['cpu_max_us']) for r in rows),'deadline_misses':sum(int(r['deadline_misses']) for r in rows),'output_sha256':{p.name:sha(p) for p in out.iterdir() if p.is_file()}}
  assert not manifest['source_changed'];(out/'measurement-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
  print(variant,suite,'completed',json.dumps({k:manifest[k] for k in ['runner_exit_code','max_callback_us','deadline_misses']}),flush=True)
