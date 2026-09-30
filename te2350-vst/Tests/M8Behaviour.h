#pragma once
#include <cmath>
#include <vector>
#include <iostream>
#include <algorithm>
#include <complex>
extern "C" {
#include "../../include/te2350.h"
}

// Shared by MusicalBehaviourTest and a JUCE-independent core runner.
#ifdef M8_ALLOCATION_AUDIT
extern bool m8InCallback;
extern unsigned long m8AllocationCount;
#endif
inline void m8Process(te2350_t* core, q31_t input, q31_t* l, q31_t* r)
{
#ifdef M8_ALLOCATION_AUDIT
    m8InCallback=true;
#endif
    te2350_process(core,input,l,r);
#ifdef M8_ALLOCATION_AUDIT
    m8InCallback=false;
#endif
}
inline double m8HighFrequencyRatio(const std::vector<double>& signal, double sr)
{
    constexpr size_t size=4096;
    double high=0,total=0;
    for (size_t start=0; start+size<=signal.size(); start+=size)
    {
        std::vector<std::complex<double>> a(size);
        for (size_t k=0;k<size;++k) a[k]=signal[start+k]*(.5-.5*std::cos(6.283185307179586*k/(size-1)));
        for (size_t i=1,j=0;i<size;++i) {
            size_t bit=size>>1;
            for (;j&bit;bit>>=1) j^=bit;
            j^=bit; if(i<j) std::swap(a[i],a[j]);
        }
        for (size_t len=2;len<=size;len<<=1) {
            const auto w=std::polar(1.0,-6.283185307179586/len);
            for (size_t i=0;i<size;i+=len) {
                std::complex<double> phase=1;
                for (size_t j=0;j<len/2;++j) {
                    auto u=a[i+j],v=a[i+j+len/2]*phase;
                    a[i+j]=u+v;a[i+j+len/2]=u-v;phase*=w;
                }
            }
        }
        for(size_t k=1;k<=size/2;++k) {
            const double e=std::norm(a[k]); total+=e;
            if(k*sr/size>8000) high+=e;
        }
    }
    return high/std::max(total,1.e-30);
}

