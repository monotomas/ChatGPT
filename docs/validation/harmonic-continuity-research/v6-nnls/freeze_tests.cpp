// SPDX-License-Identifier: AGPL-3.0-or-later
#define main original_mechanism_main
#include "tests.cpp"
#undef main
int main(){try{
    const auto fixed=[](int){return 110.;};
    const auto frozen=[](int i){return i>=3600&&i<7800;};
    const auto common=run([](int i){const double t=i/rate;return tone(2*pi*110*t+(t>=.3?.5:0));},fixed,12000,frozen);
    int support=0;double first=-1;
    for(const auto& r:common)if(r.sample>=3600&&r.sample<6000){support=std::max(support,r.evidence.departurePartials);if(first<0&&r.evidence.departurePartials>=3)first=r.sample/rate;}
    std::cout<<"frozen_common_reset departures "<<support<<" first "<<first<<'\n';check(support>=3,"Frozen common reset lost");
    const auto contaminated=run([](int i){const double t=i/rate;return tone(2*pi*110*t)+(t>=.3&&t<.5?.2*std::sin(2*pi*110*t+1.5):0);},fixed,12000,frozen);
    int maximumChanged=0,energy=0;
    for(const auto& r:contaminated)if(r.sample>=3600&&r.sample<7800){maximumChanged=std::max(maximumChanged,r.evidence.departurePartials);energy+=energyEvidence(r.evidence);}
    std::cout<<"frozen_one_band departures "<<maximumChanged<<" energy "<<energy<<'\n';check(maximumChanged<=1&&energy==0,"Frozen one-band contaminant became independent excitation");
    const std::array<double,8> shifts{{.05,.9,-.8,1.4,-.6,2.1,.2,-1}};
    const auto independent=run([&](int i){const double t=i/rate;double x=0;for(int h=1;h<=8;++h)x+=.1*std::sin(2*pi*110*t*h+h*.17+(t>=.3?shifts[size_t(h-1)]:0))/std::pow(double(h),1.3);return x;},fixed,12000,frozen);
    support=0;first=-1;for(const auto& r:independent)if(r.sample>=3600&&r.sample<6000){support=std::max(support,r.evidence.departurePartials);if(first<0&&r.evidence.departurePartials>=3)first=r.sample/rate;}
    std::cout<<"frozen_independent_reset departures "<<support<<" first "<<first<<'\n';check(support>=3,"Frozen independent partial reset lost");
    for(double duration:{.025,.080,.220}){
        std::vector<double> frequency(12000),audio(12000);double phase=0;
        for(int i=0;i<12000;++i){const double t=i/rate,rise=std::clamp((t-.32)/duration,0.,1.);frequency[size_t(i)]=110*std::pow(2.,2*rise/12);phase+=2*pi*frequency[size_t(i)]/rate;audio[size_t(i)]=tone(phase);}
        const auto bend=run([&](int i){return audio[size_t(i)];},[&](int i){return frequency[size_t(i)];},12000,frozen);
        support=0;double minimum=1;for(const auto& r:bend)if(r.sample>=3600&&r.sample<7800){support=std::max(support,r.evidence.departurePartials);minimum=std::min(minimum,double(r.evidence.phaseContinuity));}
        std::cout<<"frozen_bend duration "<<duration<<" max_departures "<<support<<" min_continuity "<<minimum<<'\n';
        // Diagnostic only: retain failures rather than assert false success.
    }
    std::cout<<"Frozen mechanism reset/contaminant controls PASS; bend rows remain diagnostics.\n";
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
