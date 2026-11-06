from pathlib import Path
import hashlib,json,shutil,subprocess,datetime
H=Path(__file__).resolve().parent
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
for name in ['base','observed-f0','uncertainty-f0']:
 dst=H/('profile-cold-'+name);shutil.copytree(H/name/'Source',dst/'Source')
 p=dst/'Source/Pitch/TimeDomainPursuit.h';s=p.read_text().replace('#include <memory>','#include <memory>\n#include <cstdint>')
 old='    bool copyPrediction(std::array<float,predictionSamples>&) const noexcept;';assert old in s
 s=s.replace(old,old+'''\n    struct CpuProfile {uint64_t analyseNs=0,retuneNs=0,basisNs=0,inverseNs=0,gramNs=0,analysisCalls=0,retuneCalls=0,modelUpdates=0;};
    bool copyCpuProfile(CpuProfile&) const noexcept;''');p.write_text(s)
 p=dst/'Source/Pitch/TimeDomainPursuit.cpp';s=p.read_text().replace('#include <cstdint>','#include <cstdint>\n#include <ctime>')
 old='constexpr double pi=3.1415926535897932384626433832795,ridge=.006;';assert old in s
 s=s.replace(old,old+'''\nuint64_t profileNow() noexcept {timespec t{};if(clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t))return 0;return uint64_t(t.tv_sec)*1000000000ULL+uint64_t(t.tv_nsec);}
struct ProfileScope {uint64_t& total;uint64_t start;explicit ProfileScope(uint64_t& t) noexcept:total(t),start(profileNow()){}~ProfileScope(){total+=profileNow()-start;}};''')
 s=s.replace('    std::array<Model,noteCount> models{};','    std::array<Model,noteCount> models{};\n    CpuProfile profile;')
 old='    if(!data||!wave||count<data->samples)return;';assert old in s;s=s.replace(old,old+'\n    ++data->profile.analysisCalls;ProfileScope timeAnalysis(data->profile.analyseNs);')
 old='''namespace {
// Actual detector peaks are strictly frequency-ordered.''';assert old in s;s=s.replace(old,'''bool TimeDomainPursuit::copyCpuProfile(CpuProfile& out) const noexcept {if(!data)return false;out=data->profile;return true;}
namespace {
// Actual detector peaks are strictly frequency-ordered.''')
 if name!='base':
  begin=s.index('    bool rebuildModel(');end=s.index('    void retuneObserved(',begin);body=s[begin:end]
  body=body.replace(' noexcept {',' noexcept {\n        ++profile.modelUpdates;const uint64_t basisStart=profileNow();',1)
  mark='        for(int r=0;r<columns;++r)for(int c=0;c<columns;++c)next.inv';assert mark in body
  body=body.replace(mark,'        profile.basisNs+=profileNow()-basisStart;const uint64_t inverseStart=profileNow();\n'+mark)
  mark='        if(!invertBounded(next.inv))return false;';assert mark in body
  body=body.replace(mark,'        const bool invertible=invertBounded(next.inv);profile.inverseNs+=profileNow()-inverseStart;if(!invertible)return false;const uint64_t gramStart=profileNow();')
  body=body.replace('        return true;','        profile.gramNs+=profileNow()-gramStart;return true;')
  s=s[:begin]+body+s[end:]
  old='    void retuneObserved(const SpectralEvidence* evidence) noexcept {';s=s.replace(old,old+'\n        ++profile.retuneCalls;ProfileScope timeRetune(profile.retuneNs);')
 p.write_text(s)
