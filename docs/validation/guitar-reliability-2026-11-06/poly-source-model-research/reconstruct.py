#!/usr/bin/env python3
"""Reconstruct/verify a retained private poly research snapshot, without mutation."""
from pathlib import Path,PurePosixPath
import argparse,hashlib,json,shutil
ROOT=Path(__file__).resolve().parent

def require(value,message):
    if not value:raise ValueError(message)

def source_bytes(record):
    relative=PurePosixPath(record['retained_path'])
    require(not relative.is_absolute() and '..' not in relative.parts,'Unsafe retained path')
    path=ROOT.joinpath(*relative.parts)
    require(path.is_file() and not path.is_symlink(),'Missing or linked retained file: '+str(relative))
    data=path.read_bytes()
    require(hashlib.sha256(data).hexdigest()==record['sha256'],'Retained hash mismatch: '+str(relative))
    return data

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--variant',default='baseline')
    parser.add_argument('--output',type=Path)
    parser.add_argument('--verify-all',action='store_true')
    args=parser.parse_args();manifest=json.loads((ROOT/'research-snapshots.json').read_text())
    baseline={name:{'sha256':digest,'retained_path':'baseline/'+name} for name,digest in manifest['baseline_git_bytes_verified'].items()}
    variants={'baseline':baseline,**{name:v['complete_source_bindings'] for name,v in manifest['variants'].items()}}
    if args.verify_all:
        total=0
        for bindings in variants.values():
            for record in bindings.values():source_bytes(record);total+=1
        print('Verified',total,'source bindings across',len(variants),'private snapshots')
    if args.output is None:
        require(args.verify_all,'Use --output <fresh-directory> or --verify-all');return
    require(args.variant in variants,'Unknown variant: '+args.variant)
    output=args.output.resolve();require(not output.exists(),'Refusing an existing output directory')
    output.mkdir(parents=True)
    for name,record in variants[args.variant].items():
        relative=PurePosixPath(name);require(not relative.is_absolute() and relative.parts[0]=='Source' and '..' not in relative.parts,'Unsafe source path')
        path=output.joinpath(*relative.parts);path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(source_bytes(record))
    for path in (ROOT/'drivers').glob('*.cpp'):shutil.copyfile(path,output/path.name)
    command=['g++','-std=c++17','-O2','-DNDEBUG','-I',str(output/'Source'),str(output/'ablation_probe.cpp'),*[str(output/p) for p in manifest['compiled_cpp_relative_paths']],'-o',str(output/'probe')]
    print(json.dumps({'variant':args.variant,'output':str(output),'example_compile_argv':command},indent=2))
if __name__=='__main__':main()