inline bool verifyM8Behaviour()
{
    bool passed = true;
    const auto check = [&](bool ok, const char* name) {
        std::cout << "M8 " << name << (ok ? " PASSED" : " FAILED") << '\n';
        passed = ok && passed;
    };
    for (int sr : {44100, 48000, 96000})
    {
        for (int interval = 0; interval < 3; ++interval)
        {
            for (bool freeze : {false, true})
            {
                std::vector<q31_t> pool(TE2350_REQUIRED_MEMORY_WORDS, 0);
                te2350_t core;
                check(te2350_init(&core, pool.data(), pool.size()*sizeof(q31_t), float(sr)), "init");
                te2350_set_mix(&core, Q31_MAX);
                te2350_set_time_samples(&core, sr/8);
                te2350_set_feedback(&core, FLOAT_TO_Q31(.9f));
                te2350_set_tail(&core, FLOAT_TO_Q31(.9f));
                te2350_set_shimmer(&core, FLOAT_TO_Q31(.75f));
                te2350_set_shimmer_interval(&core, interval);
                te2350_set_octave_feedback_enabled(&core, true);
                te2350_set_octave_feedback_amount(&core, FLOAT_TO_Q31(.9f));
                te2350_set_ducking(&core, 0);
                std::vector<double> energy(22, 0);
                std::vector<double> frozenEarly, frozenLate;
                double peak = 0;
                for (int n=0; n<22*sr; ++n)
                {
                    if (freeze && n==sr) te2350_set_freeze(&core, true);
                    float input = n<sr ? .35f*std::sin(float(6.283185307179586*220*n/sr)) : 0;
                    q31_t l,r; m8Process(&core, float_to_q31_safe(input), &l, &r);
                    const double a=Q31_TO_FLOAT(l), b=Q31_TO_FLOAT(r);
                    peak=std::max(peak,std::max(std::abs(a),std::abs(b)));
                    energy[n/sr] += (a*a+b*b)/(2*sr);
                    if(freeze && n>=2*sr && n<3*sr) frozenEarly.push_back(a);
                    if(freeze && n>=21*sr) frozenLate.push_back(a);
                }
                check(std::isfinite(peak) && peak < .98, "interval peak/finite");
                check(energy[1]>1.e-10, "early tail present");
                check(energy.back() < energy[2]*1.5+1.e-12, "no long-tail runaway");
                if (freeze) check(energy.back()>energy[2]*.1 && energy.back()<energy[2]*1.5,
                                   "freeze retains energy for 20 seconds");
                if (freeze) check(m8HighFrequencyRatio(frozenLate,sr)
                                     < m8HighFrequencyRatio(frozenEarly,sr)*2+.01,
                                    "freeze HF does not run away");
            }
        }
    }
    for (int interval=0;interval<3;++interval) {
        double ratios[2]={};
        for(int mode=0;mode<2;++mode) {
            constexpr int sr=48000;
            std::vector<q31_t> pool(TE2350_REQUIRED_MEMORY_WORDS,0);
            te2350_t core; te2350_init(&core,pool.data(),pool.size()*sizeof(q31_t),sr);
            te2350_set_mix(&core,Q31_MAX); te2350_set_time_samples(&core,sr/8);
            te2350_set_feedback(&core,FLOAT_TO_Q31(.9f)); te2350_set_tail(&core,FLOAT_TO_Q31(.9f));
            te2350_set_shimmer(&core,FLOAT_TO_Q31(.75f));te2350_set_shimmer_interval(&core,interval);
            te2350_set_octave_feedback_enabled(&core,true);
            te2350_set_octave_feedback_amount(&core,float_to_q31_safe(mode?.9f:.3f));
            te2350_set_ducking(&core,0);
            std::vector<double> tail;
            uint32_t seed=2350;
            for(int n=0;n<sr*3;++n) {
                seed=seed*1664525u+1013904223u;
                float input=n<sr/10 ? .35f*(float(seed>>8)/8388608.f-1.f):0;
                q31_t l,r;m8Process(&core,float_to_q31_safe(input),&l,&r);
                if(n>=sr/2) tail.push_back(Q31_TO_FLOAT(l));
            }
            ratios[mode]=m8HighFrequencyRatio(tail,sr);
        }
        std::cout<<"M8 HF interval="<<interval<<" low="<<ratios[0]<<" high="<<ratios[1]<<'\n';
        check(ratios[1]<ratios[0]*1.5+.02,"HF bounded at high Regen");
    }
    for(int sr:{44100,48000,96000}) {
        double depths[3]={};
        int index=0;
        for(float db:{-48.f,-24.f,-12.f}) {
            std::vector<q31_t> pool(TE2350_REQUIRED_MEMORY_WORDS,0);
            te2350_t core;te2350_init(&core,pool.data(),pool.size()*sizeof(q31_t),float(sr));
            te2350_set_mix(&core,Q31_MAX);te2350_set_ducking(&core,FLOAT_TO_Q31(.9f));
            te2350_set_duck_threshold(&core,float_to_q31_safe(std::pow(10.f,db/20)));
            double maxReduction=0,atAttack=0,atRecovery=0,sustainMin=1,sustainMax=0;
            for(int n=0;n<3*sr;++n) {
                const float input=n>=sr && n<sr+sr/500 ? .20f
                    : (n>=2*sr ? .4f*std::sin(float(6.283185307179586*220*n/sr)):0);
                q31_t l,r;m8Process(&core,float_to_q31_safe(input),&l,&r);
                const double red=Q31_TO_FLOAT(core.duck_reduction_state);
                if(n>=sr && n<2*sr) maxReduction=std::max(maxReduction,red);
                if(n==sr+sr/1000) atAttack=red;
                if(n==sr+sr/2) atRecovery=red;
                if(n>int(2.5*sr)) {sustainMin=std::min(sustainMin,red);sustainMax=std::max(sustainMax,red);}
            }
            depths[index++]=maxReduction;
            if(db<=-24) {
                check(atAttack>.05,"duck attack within 1 ms");
                check(atRecovery<maxReduction*.1,"duck recovery within 500 ms");
            }
            check(sustainMax<.65 && sustainMax-sustainMin<.05,"moderate sustain without pumping");
        }
        check(depths[0]>depths[1] && depths[1]>depths[2]+.05 && depths[2]<.005,
              "threshold remains effective");
    }
    check(q31_mul(Q31_MIN,Q31_MIN)==Q31_MAX && q31_add_sat(Q31_MAX,1)==Q31_MAX,
          "Q31 saturation edges");
#ifdef M8_ALLOCATION_AUDIT
    check(m8AllocationCount==0,"zero allocations/frees in core callback");
#endif
    return passed;
}
