#!/usr/bin/env python3
"""Build and run the independent audio evaluator, recording exact source hashes.
SPDX-License-Identifier: AGPL-3.0-or-later
Measurement completion is not product acceptance or physical latency verification.
"""
from pathlib import Path
import argparse,datetime,hashlib,json,os,platform,shutil,subprocess,sys

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output',type=Path,required=True)
    parser.add_argument('--full',action='store_true')
    parser.add_argument('--compiler',default=os.environ.get('CXX','g++'))
    parser.add_argument('--executable',type=Path,help='Use a separately built native evaluator instead of compiling it')
    args=parser.parse_args();root=Path(__file__).resolve().parents[1];out=args.output.resolve();out.mkdir(parents=True,exist_ok=True)
    subprocess.run([sys.executable,str(root/'Tools/audio_validation_verify.py')],check=True)
    detector_sources=sorted((root/'Source/Pitch').glob('*.cpp'))
    sources=[root/'Tools/audio_validation.cpp',*detector_sources,*sorted((root/'Source/Pitch').glob('*.h'))]
    def hashes():return {str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}
    before=hashes()
    if args.executable:
        executable=args.executable.resolve();compiler_version='Externally built executable, source hashes at execution time only'
    else:
        compiler=shutil.which(args.compiler)
        if not compiler:raise SystemExit('C++17 compiler unavailable: '+args.compiler)
        compiler_version=subprocess.check_output([compiler,'--version'],text=True).splitlines()[0]
        executable=root/'build'/'audio-validation'/out.name/('AudioValidation.exe' if os.name=='nt' else 'AudioValidation')
        executable.parent.mkdir(parents=True,exist_ok=True)
        command=[compiler,'-std=c++17','-O2','-DNDEBUG','-I',str(root/'Source'),str(root/'Tools/audio_validation.cpp'),*[str(p) for p in detector_sources],'-o',str(executable)]
        subprocess.run(command,check=True)
        if hashes()!=before:raise SystemExit('Detector changed during compilation; rerun against a stable source snapshot')
    command=[str(executable),str(root/'Tests/AudioFixtures'),str(out)]
    if args.full:command.append('full')
    manifest={'recorded_at_utc':datetime.datetime.now(datetime.timezone.utc).isoformat(),'platform':platform.platform(),'compiler':compiler_version,'source_sha256':before,'binary_sha256':hashlib.sha256(executable.read_bytes()).hexdigest(),'fixtures_sha256':hashlib.sha256((root/'Tests/AudioFixtures/fixtures.json').read_bytes()).hexdigest(),'configuration':'full:44100/48000/96000Hz x32/64/128samples' if args.full else 'quick:48000Hz/64samples','measurement_not_acceptance':True,'minimum_useful_correct_note_ms':20}
    result=subprocess.run(command);manifest['runner_exit_code']=result.returncode
    manifest['source_changed_during_measurement']=hashes()!=before
    (out/'measurement-manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    if result.returncode:raise SystemExit(result.returncode)
    print((out/'audio-summary.txt').read_text())

if __name__=='__main__':main()
