from pathlib import Path
import shutil,json,datetime
b=Path('/tmp/flysnapper-v4-real-repeat-audit');old=Path('/tmp/flysnapper-v4-mono-audit')
for name,bypass in [('withdrawal-no-phase',False),('withdrawal-coherent-phase',True)]:
 out=b/name;shutil.copytree(old/'baseline/Source',out/'Source',dirs_exist_ok=True)
 h=out/'Source/Pitch/PitchDetector.h';s=h.read_text().replace('bool configured=false,needsReset=false;','bool monoSourceWithdrawn=false;int monoWithdrawalSamples=0;\n    bool configured=false,needsReset=false;');h.write_text(s)
 f=out/'Source/Pitch/PitchDetector.cpp';s=f.read_text().replace('view={};needsReset=false;','view={};needsReset=false;monoSourceWithdrawn=false;monoWithdrawalSamples=0;')
 rule='!monoSourceWithdrawn'+('||view.confidence>.985f' if bypass else '')
 block='''if(!config.polyphonic&&view.activeNotes>0&&measuredPeriod>0){
            int held=-1;for(int q=0;q<noteCount;++q)if(voices[size_t(q)].active)held=q;
            if(held>=0&&voices[size_t(held)].age>analysisRate*.070){
                if(onsetEnvelope<slowEnvelope*.65f)monoWithdrawalSamples=std::min(monoWithdrawalSamples+1,windowSize*16);
                else monoWithdrawalSamples=0;
                if(monoWithdrawalSamples>=std::max(hopSize,int(std::ceil(measuredPeriod))))monoSourceWithdrawn=true;
            }
            if(monoSourceWithdrawn&&phaseAttack'''+('&&view.confidence<=.985f' if bypass else '')+'''){phaseAttack=false;attackRemaining=0;}
        }
        '''
 s=s.replace('if(candidateProtectionRemaining>0)--candidateProtectionRemaining;',block+'if(candidateProtectionRemaining>0)--candidateProtectionRemaining;')
 s=s.replace('attackRemaining=int(analysisRate*.045);attackCooldown=int(analysisRate*.075);phaseAttack=false;','attackRemaining=int(analysisRate*.045);attackCooldown=int(analysisRate*.075);phaseAttack=false;monoSourceWithdrawn=false;monoWithdrawalSamples=0;')
 s=s.replace('&&voices[size_t(active)].age>analysisRate*.07) {','&&voices[size_t(active)].age>analysisRate*.07&&(config.polyphonic||('+rule+'))) {')
 s=s.replace('if(!config.polyphonic){phaseAttack=false;attackRemaining=0;measuredPeriod=', 'if(!config.polyphonic){phaseAttack=false;attackRemaining=0;monoSourceWithdrawn=false;monoWithdrawalSamples=0;measuredPeriod=')
 f.write_text(s)
 shutil.copyfile(old/'baseline/capture.cpp',out/'capture.cpp')
(b/'withdrawal-predeclared.json').write_text(json.dumps({'created_at_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'scope':'Private causal SAME-anchor lifecycle experiments; no production edit','baseline':'e235c8a098dadce6e0a4b8bd6ca9de789a5d174d','source_rule':'existing onset/slow<.65 for one measuredperiod after70msheld marksrapidwithdrawal; amplitude attack1.9 or actualnewNote resets; do notchangeNoteOff timing','variants':{'withdrawal-no-phase':'suppress post-withdrawal phase-arm entirely','withdrawal-coherent-phase':'allow existingvery-highly-periodic confidence>.985 bypass'},'falsification':'Mustpreserve pairedold107 andexisting35msnotes; explicitcleanconstant-energy independent-phase repick after20dBwithdrawal willtestsilentveto flaw before adoption','reserved_detector_runs':0},indent=2)+'\n')
