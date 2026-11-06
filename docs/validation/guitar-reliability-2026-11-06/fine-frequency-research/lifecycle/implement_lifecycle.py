from pathlib import Path
import shutil
H=Path('/tmp/flysnapper-v4-poly-f0-lifecycle');H.mkdir()
shutil.copytree('/tmp/flysnapper-v4-poly-f0/uncertainty-f0/Source',H/'Source')
p=H/'Source/Pitch/TimeDomainPursuit.cpp';s=p.read_text()
s=s.replace('    std::array<Model,noteCount> models{};','''    std::array<Model,noteCount> models{},nominalModels{};
    std::array<bool,noteCount> fineCacheValid{},fineUsed{};''')
s=s.replace('    std::vector<float> gram;','    std::vector<float> gram,nominalGram,fineToNominalGram;')
old='''    float cross(int noteA,int colA,int noteB,int colB) const noexcept {
        return gram[size_t((noteA*columns+colA)*components+noteB*columns+colB)];
    }''';assert old in s
s=s.replace(old,'''    const Model& model(int note) const noexcept {
        return fineUsed[size_t(note)]?models[size_t(note)]:nominalModels[size_t(note)];
    }
    float cross(int noteA,int colA,int noteB,int colB) const noexcept {
        const size_t ab=size_t((noteA*columns+colA)*components+noteB*columns+colB);
        if(fineUsed[size_t(noteA)])return fineUsed[size_t(noteB)]?gram[ab]:fineToNominalGram[ab];
        if(fineUsed[size_t(noteB)])return fineToNominalGram[size_t((noteB*columns+colB)*components+noteA*columns+colA)];
        return nominalGram[ab];
    }''')
start=s.index('        models[size_t(note)]=next;');end=s.index('        return true;',start)
old=s[start:end]
new='''        models[size_t(note)]=next;fineCacheValid[size_t(note)]=true;
        const auto& a=models[size_t(note)];
        const auto fillRow=[&](int other,const Model& b,std::vector<float>& table,bool symmetric) noexcept {
            for(int h=0;h<harmonics;++h)for(int k=0;k<harmonics;++k){
                const auto minus=combinedSums(a.geometry[size_t(h)],b.geometry[size_t(k)],false,samples);
                const auto plus=combinedSums(a.geometry[size_t(h)],b.geometry[size_t(k)],true,samples);
                const double product=a.weight[size_t(h)]*b.weight[size_t(k)];
                const std::array<double,4> value{{.5*(minus.cosine-plus.cosine),.5*(plus.sine+minus.sine),.5*(plus.sine-minus.sine),.5*(minus.cosine+plus.cosine)}};
                for(int qa=0;qa<2;++qa)for(int qb=0;qb<2;++qb){const int ca=h+qa*harmonics,cb=k+qb*harmonics;
                    const float crossValue=a.valid[size_t(ca)]&&b.valid[size_t(cb)]?float((value[size_t(qa*2+qb)]-samples*a.mean[size_t(ca)]*b.mean[size_t(cb)])*product):0;
                    table[size_t((note*columns+ca)*components+other*columns+cb)]=crossValue;
                    if(symmetric)table[size_t((other*columns+cb)*components+note*columns+ca)]=crossValue;
                }
            }
        };
        for(int other=0;other<noteCount;++other){
            fillRow(other,nominalModels[size_t(other)],fineToNominalGram,false);
            if(fineCacheValid[size_t(other)])fillRow(other,models[size_t(other)],gram,true);
        }
'''
s=s[:start]+new+s[end:]
s=s.replace('''    void retuneObserved(const SpectralEvidence* evidence) noexcept {
        if(!evidence)return;''','''    void retuneObserved(const SpectralEvidence* evidence) noexcept {
        fineUsed.fill(false);
        if(!evidence)return;''')
s=s.replace('const auto& owner=models[size_t(other)];','const auto& owner=nominalModels[size_t(other)];')
start=s.index('        for(int i=0;i<selected;++i){const auto& c=candidates');end=s.index('\n    }\n};',start)
s=s[:start]+'''        for(int i=0;i<selected;++i){const auto& c=candidates[size_t(i)];
            // Exact current-frame request; cached state never selects F0.
            const bool reusable=fineCacheValid[size_t(c.note)]&&models[size_t(c.note)].frequency==c.f;
            if(reusable||rebuildModel(c.note,c.f)){
                models[size_t(c.note)].frequencyUncertainty=c.uncertainty;
                fineUsed[size_t(c.note)]=true;
            }
        }'''+s[end:]
# prepare still uses raw gram because nominal accessor is not available yet.
s=s.replace('m.inv[size_t(r*columns+c)]=next->cross(note,r,note,c)+(r==c?ridge*samples:0);','m.inv[size_t(r*columns+c)]=next->gram[size_t((note*columns+r)*components+note*columns+c)]+(r==c?ridge*samples:0);')
s=s.replace('    data=std::move(next);','''    // Immutable backup creation is outside realtime reset/analyse.
    next->nominalModels=next->models;next->nominalGram=next->gram;
    next->fineToNominalGram.resize(size_t(components)*components);
    data=std::move(next);''')
s=s.replace('bool TimeDomainPursuit::copyPrediction(','''void TimeDomainPursuit::reset() noexcept {
    if(!data)return;
    data->fineUsed.fill(false);data->fineCacheValid.fill(false);
    data->prediction.fill(0);data->predictionValid=false;
}
bool TimeDomainPursuit::copyPrediction(''')
s=s.replace('    if(data)data->predictionValid=false;','    if(data){data->predictionValid=false;data->fineUsed.fill(false);}')
s=s.replace('data->models[size_t(n)]','data->model(n)').replace('data->models[size_t(note)]','data->model(note)').replace('data->models[size_t(predictionSelected[size_t(j)])]','data->model(predictionSelected[size_t(j)])')
p.write_text(s)
p=H/'Source/Pitch/TimeDomainPursuit.h';s=p.read_text().replace('    void prepare(double analysisRate,int samples=512);','    void prepare(double analysisRate,int samples=512);\n    void reset() noexcept;');p.write_text(s)
p=H/'Source/Pitch/PitchDetector.cpp';s=p.read_text();needle='void PitchDetector::reset(Output& out,int offset) noexcept {';assert needle in s;s=s.replace(needle,needle+'\n    pursuit.reset();');p.write_text(s)
print(H)
