// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Pitch/HarmonicContinuity.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <ctime>
namespace {bool countNew=false;size_t news=0;}
void* operator new(std::size_t n){if(countNew)++news;if(auto* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept{std::free(p);}void operator delete[](void* p) noexcept{std::free(p);}
void operator delete(void* p,std::size_t) noexcept{std::free(p);}void operator delete[](void* p,std::size_t) noexcept{std::free(p);}
double cpu(){timespec t{};clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t);return t.tv_sec+t.tv_nsec*1e-9;}
int main(){using Clock=std::chrono::steady_clock;constexpr int observations=800;std::array<float,1024> frame{};
std::cout<<"frequency_hz,wall_median_us,wall_p95_us,wall_max_us,cpu_median_us,cpu_p95_us,cpu_max_us,cpp_new_calls\n";
for(double frequency:{41.2034446,110.,220.,2637.02046}){flysnapper::pitch::HarmonicContinuity observer;observer.prepare(12000);std::array<double,observations> wall{},active{};news=0;
for(int j=0;j<observations;++j){const int sample=1023+j*32;for(int n=0;n<1024;++n){double x=0;for(int h=1;h<=8&&frequency*h<5400;++h)x+=.1*std::sin(6.283185307179586*frequency*h*(sample-1023+n)/12000.+h*.17)/std::pow(h,1.3);frame[size_t(n)]=float(x);}
observer.freezeReference((j/64)%2);countNew=true;const auto start=Clock::now();const auto c=cpu();const auto e=observer.observeAt(frame.data(),1024,float(frequency),sample,sample);active[size_t(j)]=(cpu()-c)*1e6;wall[size_t(j)]=std::chrono::duration<double,std::micro>(Clock::now()-start).count();countNew=false;if(j>10&&e.observedPartials==0){std::cerr<<"Unexpected unobservable stable probe\n";return 1;}}
std::sort(wall.begin(),wall.end());std::sort(active.begin(),active.end());std::cout<<frequency<<','<<wall[observations/2]<<','<<wall[size_t(observations*.95)]<<','<<wall.back()<<','<<active[observations/2]<<','<<active[size_t(observations*.95)]<<','<<active.back()<<','<<news<<'\n';if(news)return 1;}
std::cout<<"Scope: one known source only; shared Linux runner, no host/MIDI/DSP-front-end work or hardware real-time guarantee.\n";}
