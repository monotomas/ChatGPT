// SPDX-License-Identifier: AGPL-3.0-or-later
// Raw causal DEV-only pluck capture. No physical pick/damping truth is implied.
#include "Pitch/PitchDetector.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
namespace fs=std::filesystem;
struct Record {std::string name;fs::path file;int rate=0,note=0,frames=0;};
std::vector<std::string> split(const std::string& line){std::stringstream in(line);std::vector<std::string> r;std::string s;while(std::getline(in,s,'\t'))r.push_back(s);return r;}
int main(int argc,char** argv){try{
 if(argc!=3)throw std::runtime_error("capture <dev-inputs.tsv> <output-dir>");
 const fs::path output=fs::u8path(argv[2]);fs::create_directories(output);
 std::ifstream inventory(fs::u8path(argv[1]));if(!inventory)throw std::runtime_error("Missing preselected inventory");
 std::string line;std::getline(inventory,line);std::vector<Record> records;
 while(std::getline(inventory,line)){auto f=split(line);if(f.size()!=6)throw std::runtime_error("Bad inventory row");records.push_back({f[0],fs::u8path(f[1]),std::stoi(f[2]),std::stoi(f[3]),std::stoi(f[4])});if(f[5]!="development")throw std::runtime_error("Reserved data is prohibited by this capture");}
 if(records.size()!=7)throw std::runtime_error("Expected the exact seven DEV inputs");
 std::ofstream events(output/"events.csv"),metrics(output/"capture.csv");
 events<<"record,profile,idle_samples,rate,block,sample,type,note,velocity,pitch,voice_id\n";
 metrics<<"record,profile,idle_samples,rate,block,source_frames,total_frames,events,overflow,deadline_misses,max_callback_us,active_at_end\n";
 events<<std::setprecision(10);metrics<<std::setprecision(10);
 int runs=0;
 for(const auto& record:records){if(record.name!="staccato_e4_rr3")continue;
  std::ifstream in(record.file,std::ios::binary|std::ios::ate);if(!in)throw std::runtime_error("Missing selected float input");
  const auto bytes=in.tellg();if(bytes!=std::streamoff(record.frames*sizeof(float)))throw std::runtime_error("Float input shape changed");
  in.seekg(0);std::vector<float> original(size_t(record.frames));in.read(reinterpret_cast<char*>(original.data()),bytes);if(!in)throw std::runtime_error("Truncated input");
  for(float x:original)if(!std::isfinite(x))throw std::runtime_error("Nonfinite input");
  for(bool bend:{false,true})for(bool dynamics:{false,true})for(int idleSamples:{0,int(std::llround(record.rate*.350))}){
   const std::string profile=std::string(bend?"mono_bend":"mono_plain")+(dynamics?"_dynamics_default":"_dynamics_off");
   std::ofstream trace(output/(profile+"_idle"+std::to_string(idleSamples)+"-trace.csv"));trace<<"sample,held,absent,phase_attack,attack_age,attack_remaining,can_admit,current_pitch,period,history,history_range,tracked_only,clean_envelope,raw_envelope,gate,onset_envelope,slow_envelope,fast_envelope,innovation_ratio,independent_excitation,phase_innovation,phase_departure,power_change,instant_excitation,phase_held,power1,power2,power3,power4,confidence,harmonic_fraction,phase_coherence,snr_db\n";
   auto detector=std::make_unique<flysnapper::pitch::PitchDetector>();detector->prepare(record.rate);
   flysnapper::pitch::Settings settings;settings.polyphonic=false;settings.monoBend=bend;settings.bendRangeSemitones=12;settings.inputDynamics.enabled=dynamics;detector->configure(settings);
   std::vector<float> signal(size_t(idleSamples)+original.size()+size_t(std::llround(record.rate*.300)),0.f);
   std::copy(original.begin(),original.end(),signal.begin()+idleSamples);
   constexpr int block=64;int countEvents=0,overflow=0,deadlineMisses=0;double maximumUs=0;
   for(int at=0;at<int(signal.size());at+=block){
    const int n=std::min(block,int(signal.size())-at);flysnapper::pitch::Output out;
    const auto before=std::chrono::steady_clock::now();detector->process(signal.data()+at,n,out);const auto after=std::chrono::steady_clock::now();
    int held=-1,absent=0;for(int q=0;q<detector->noteCount;++q)if(detector->voices[size_t(q)].active){held=q;absent=detector->voices[size_t(q)].absent;}
 trace<<at+n-1<<','<<(held<0?-1:held+detector->lowNote)<<','<<absent<<','<<detector->phaseAttack<<','<<detector->phaseAttackAge<<','<<detector->attackRemaining<<','<<detector->canAdmit()<<','<<detector->currentPitch<<','<<detector->measuredPeriod<<','<<detector->pitchHistoryCount<<','<<detector->pitchHistoryRange<<','<<detector->monoTrackedOnly<<','<<detector->cleanEnvelope<<','<<detector->rawCleanEnvelope<<','<<detector->gate<<','<<detector->onsetEnvelope<<','<<detector->slowEnvelope<<','<<detector->fastEnvelope<<','<<detector->previousInnovationRatio<<','<<detector->auditIndependentExcitation<<','<<detector->monoPartialPhase.auditMaxInnovation<<','<<detector->monoPartialPhase.auditMaxDeparture<<','<<detector->monoPartialPhase.auditPowerChange<<','<<detector->monoPartialPhase.auditInstantExcitation<<','<<detector->monoPartialPhase.heldExcitation;
 for(auto power:detector->monoPartialPhase.auditPower)trace<<','<<power;trace<<','<<detector->view.confidence<<','<<detector->auditHarmonicFraction<<','<<detector->auditPhaseCoherence<<','<<detector->auditSnr<<'\n';
 const double us=std::chrono::duration<double,std::micro>(after-before).count();maximumUs=std::max(maximumUs,us);deadlineMisses+=us>1e6*n/record.rate;overflow+=out.overflow;
    for(size_t k=0;k<out.size;++k){const auto& e=out.events[k];if(e.offset<0||e.offset>=n)throw std::runtime_error("Noncausal event offset");
     events<<record.name<<','<<profile<<','<<idleSamples<<','<<record.rate<<','<<block<<','<<at+e.offset<<','<<(e.pitchChange?'P':e.on?'N':'F')<<','<<e.note<<','<<e.velocity<<',';
     if(e.pitchChange)events<<e.pitch;events<<','<<e.voiceId<<'\n';++countEvents;
    }
   }
   metrics<<record.name<<','<<profile<<','<<idleSamples<<','<<record.rate<<','<<block<<','<<original.size()<<','<<signal.size()<<','<<countEvents<<','<<overflow<<','<<deadlineMisses<<','<<maximumUs<<','<<detector->status().activeNotes<<'\n';++runs;
  }
 }
 std::cout<<"Captured "<<runs<<" DEV-only causal pluck runs. No physical-contact latency or damping claim.\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
