from pathlib import Path
import shutil,json,hashlib,datetime
b=Path('/tmp/flysnapper-v4-real-repeat-audit');root=Path('/workspace/ChatGPT')
for name in ['raw-onset-denominator','raw-refined-period']:
 out=b/name;shutil.copytree(b/'innovation-raw/Source',out/'Source',dirs_exist_ok=True);shutil.copyfile(b/'innovation-raw/capture.cpp',out/'capture.cpp');shutil.copyfile(b/'innovation-raw/legacy_driver.cpp',out/'legacy_driver.cpp')
 f=out/'Source/Pitch/PitchDetector.cpp';s=f.read_text()
 if name=='raw-onset-denominator':
  match='float ratio=innovationEnvelope/std::max(envelope,gate);';assert s.count(match)==1;s=s.replace(match,'float ratio=innovationEnvelope/std::max(config.polyphonic?envelope:onsetEnvelope,gate);')
 else:
  h=out/'Source/Pitch/PitchDetector.h';text=h.read_text().replace('bool configured=false,needsReset=false;','float innovationPeriod=0;int innovationPeriodNote=-1;\n    bool configured=false,needsReset=false;');h.write_text(text)
  s=s.replace('view={};needsReset=false;','view={};needsReset=false;innovationPeriod=0;innovationPeriodNote=-1;')
  match='float lag=measuredPeriodNote==active&&measuredPeriod>0?measuredPeriod:float(analysisRate)/frequency[size_t(active)];';assert s.count(match)==1;s=s.replace(match,'float lag=!config.polyphonic&&innovationPeriodNote==active&&innovationPeriod>0?innovationPeriod:measuredPeriodNote==active&&measuredPeriod>0?measuredPeriod:float(analysisRate)/frequency[size_t(active)];')
  marker='float narrowTrackingConfidence=0;';assert s.count(marker)==1;s=s.replace(marker,'''innovationPeriod=0;innovationPeriodNote=-1;
    if(held>=0&&measuredPeriod>0&&measuredPeriodNote==held){
        const float expected=float(69+12*std::log2(analysisRate/(440*measuredPeriod)));
        const auto predictorPeriod=monoEstimator.track(rawReference.data(),count,expected,.8f,.985f);
        if(predictorPeriod.valid){innovationPeriod=float(analysisRate/predictorPeriod.frequency);innovationPeriodNote=held;}
    }
    float narrowTrackingConfidence=0;''')
 f.write_text(s)
(d:=b/'normalization-predeclared.json').write_text(json.dumps({'created_at_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'baseline':'e235c8a098dadce6e0a4b8bd6ca9de789a5d174d','known_raw_innovation_regression':'hidden_recorded_01_mono falseOff/On183.979ms despite .990-.996optimalrawcorrelation; waveformtroughcorrelation suggests numerator/denominatorbandwidthmismatch','hypotheses':{'raw-onset-denominator':'RawNCinnovation divided by existingperiod-smoothedRAWhostonsetEnvelope, no detectionthreshold change. Poly unchanged.','raw-refined-period':'RawNCinnovation usesseparatelyrefinedrawperiodonlywhentrackconf>.985; pitch/MIDIidentity unchanged, denominatororiginal. Existing.985 precisioncriterion.'},'no_threshold_grid':True,'no_coherence_admission_veto':True,'must_preserve':'107OFFtrue/104DEFAULT+restoretargetswithoutnewfalse; all14baselinequietpostdroptruecounter configurations; realDEVtailfalsemustnotworsen;35mssteps no newmiss','production_source_changed':False,'reserved_detector_runs':0},indent=2)+'\n')
# Expanded trace of rawinnovation BEFOREanynormalization/refinement
s=(b/'legacy_evidence_trace.cpp').read_text().replace('raw_highly_periodic\\n','raw_highly_periodic,host_envelope,onset_envelope,slow_envelope,innovation_envelope\\n');s=s.replace("<<rawEvidence.harmonicFraction<<','<<rawEvidence.phaseCoherence<<','<<pure<<'\\n';","<<rawEvidence.harmonicFraction<<','<<rawEvidence.phaseCoherence<<','<<pure<<','<<detector->envelope<<','<<detector->onsetEnvelope<<','<<detector->slowEnvelope<<','<<detector->innovationEnvelope<<'\\n';");(b/'raw_arm_trace.cpp').write_text(s)
print(d)
