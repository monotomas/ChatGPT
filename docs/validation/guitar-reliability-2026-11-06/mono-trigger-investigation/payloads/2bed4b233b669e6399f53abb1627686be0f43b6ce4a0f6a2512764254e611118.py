from pathlib import Path
import shutil,json,hashlib
base=Path('/tmp/flysnapper-v4-mono-audit');root=Path('/workspace/ChatGPT')
for name,sigma in [('plain-phase',None),('variance2',2),('variance3',3)]:
 out=base/name;shutil.copytree(base/'Source',out/'Source',dirs_exist_ok=True)
 cpp=out/'Source/Pitch/PitchDetector.cpp';s=cpp.read_text();s=s.replace('held>=0&&config.monoBend&&measuredPeriod>0\n        ?monoPartialPhase.observe','held>=0&&measuredPeriod>0\n        ?monoPartialPhase.observe').replace('(config.monoBend&&independentExcitation<.75f)','(independentExcitation<.75f)');cpp.write_text(s)
 if sigma is not None:
  h=out/'Source/Pitch/ArticulationTracker.h';s=h.read_text().replace('previous{},trend{},departure{};','previous{},trend{},departure{},previousVariance{};').replace('bool powerValid=false;','bool powerValid=false;double previousPowerUncertainty=0;');h.write_text(s)
  cpp=out/'Source/Pitch/ArticulationTracker.cpp';s=cpp.read_text().replace('previous={};trend={};departure={};','previous={};trend={};departure={};previousVariance={};previousPowerUncertainty=0;')
  s=s.replace('double system[dimension][dimension+1]{};','double system[dimension][dimension+1]{};double observedEnergy=0;')
  s=s.replace('double basis[dimension]{1};double hc=c,hs=s;','observedEnergy+=double(wave[i])*wave[i];double basis[dimension]{1};double hc=c,hs=s;')
  s=s.replace('for(int a=0;a<dimension;++a)for(int b=a+1;', 'std::array<double,dimension> originalRhs{};for(int a=0;a<dimension;++a)originalRhs[size_t(a)]=system[a][dimension];\n    for(int a=0;a<dimension;++a)for(int b=a+1;')
  s=s.replace('std::array<double,4> phase{},power{};double maximum=0;', 'double fittedEnergy=0;for(int a=0;a<dimension;++a)fittedEnergy+=system[a][dimension]*originalRhs[size_t(a)];\n    const double residualVariance=std::max(0.,observedEnergy-fittedEnergy)/std::max(1,count-dimension);\n    const double coefficientVariance=2*residualVariance/count;\n    std::array<double,4> phase{},power{};double maximum=0;')
  s=s.replace('const double relative=std::remainder(phase[size_t(h-1)]-h*phase[0],2*pi);','const double relative=std::remainder(phase[size_t(h-1)]-h*phase[0],2*pi);\n        const double phaseVariance=coefficientVariance*(1/std::max(power[size_t(h-1)],1e-20)+h*h/std::max(power[0],1e-20));')
  s=s.replace('const double distance=std::abs(departure[index]);largest=std::max(largest,distance);\n            changed+=distance>.32;++supported;',f'const double distance=std::abs(departure[index]);const bool resolved=distance>{sigma}*std::sqrt(phaseVariance+previousVariance[index]);if(resolved)largest=std::max(largest,distance);\n            changed+=distance>.32&&resolved;++supported;')
  s=s.replace('previous[index]=relative;valid[index]=supportedNow;','previous[index]=relative;valid[index]=supportedNow;previousVariance[index]=phaseVariance;')
  s=s.replace('if(powerChange>.32)excitation=',f'double powerUncertainty=0;for(double p:power)powerUncertainty+=2*std::sqrt(coefficientVariance*std::max(p,0.))/std::max(totalPower,1e-20);\n    const bool resolvedPower=powerChange>{sigma}*std::sqrt(powerUncertainty*powerUncertainty+previousPowerUncertainty*previousPowerUncertainty);previousPowerUncertainty=powerUncertainty;\n    if(powerChange>.32&&resolvedPower)excitation=')
  cpp.write_text(s)
 driver=(root/'Tools/guitar_evaluation_capture.cpp').read_text().replace('#include <algorithm>','#include <algorithm>\n#include <cstdlib>').replace('const auto original=wave(item.file);','if(item.mode!="mono")continue;const auto original=wave(item.file);').replace('detector->configure(config);','if(std::getenv("PLAIN_MODE"))config.monoBend=false;detector->configure(config);')
 (out/'capture.cpp').write_text(driver)
