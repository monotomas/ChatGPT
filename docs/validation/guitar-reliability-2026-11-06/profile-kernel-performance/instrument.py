from pathlib import Path
import shutil
b=Path(__file__).parent
for label in ('baseline','fast'):
 source=b/(label+'-timed-Source');shutil.copytree(b/(label+'-Source'),source)
 h=source/'Pitch/TimeDomainPursuit.h';s=h.read_text().replace('#include <memory>','#include <memory>\n#include <cstdint>');needle='    TimeDomainPursuit();'
 s=s.replace(needle,'    struct CpuProfile {uint64_t analyseNs=0,profileNs=0,analysisCalls=0,profileCandidates=0;};\n    bool copyCpuProfile(CpuProfile&) const noexcept;\n'+needle);h.write_text(s)
 p=source/'Pitch/TimeDomainPursuit.cpp';s=p.read_text().replace('#include <cstdint>','#include <cstdint>\n#include <ctime>');s=s.replace('constexpr double pi=',r'''uint64_t profileThreadNow() noexcept {timespec t{};clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t);return uint64_t(t.tv_sec)*1000000000ULL+uint64_t(t.tv_nsec);}
struct ProfileCpuScope {uint64_t& total;uint64_t start;explicit ProfileCpuScope(uint64_t& t):total(t),start(profileThreadNow()){}~ProfileCpuScope(){total+=profileThreadNow()-start;}};
constexpr double pi=''',1)
 s=s.replace('    std::array<Model,noteCount> models{};','    std::array<Model,noteCount> models{};\n    CpuProfile profile;')
 s=s.replace('bool TimeDomainPursuit::copyPrediction(', 'bool TimeDomainPursuit::copyCpuProfile(CpuProfile& out) const noexcept {if(!data)return false;out=data->profile;return true;}\nbool TimeDomainPursuit::copyPrediction(',1)
 s=s.replace('    if(!data||!wave||count<data->samples)return;','    if(!data||!wave||count<data->samples)return;\n    ++data->profile.analysisCalls;ProfileCpuScope timeAnalyse(data->profile.analyseNs);')
 marker='            for(const auto& profile:data->profiles){' if label=='baseline' else '            unsigned validMask=0;'
 start=s.index(marker);end=s.index('\n        }\n        if(iteration==0)',start)
 s=s[:start]+'            {++data->profile.profileCandidates;ProfileCpuScope timeProfiles(data->profile.profileNs);\n'+s[start:end]+'\n            }'+s[end:]
 p.write_text(s)
driver=Path('/tmp/flysnapper-v4-poly-f0/profile_cpu_probe_cold.cpp').read_text()
driver=driver.replace('retune_calls,retune_thread_us,model_updates,basis_thread_us,inverse_thread_us,gram_thread_us','profile_candidates,profile_thread_us')
driver=driver.replace("<<p.retuneCalls<<','<<p.retuneNs/1000.<<','<<p.modelUpdates<<','<<p.basisNs/1000.<<','<<p.inverseNs/1000.<<','<<p.gramNs/1000.","<<p.profileCandidates<<','<<p.profileNs/1000.")
(b/'profile-thread-probe.cpp').write_text(driver)

auditDriver=(b/'probe-cold.cpp').read_text();auditDriver='#include <fstream>\n#include <cstdint>\n'+auditDriver
hooks=r'''
namespace {uint64_t scoreCalls=0,candidateCalls=0,compressedMismatches=0,finalMismatches=0,extraRechecks=0,zeroReferenceNonzero=0;double maxAbsolute=0,maxRelative=0;
struct AuditReport {~AuditReport(){std::ofstream out(FLYSNAPPER_AUDIT_PATH);out<<std::setprecision(17)<<"{\n\"score_calls\":"<<scoreCalls<<",\n\"candidate_calls\":"<<candidateCalls<<",\n\"compressed_argmax_differences\":"<<compressedMismatches<<",\n\"final_argmax_differences\":"<<finalMismatches<<",\n\"extra_original_rechecks\":"<<extraRechecks<<",\n\"reference_zero_approx_nonzero\":"<<zeroReferenceNonzero<<",\n\"max_absolute_score_error\":"<<maxAbsolute<<",\n\"max_relative_score_error\":"<<maxRelative<<"\n}\n";}} reporter;
}
extern "C" void flysnapperScoreAudit(double approximate,double reference){++scoreCalls;const double error=std::abs(approximate-reference);maxAbsolute=std::max(maxAbsolute,error);if(reference!=0)maxRelative=std::max(maxRelative,error/std::abs(reference));else zeroReferenceNonzero+=approximate!=0;}
extern "C" void flysnapperChoiceAudit(int approximate,int reference,int final,int rechecks){++candidateCalls;compressedMismatches+=approximate!=reference;finalMismatches+=final!=reference;extraRechecks+=std::max(0,rechecks-1);}
'''
auditDriver=auditDriver.replace('int main(int argc,char**argv){',hooks+'\nint main(int argc,char**argv){')
(b/'score-audit-probe.cpp').write_text(auditDriver)
