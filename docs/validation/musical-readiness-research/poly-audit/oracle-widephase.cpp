#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>
#define private public
#include "Pitch/PolyphonicTracker.h"
#undef private
#include "Pitch/PolyphonicTracker.cpp"
using flysnapper::pitch::PolyphonicTracker;
constexpr double pi=3.14159265358979323846,rate=12000;
struct Tone {double note,level=.12,on=0,stiffness=0,evenGain=1,rootGain=1,bend=0,bendStart=.3,bendDuration=.3;int partials=8;};
struct Fixture {std::string name;std::vector<Tone> tones;int lengthMode=0;};
double hz(double p){return 440*std::pow(2.,(p-69)/12.);}
double midi(const Tone&t,double time){return t.note+t.bend*std::clamp((time-t.bendStart)/t.bendDuration,0.,1.);}
double quantile(std::vector<double> v,double q){if(v.empty())return -1;std::sort(v.begin(),v.end());return v[size_t(q*(v.size()-1))];}
void run(const Fixture& fixture,std::ostream& summary){
 PolyphonicTracker tracker;tracker.prepare(rate);PolyphonicTracker::Settings settings;settings.newNoteThresholdDb=-70;settings.earlyAcquisition=false;settings.legacySeeds=false;std::array<bool,6> seeded{};
 std::array<float,512> ring{},frame{};int wr=0,filled=0;
 std::array<std::array<double,8>,6> phases{};std::vector<double> times,errors;int assessed=0,missing=0,extra=0,bad=0,maxCount=0;
 std::ofstream trace("/tmp/flysnapper-poly-audit/widephase/oracle-"+fixture.name+".csv");trace<<"sample,time,observation,voices,id,anchor,pitch,level,confidence,onset,truth_nearest,error_cents,analyse_us\n";
 for(int sample=0;sample<14400;++sample){
  const double time=sample/rate;double value=0;
  for(size_t j=0;j<fixture.tones.size();++j){const auto&t=fixture.tones[j];const double p=midi(t,time),f=hz(p);
   for(int h=1;h<=t.partials;++h){const double partial=f*h*std::sqrt((1+t.stiffness*h*h)/(1+t.stiffness));phases[j][size_t(h-1)]+=2*pi*partial/rate;
    if(time<t.on)continue;
    const double attack=std::min(1.,(time-t.on)/.003);double amplitude=t.level*attack/std::pow(double(h),1.35);if(h==1)amplitude*=t.rootGain;if(h%2==0)amplitude*=t.evenGain;
    // Distinct fixed partial phases avoid making every string artificially phase aligned.
    value+=amplitude*std::sin(phases[j][size_t(h-1)]+.27*j+.13*h*h);
   }
  }
  ring[size_t(wr)]=float(value);wr=(wr+1)%512;filled=std::min(512,filled+1);if(sample%32!=31||filled<128)continue;
  int n=filled-filled%32;
  if(time>.4&&fixture.lengthMode==1)n=256+32*((sample/32)%9);
  if(time>.4&&fixture.lengthMode==2)n=256;
  if(time>.4&&fixture.lengthMode==3)n=384;
  n=std::min(n,filled-filled%32);
  for(int j=0;j<n;++j)frame[size_t(j)]=ring[size_t((wr-n+j+512)%512)];
  for(size_t j=0;j<fixture.tones.size();++j)if(!seeded[j]&&time>=fixture.tones[j].on+.03&&filled>=512){auto& s=tracker.data->states[j];s.view.active=true;s.view.id=unsigned(j);s.view.anchorNote=int(std::lround(fixture.tones[j].note));s.view.frequency=float(hz(std::round(fixture.tones[j].note)));s.view.pitch=float(std::round(fixture.tones[j].note));seeded[j]=true;}
  PolyphonicTracker::Frame output;auto before=std::chrono::steady_clock::now();tracker.analyse(frame.data(),n,32,settings,output);auto after=std::chrono::steady_clock::now();
  const double us=std::chrono::duration<double,std::micro>(after-before).count();maxCount=std::max(maxCount,output.count);
  if(time>.2&&time<1.05){++assessed;times.push_back(us);int thisMissing=0,thisExtra=0;
   std::array<bool,6> matched{};
   for(const auto&t:fixture.tones)if(time>t.on+.08){int best=-1;double error=.6;for(int j=0;j<6;++j)if(output.voices[size_t(j)].active&&!matched[size_t(j)]){double e=std::abs(output.voices[size_t(j)].pitch-midi(t,time));if(e<error){error=e;best=j;}}if(best<0)++thisMissing;else{matched[size_t(best)]=true;errors.push_back(100*error);}}
   for(int j=0;j<6;++j)if(output.voices[size_t(j)].active&&!matched[size_t(j)])++thisExtra;
   missing+=thisMissing;extra+=thisExtra;bad+=(thisMissing||thisExtra);
  }
  if(!output.count)trace<<sample<<','<<time<<','<<n<<",0,-1,-1,-1,0,0,0,-1,-1,"<<us<<'\n';
  for(const auto&v:output.voices)if(v.active){double nearest=-1,error=1e9;for(const auto&t:fixture.tones)if(time>=t.on){double p=midi(t,time),e=std::abs(v.pitch-p);if(e<error){error=e;nearest=p;}}
   trace<<sample<<','<<time<<','<<n<<','<<output.count<<','<<v.id<<','<<v.anchorNote<<','<<v.pitch<<','<<v.level<<','<<v.confidence<<','<<v.onset<<','<<nearest<<','<<error*100<<','<<us<<'\n';
  }
 }
 summary<<fixture.name<<','<<fixture.tones.size()<<','<<assessed<<','<<bad<<','<<missing<<','<<extra<<','<<maxCount<<','<<quantile(errors,.5)<<','<<quantile(errors,.95)<<','<<quantile(errors,1)<<','<<quantile(times,.5)<<','<<quantile(times,.95)<<','<<quantile(times,.99)<<','<<quantile(times,1)<<'\n';
}
int main(){
 std::vector<Fixture> fixtures;
 fixtures.push_back({"single_fractional",{{57.27,.16}}});
 fixtures.push_back({"single_fractional_lengths",{{57.27,.16}},1});
 fixtures.push_back({"single_fractional_256",{{57.27,.16}},2});
 fixtures.push_back({"single_fractional_384",{{57.27,.16}},3});
 fixtures.push_back({"major_seventh",{{57,.13},{68,.11}}});
 fixtures.push_back({"adjacent_52_53",{{52,.13},{53,.11}}});
 fixtures.push_back({"octave_52_64",{{52,.13},{64,.11}}});
 fixtures.push_back({"octave_strummed",{{52,.13},{64,.11,.25}}});
 fixtures.push_back({"strong_even_single",{{52,.13,0,0,3,.15}}});
 fixtures.push_back({"stiff_single",{{52,.13,0,.0005,1,.6}}});
 fixtures.push_back({"stiff_adjacent",{{52,.13,0,.0003},{53,.11,0,.0006}}});
 fixtures.push_back({"bend_two_slow",{{57,.13,0,0,1,1,2,.25,.3},{64,.11}}});
 fixtures.push_back({"bend_two_fast",{{57,.13,0,0,1,1,2,.25,.07},{64,.11}}});
 fixtures.push_back({"bend_lowE",{{40,.13,0,0,1,1,2,.25,.3},{59,.11}}});
 fixtures.push_back({"open_Em_six",{{40,.10},{47,.10},{52,.10},{55,.10},{59,.10},{64,.10}}});
 fixtures.push_back({"strummed_Em_six",{{40,.10},{47,.10,.03},{52,.10,.06},{55,.10,.09},{59,.10,.12},{64,.10,.15}}});
 fixtures.push_back({"low_fractional",{{40.27,.16}}});
 fixtures.push_back({"low_fractional_lengths",{{40.27,.16}},1});
 fixtures.push_back({"low_bend",{{40,.16,0,0,1,1,.8,.40,.35}}});
 fixtures.push_back({"low_bend_lengths",{{40,.16,0,0,1,1,.8,.40,.35}},1});
 fixtures.push_back({"six_bend_E3",{{40,.10},{47,.10},{52,.10,0,0,1,1,2,.25,.40},{55,.10},{59,.10},{64,.10}}});
 fixtures.push_back({"six_bend_B3",{{40,.10},{47,.10},{52,.10},{55,.10},{59,.10,0,0,1,1,2,.25,.40},{64,.10}}});
 fixtures.push_back({"six_bend_E4",{{40,.10},{47,.10},{52,.10},{55,.10},{59,.10},{64,.10,0,0,1,1,2,.25,.40}}});
 for(size_t i=0,last=fixtures.size();i<last;++i)if(fixtures[i].name.rfind("six_bend_",0)==0){auto f=fixtures[i];f.name+="_3partials";for(auto& t:f.tones)t.partials=3;fixtures.push_back(f);}
 std::ofstream summary("/tmp/flysnapper-poly-audit/widephase/oracle-summary.csv");summary<<"case,truth_voices,assessed_frames,bad_frames,missing_voice_frames,extra_voice_frames,max_active,error_median_cents,error_p95_cents,error_max_cents,analyse_p50_us,analyse_p95_us,analyse_p99_us,analyse_max_us\n";
 for(const auto&fixture:fixtures){run(fixture,summary);std::cout<<fixture.name<<" done\n";summary.flush();}
}
