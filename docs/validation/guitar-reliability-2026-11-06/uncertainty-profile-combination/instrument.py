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
