#!/usr/bin/env python3
"""Regenerate the original diagnostic signals; no detector output informs them."""
from pathlib import Path
import argparse,array,math,wave,hashlib,json

def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--output',type=Path,required=True);args=parser.parse_args()
    output=args.output.resolve();output.mkdir(parents=True,exist_ok=True)
    if any(output.iterdir()):parser.error('Refuse an existing nonempty output directory')
    manifest={}
    for label,upper in [('two-string-45-64',True),('single45-strong-third',False)]:
        samples=array.array('h');moving=fixed=0.
        for n in range(55200):
            t=n/48000
            if t>=.9:value=0.
            else:
                current=45 if t<.4 else 57
                moving+=2*math.pi*440*2**((current-69)/12)/48000
                fixed+=2*math.pi*440*2**((64-69)/12)/48000
                tone=lambda p:math.sin(p)+.2*math.sin(2*p+.2)+.08*math.sin(3*p+.4)
                envelope=min(1.,t/.002)*math.exp(-.18*t)
                if upper:value=envelope*(.15*tone(moving)+.11*tone(fixed))
                else:value=envelope*(.15*math.sin(moving)+.03*math.sin(2*moving+.2)+.122*math.sin(3*moving+.4)+.022*math.sin(6*moving+.2)+.0088*math.sin(9*moving+.4))
            samples.append(round(max(-1,min(1,value))*32767))
        path=output/(label+'.wav')
        with wave.open(str(path),'wb') as writer:
            writer.setnchannels(1);writer.setsampwidth(2);writer.setframerate(48000);writer.writeframes(samples.tobytes())
        manifest[path.name]=hashlib.sha256(path.read_bytes()).hexdigest()
    (output/'sha256.json').write_text(json.dumps(manifest,indent=2)+'\n')

if __name__=='__main__':main()
