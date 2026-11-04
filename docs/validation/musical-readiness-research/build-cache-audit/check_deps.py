#!/usr/bin/env python3
"""Read-only Ninja deps v4 structural, path-ID checksum and reference check."""
from pathlib import Path
import json,struct,sys
p=Path(sys.argv[1]);data=p.read_bytes();bad=[];at=16;paths=[];deps=0
if data[:12]!=b'# ninjadeps\n' or len(data)<16 or struct.unpack_from('<I',data,12)[0]!=4:
    raise SystemExit('Unsupported Ninja dependency log signature/version')
while at<len(data):
    start=at
    if at+4>len(data):bad.append({'offset':at,'problem':'partial length'});break
    word,=struct.unpack_from('<I',data,at);at+=4;size=word&0x7fffffff
    if size%4 or at+size>len(data):
        bad.append({'offset':start,'problem':'invalid/truncated record','size':size});break
    body=data[at:at+size];at+=size
    if word>>31:
        if size<12:bad.append({'offset':start,'problem':'short dependency record'});continue
        ids=[struct.unpack_from('<I',body)[0],*struct.unpack_from('<'+'I'*((size-12)//4),body,12)]
        invalid=[value for value in ids if value>=len(paths)]
        if invalid:bad.append({'offset':start,'problem':'out-of-range dependency path IDs','ids':invalid[:8]})
        deps+=1
    else:
        if size<4:bad.append({'offset':start,'problem':'short path record'});continue
        checksum,=struct.unpack_from('<I',body,size-4);encoded_id=(~checksum)&0xffffffff
        if encoded_id!=len(paths):
            bad.append({'offset':start,'problem':'path ID checksum mismatch','expected_id':len(paths),'encoded_id':encoded_id,'path':body[:-4].rstrip(b'\0').decode(errors='replace')})
        paths.append(body[:-4].rstrip(b'\0'))
print(json.dumps({'file':str(p),'bytes':len(data),'parsed_to':at,'paths':len(paths),'dependency_records':deps,'problems':bad},indent=2))
sys.exit(bool(bad))
