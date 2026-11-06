#!/usr/bin/env python3
"""Optional source-restoration check; no external recording or reserved input."""
import sys
sys.dont_write_bytecode=True
from pathlib import Path
import argparse,hashlib,json,subprocess,datetime
import verify_restore

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    logical,_,_=verify_restore.verify_archive();out=a.output.resolve()
    if out.exists():raise ValueError('Fresh output required')
    out.mkdir(parents=True);restored=verify_restore.restore(logical,out/'restored')
    prefix='studies/real-repeat/raw-onset-floor';source=restored/prefix/'Source';driver=restored/'studies/real-repeat/quiet_repick_counter.cpp'
    cpp=sorted(p for p in (source/'Pitch').glob('*.cpp') if p.name!='MpeOutputRouter.cpp')
    exe=out/'Counter';cmd=['g++','-std=c++17','-O2','-DNDEBUG','-I',str(source),str(driver),*map(str,cpp),'-o',str(exe)]
    with (out/'compile.txt').open('w') as log:r=subprocess.run(cmd,stdout=log,stderr=subprocess.STDOUT)
    if r.returncode:raise RuntimeError('Restored counter compilation failed')
    result=out/'counter';result.mkdir()
    with (out/'run.txt').open('w') as log:r=subprocess.run([str(exe),str(result)],stdout=log,stderr=subprocess.STDOUT)
    if r.returncode:raise RuntimeError('Restored counter execution failed')
    compared={}
    for name in ('counter-events.csv','counter-results.csv'):
        expected=logical[prefix+'/quiet-counter/'+name];actual=(result/name).read_bytes()
        if actual!=expected:raise ValueError('Deterministic counter differs '+name)
        compared[name]=hashlib.sha256(actual).hexdigest()
    proof={'recorded_at_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'kind':'Fresh archive restoration/rebuild check, not original measurement or product approval','candidate':'REJECTED raw-onset-floor','compiler':subprocess.check_output(['g++','--version'],text=True).splitlines()[0],'compile_command':cmd,'source_logical_prefix':prefix+'/Source','driver_logical_path':'studies/real-repeat/quiet_repick_counter.cpp','counter_files_sha256':compared,'recorded_original_events_exact':True,'reserved_audio_read':False,'physical_latency_verified':False}
    (out/'reproduction-proof.json').write_text(json.dumps(proof,indent=2)+'\n');print('Restored source rebuilt; both original deterministic counter CSVs match exactly. No recording was read.')
if __name__=='__main__':main()