s=(H/'ablation_probe.cpp').read_text();s=s.replace('#include "Pitch/PitchDetector.h"','#include <array>\n#include <memory>\n#include <cstdint>\n#include <cstddef>\n#define private public\n#include "Pitch/PitchDetector.h"\n#undef private').replace('#include <vector>','#include <vector>\n#include <chrono>\n#include <ctime>')
s=s.replace('int main(int argc,char**argv){','uint64_t threadNow(){timespec t{};clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t);return uint64_t(t.tv_sec)*1000000000ULL+uint64_t(t.tv_nsec);}\nint main(int argc,char**argv){')
old=' std::cout<<"case,cents,timbre,dynamics,sample,type,note\\n";';assert old in s
s=s.replace(old,old+'''\n std::cerr<<"case,cents,timbre,dynamics,wall_deadlines,thread_deadlines,wall_p99_us,wall_max_us,thread_p99_us,thread_max_us,process_thread_us,analyse_calls,analyse_thread_us,retune_calls,retune_thread_us,model_updates,basis_thread_us,inverse_thread_us,gram_thread_us\\n";''')
old=' for(int at=0;at<sr;at+=block){flysnapper::pitch::Output out;det->process(input.data()+at,std::min(block,sr-at),out);';assert old in s
s=s.replace(old,''' std::vector<double> wall,cpu;int wallMiss=0,cpuMiss=0;double processCpu=0;
 for(int at=0;at<sr;at+=block){flysnapper::pitch::Output out;const uint64_t beforeCpu=threadNow();const auto beforeWall=std::chrono::steady_clock::now();det->process(input.data()+at,std::min(block,sr-at),out);const double w=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-beforeWall).count(),threadUs=(threadNow()-beforeCpu)/1000.;wall.push_back(w);cpu.push_back(threadUs);processCpu+=threadUs;wallMiss+=w>1e6*block/sr;cpuMiss+=threadUs>1e6*block/sr;''')
end=' }\n}\n';assert s.endswith(end)
s=s[:-len(end)]+''' flysnapper::pitch::TimeDomainPursuit::CpuProfile p;det->pursuit.copyCpuProfile(p);
 std::sort(wall.begin(),wall.end());std::sort(cpu.begin(),cpu.end());const size_t p99=size_t(std::ceil(.99*(wall.size()-1)));
 std::cerr<<c<<','<<cents<<','<<timbre<<','<<dynamics<<','<<wallMiss<<','<<cpuMiss<<','<<wall[p99]<<','<<wall.back()<<','<<cpu[p99]<<','<<cpu.back()<<','<<processCpu<<','<<p.analysisCalls<<','<<p.analyseNs/1000.<<','<<p.retuneCalls<<','<<p.retuneNs/1000.<<','<<p.modelUpdates<<','<<p.basisNs/1000.<<','<<p.inverseNs/1000.<<','<<p.gramNs/1000.<<'\\n';
 }
}
'''
s=s.replace(' auto det=std::make_unique<flysnapper::pitch::PitchDetector>();','').replace(' det->prepare(sr);',' auto det=std::make_unique<flysnapper::pitch::PitchDetector>();det->prepare(sr);')
driver=H/'profile_cpu_probe_cold.cpp';driver.write_text(s)
for name in ['base','observed-f0','uncertainty-f0']:
 dst=H/('profile-cold-'+name);sources=[p for p in sorted((dst/'Source/Pitch').glob('*.cpp')) if p.name!='MpeOutputRouter.cpp'];exe=dst/'capture';inputs=sorted(p for p in (dst/'Source').rglob('*') if p.is_file());before={p.relative_to(dst).as_posix():sha(p) for p in inputs}
 with (dst/'compile.txt').open('w') as log:subprocess.run(['g++','-std=c++17','-O2','-DNDEBUG','-I',str(dst/'Source'),str(driver),*[str(p) for p in sources],'-o',str(exe)],stdout=log,stderr=subprocess.STDOUT,check=True)
 with (dst/'events.csv').open('w') as stdout,(dst/'profile.csv').open('w') as stderr:result=subprocess.run([str(exe),'10'],stdout=stdout,stderr=stderr)
 assert before=={p.relative_to(dst).as_posix():sha(p) for p in inputs}
 (dst/'manifest.json').write_text(json.dumps({'recorded_at_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'original_snapshot':str(H/name),'base_git_commit':'e235c8a098dadce6e0a4b8bd6ca9de789a5d174d','files_sha256':before,'driver_sha256':sha(driver),'binary_sha256':sha(exe),'algorithm_unchanged_except_instrumentation':True,'fresh_detector_instance_per_configuration':True,'cache_reuse_between_configurations':False,'clocks':'steady_clock wall plus CLOCK_THREAD_CPUTIME_ID process/function scopes; instrumentation overhead is included','scope':'30 standard six-open-string synthesis configurations, all five cents offsets/three timbres/dynamics off/default; no real/reserved data','runner_exit_code':result.returncode,'output_sha256':{p.name:sha(p) for p in dst.iterdir() if p.is_file() and p.suffix=='.csv'}},indent=2)+'\n')
 print(name,'thread/function profiling complete',flush=True)
