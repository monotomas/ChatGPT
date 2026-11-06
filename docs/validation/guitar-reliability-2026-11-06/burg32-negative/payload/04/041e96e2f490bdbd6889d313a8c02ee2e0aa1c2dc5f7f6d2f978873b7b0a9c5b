#include "Pitch/PitchDetector.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
using namespace flysnapper::pitch;
int main(int argc,char**argv){if(argc!=2)return 2;std::filesystem::path folder=argv[1];std::filesystem::create_directories(folder);std::ofstream out(folder/"counter-events.csv"),metrics(folder/"counter-results.csv");out<<"note,bend,dynamics,sample,type,pitch\n";metrics<<"note,bend,dynamics,note_ons,note_offs,correct,second_decision,active\n";constexpr double pi=3.14159265358979323846;for(int note:{40,52,57,64})for(bool bend:{false,true})for(bool dynamics:{false,true}){auto detector=std::make_unique<PitchDetector>();detector->prepare(48000);Settings config;config.monoBend=bend;config.bendRangeSemitones=12;config.inputDynamics.enabled=dynamics;detector->configure(config);std::vector<float> wave(48000,0);double f=440*std::pow(2.,(note-69.)/12.);for(int at=3840;at<33600;++at){double t=at/48000.,age=t-.08;double envelope=.12*std::min(1.,age/.002);if(t>.2)envelope*=1-.9*std::min(1.,(t-.2)/.02);for(int h=1;h<=8;++h){double phase=.17*h+(at>=21600?.43*h*h:0);wave[size_t(at)]+=float(envelope*std::sin(2*pi*f*h*t+phase)/std::pow(double(h),1.25));}}int ons=0,offs=0,correct=0,second=-1;Output result;for(int at=0;at<int(wave.size());at+=64){result.clear();detector->process(wave.data()+at,std::min(64,int(wave.size())-at),result);for(size_t k=0;k<result.size;++k){auto&e=result.events[k];if(e.pitchChange)continue;out<<note<<','<<bend<<','<<dynamics<<','<<at+e.offset<<','<<(e.on?'N':'F')<<','<<e.note<<'\n';if(e.on){++ons;correct+=e.note==note;if(at+e.offset>=21600&&e.note==note&&second<0)second=at+e.offset;}else++offs;}}metrics<<note<<','<<bend<<','<<dynamics<<','<<ons<<','<<offs<<','<<correct<<','<<second<<','<<detector->status().activeNotes<<'\n';}std::cout<<"16 predeclared post-withdrawal quiet phase-only repick cases; each expects two same-note excitations; no physical recording claim.\n";}
