from pathlib import Path
import difflib,hashlib,json
root=Path(__file__).parent
base=(root/'baseline-Source/Pitch/TimeDomainPursuit.cpp').read_text();s=base
s=s.replace('#include <cstdint>','#include <cstdint>\n#include <limits>\n#ifdef FLYSNAPPER_PROFILE_SCORE_AUDIT\nextern "C" void flysnapperScoreAudit(double,double);\nextern "C" void flysnapperChoiceAudit(int,int,int,int);\n#endif')
s=s.replace('    std::array<std::array<float,8>,27> profiles{};','    std::array<std::array<float,8>,27> profiles{};\n    std::array<std::array<double,27>,256> profileScales{};')
marker='\n    for(int note=0;note<noteCount;++note){'
prepare=r'''
    // Profile normalization depends only on the currently valid harmonics.
    // Cache every mask so a later frequency retune may change validity safely.
    for(unsigned mask=0;mask<256;++mask)for(size_t p=0;p<next->profiles.size();++p){
        double power=0;for(int h=0;h<8;++h)if(mask&(1u<<h))power+=double(next->profiles[p][size_t(h)])*next->profiles[p][size_t(h)];
        if(power>0)next->profileScales[mask][p]=1/std::sqrt(power);
    }
'''
assert s.count(marker)==2
s=s.replace(marker,prepare+marker,1)
start=s.index('            for(const auto& profile:data->profiles){')
end=s.index('\n        }\n        if(iteration==0)',start)
old=s[start:end]
new=r'''
            unsigned validMask=0;for(int h=0;h<8;++h)if(model.valid[size_t(h)])validMask|=1u<<h;
            const auto& scales=data->profileScales[validMask];
            // Once per candidate: project the self Gram matrix onto each
            // harmonic's already fixed sine/cosine phase direction.
            std::array<double,8> projected{};
            std::array<std::array<double,8>,8> quadratic{};
            for(int h=0;h<8;++h){
                const double hs=phaseSin[size_t(h)],hc=phaseCos[size_t(h)];
                projected[size_t(h)]=correlation[size_t(h)]*hs+correlation[size_t(h+8)]*hc;
                for(int k=0;k<=h;++k){
                    const double ks=phaseSin[size_t(k)],kc=phaseCos[size_t(k)];
                    double value=hs*data->cross(note,h,note,k)*ks+hs*data->cross(note,h,note,k+8)*kc
                        +hc*data->cross(note,h+8,note,k)*ks+hc*data->cross(note,h+8,note,k+8)*kc;
                    if(h==k)value+=ridge*samples*(hs*hs+hc*hc);
                    quadratic[size_t(h)][size_t(k)]=value;
                }
            }
            std::array<double,27> compressedScores{};double compressedBest=0;int compressedChoice=-1;
#ifdef FLYSNAPPER_PROFILE_SCORE_AUDIT
            double auditedBest=0;int auditedChoice=-1;
#endif
            auto originalProfile=[&](size_t p,std::array<double,columns>& atom) noexcept {
                const auto& profile=data->profiles[p];const double scale=scales[p];
                for(int h=0;h<8;++h)if(model.valid[size_t(h)]){const double a=(.5*observed[size_t(h)]+.5*profile[size_t(h)]*scale)/model.weight[size_t(h)];atom[size_t(h)]=a*phaseSin[size_t(h)];atom[size_t(h+8)]=a*phaseCos[size_t(h)];}
                double numerator=0,penalty=0;for(int c=0;c<columns;++c){numerator+=correlation[size_t(c)]*atom[size_t(c)];penalty+=atom[size_t(c)]*atom[size_t(c)];}
                const double denominator=crossShape(note,atom,note,atom)+ridge*samples*penalty;
                return numerator>0&&denominator>1e-12?numerator*numerator/denominator:0.;
            };
            for(size_t p=0;p<data->profiles.size();++p){
                if(scales[p]<=0)continue;const auto& profile=data->profiles[p];
                std::array<double,8> amplitude{};
                for(int h=0;h<8;++h)if(model.valid[size_t(h)])amplitude[size_t(h)]=(.5*observed[size_t(h)]+.5*profile[size_t(h)]*scales[p])/model.weight[size_t(h)];
                double numerator=0,denominator=0;
                for(int h=0;h<8;++h){
                    const double a=amplitude[size_t(h)];numerator+=projected[size_t(h)]*a;
                    denominator+=quadratic[size_t(h)][size_t(h)]*a*a;
                    for(int k=0;k<h;++k)denominator+=2*quadratic[size_t(h)][size_t(k)]*a*amplitude[size_t(k)];
                }
                const double score=numerator>0&&denominator>1e-12?numerator*numerator/denominator:0;
                compressedScores[p]=score;
                if(score>compressedBest){compressedBest=score;compressedChoice=int(p);}
#ifdef FLYSNAPPER_PROFILE_SCORE_AUDIT
                std::array<double,columns> auditedAtom{};const double reference=originalProfile(p,auditedAtom);
                flysnapperScoreAudit(score,reference);
                if(reference>auditedBest){auditedBest=reference;auditedChoice=int(p);}
#endif
            }
            if(compressedChoice<0)continue;
            // Compressed arithmetic ranks the profiles. Keep the original
            // atom and exact 16D score for selection/output, and re-evaluate
            // close numerical ties in their original profile order.
            int exactChoice=-1,rechecks=0;double exactBest=0;std::array<double,columns> exactAtom{};
            const double tieMargin=compressedBest*1e-9;
            for(size_t p=0;p<data->profiles.size();++p)if(p==size_t(compressedChoice)||compressedBest-compressedScores[p]<=tieMargin){
                std::array<double,columns> atom{};const double score=originalProfile(p,atom);++rechecks;
                if(score>exactBest){exactBest=score;exactChoice=int(p);exactAtom=atom;}
            }
#ifdef FLYSNAPPER_PROFILE_SCORE_AUDIT
            flysnapperChoiceAudit(compressedChoice,auditedChoice,exactChoice,rechecks);
#endif
            if(exactBest>bestScore){bestScore=exactBest;best=note;bestShape=exactAtom;}
'''
s=s[:start]+new+s[end:]
(root/'fast-Source/Pitch/TimeDomainPursuit.cpp').write_text(s)
(root/'profile-fast.patch').write_text(''.join(difflib.unified_diff(base.splitlines(True),s.splitlines(True),fromfile='a/Source/Pitch/TimeDomainPursuit.cpp',tofile='b/Source/Pitch/TimeDomainPursuit.cpp')))
(root/'initial-source-hashes.json').write_text(json.dumps({'baseline':hashlib.sha256(base.encode()).hexdigest(),'fast':hashlib.sha256(s.encode()).hexdigest()},indent=2)+'\n')
print('replaced',len(old),'characters with',len(new))
