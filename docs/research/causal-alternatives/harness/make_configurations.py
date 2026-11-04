#!/usr/bin/env python3
"""Create a private case inventory; never modifies fixtures or downloads assets."""
from pathlib import Path
import argparse,hashlib,json,wave

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repository-root',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args();repository=args.repository_root.resolve(strict=True)
    output=args.output.resolve();output.mkdir(parents=True,exist_ok=True)
    if any(output.iterdir()):parser.error('Refuse an existing nonempty output directory')
    root=repository/'Tests/AudioFixtures';inputs={};cases=[];fixtures={}
    for label,folder in [('public',root),('holdout',root/'holdout'),('confirmation',root/'confirmation'),('recorded_noise',root/'readiness/recorded_noise')]:
        file=folder/'fixtures.json';manifest=json.loads(file.read_text());fixtures[label]=hashlib.sha256(file.read_bytes()).hexdigest()
        for case in manifest['cases']:
            audio=(folder/case['file']).resolve(strict=True)
            if not audio.is_relative_to(folder.resolve()):raise ValueError('Fixture escapes its dataset directory')
            identity=str(audio)
            if identity not in inputs:
                with wave.open(str(audio),'rb') as reader:
                    if reader.getframerate()!=48000 or reader.getnchannels()!=1:raise ValueError('Expected original mono48-kHz inputs')
                inputs[identity]={'id':label+':'+case['file'],'file':identity,'sha256':hashlib.sha256(audio.read_bytes()).hexdigest()}
            cases.append({'dataset':label,'input_id':inputs[identity]['id'],'case':case})
    for model in ('fast','piano'):
        document={'model':model,'block':64,'scope':'All disclosed public196 plus recordednoise128; integer-key decoder unmodified operating point','fixture_sha256':fixtures,'inputs':list(inputs.values()),'cases':cases}
        (output/(model+'.json')).write_text(json.dumps(document,indent=2)+'\n')
    print('Prepared',len(cases),'cases from',len(inputs),'unchanged PCM inputs')

if __name__=='__main__':main()
