#!/usr/bin/env python3
"""Package genuinely built/tested Windows x64 Flysnapper without user build tools.
SPDX-License-Identifier: AGPL-3.0-or-later
"""
from __future__ import annotations
import argparse,hashlib,importlib.util,json,re,stat,struct,tempfile
from pathlib import Path,PurePosixPath
from zipfile import ZipFile,ZipInfo,ZIP_DEFLATED
import xml.etree.ElementTree as ET
ROOT=Path(__file__).resolve().parents[1]
BINARY='Contents/x86_64-win/Flysnapper.vst3'

def safe_relative(name: str) -> str:
    path=PurePosixPath(name)
    reserved={'CON','PRN','AUX','NUL'}|{f'{p}{i}' for p in ('COM','LPT') for i in range(1,10)}
    if (not name or path.is_absolute() or any(p in ('','.', '..') or p.endswith((' ','.'))
        or p.split('.',1)[0].upper() in reserved for p in name.split('/'))
        or any(c in name for c in '\\\r\n\0:<>"|?*')):
        raise ValueError('Unsafe package path: '+repr(name))
    return name

def files_below(root: Path) -> dict[str,bytes]:
    if root.is_symlink() or not root.is_dir():raise ValueError('Expected a regular bundle directory')
    files={};folded=set()
    for path in sorted(root.rglob('*')):
        if path.is_symlink():raise ValueError('Links are not accepted in an installation bundle')
        if path.is_file():
            name=safe_relative(path.relative_to(root).as_posix())
            if name.casefold() in folded:raise ValueError('Case-colliding installation paths')
            folded.add(name.casefold());files[name]=path.read_bytes()
    return files

def verify_pe(data: bytes) -> None:
    if len(data)<64 or data[:2]!=b'MZ':raise ValueError('Not a Windows PE binary')
    offset=struct.unpack_from('<I',data,60)[0]
    if offset+6>len(data) or data[offset:offset+4]!=b'PE\0\0' or struct.unpack_from('<H',data,offset+4)[0]!=0x8664:
        raise ValueError('Expected a Windows x64 PE binary')

def verified_payload(dist: Path,version: str) -> tuple[dict[str,bytes],dict]:
    if dist.is_symlink():raise ValueError('Symlink distribution directory is not accepted')
    manifest_path=dist/'build-manifest.json'
    if manifest_path.is_symlink():raise ValueError('Symlink build manifest is not accepted')
    manifest=json.loads(manifest_path.read_text(encoding='utf-8'))
    if manifest.get('platform')!='Windows-x64' or manifest.get('tests')!='passed':
        raise ValueError('A passed, actual Windows x64 build is required')
    if manifest.get('version')!=version:raise ValueError('Build version differs from the dependency/project lock')
    if manifest.get('windows_verified') is not True or manifest.get('toolchain_probe')!='passed' or manifest.get('juceaide_unicode_probe')!='passed':
        raise ValueError('Native Windows toolchain and Unicode probes must pass')
    if manifest.get('pitch_detection_implemented') is not True or 'prototype' in str(manifest.get('stage','')).lower():
        raise ValueError('The old routing-only prototype cannot be shipped as the complete product')
    required=manifest.get('required_test_names')
    if not isinstance(required,list) or len(set(required))!=len(required) or not {'FlysnapperCore','FlysnapperYin','FlysnapperPitch','FlysnapperPluginSmoke','FlysnapperProcessorAudio','FlysnapperActualVst3','FlysnapperAudioQuality'}<=set(required):
        raise ValueError('Missing required validation target names')
    report=dist/'test-results.xml'
    if report.is_symlink():raise ValueError('Symlink test report is not accepted')
    cases=ET.parse(report).findall('.//testcase')
    passed={c.get('name') for c in cases if c.find('failure') is None and c.find('error') is None
            and c.find('skipped') is None and c.get('status','run') not in ('notrun','disabled')}
    if not set(required)<=passed:raise ValueError('All required test cases must finish without failure or skip')
    if any(sum(c.get('name')==name for c in cases)!=1 for name in required):raise ValueError('Required test cases must occur exactly once')
    if any(c.find('failure') is not None or c.find('error') is not None for c in cases):raise ValueError('Test report contains a failure')
    bundle=files_below(dist/'Flysnapper.vst3')
    actual={n:hashlib.sha256(b).hexdigest() for n,b in bundle.items()}
    if not isinstance(manifest.get('bundle_sha256'),dict) or actual!=manifest['bundle_sha256']:
        raise ValueError('Built bundle inventory or SHA256 differs from passed-test manifest')
    if BINARY not in bundle:raise ValueError('Windows VST3 binary missing')
    verify_pe(bundle[BINARY])
    if hashlib.sha256(bundle[BINARY]).hexdigest()!=manifest.get('vst3_sha256'):
        raise ValueError('Windows binary hash differs from passed-test manifest')
    return bundle,manifest

def verified_audio_evidence(root: Path,dist: Path) -> dict[str,bytes]:
    evidence=files_below(dist/'audio-validation')
    summary=json.loads(evidence['summary.json'])
    proof=json.loads(evidence['report-manifest.json'])
    if summary.get('gate')!='FlysnapperAudioQuality' or summary.get('status')!='passed' or summary.get('cases_failed')!=0 or not isinstance(summary.get('cases_checked'),int) or summary['cases_checked']<=0:
        raise ValueError('Independent audio quality gate did not pass')
    if proof.get('status')!='passed' or proof.get('evaluator_exit_code')!=0:
        raise ValueError('Independent audio evaluator proof did not pass')
    for filename,key in (('summary.json','summary_sha256'),('metrics.csv','metrics_sha256')):
        if hashlib.sha256(evidence[filename]).hexdigest()!=proof.get(key):raise ValueError('Audio evidence checksum mismatch: '+filename)
    recorded=proof.get('source_sha256')
    if not isinstance(recorded,dict):raise ValueError('Audio evidence source provenance is incomplete')
    # Older Windows reports recorded native Path strings. Accept their safe
    # representation without altering hashes, source bytes, quality thresholds
    # or evidence; new producers always write portable forward-slash paths.
    source={};folded=set()
    for native_name,digest in recorded.items():
        if not isinstance(native_name,str):raise ValueError('Invalid audio evidence source path')
        name=safe_relative(native_name.replace('\\','/'))
        if name.casefold() in folded:raise ValueError('Duplicate audio evidence source path')
        folded.add(name.casefold());source[name]=digest
    required={'Source/Pitch/PitchDetector.cpp','Source/Pitch/PitchDetector.h','Tools/audio_quality_gate.py','Tools/audio_validation.cpp'}
    if not required<=source.keys():raise ValueError('Audio evidence source provenance is incomplete')
    for name,digest in source.items():
        path=root/name
        if path.is_symlink() or not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest()!=digest:
            raise ValueError('Audio evidence belongs to different source: '+name)
    for name,key in (('Tests/AudioFixtures/quality_gate.json','policy_sha256'),('Tests/AudioFixtures/fixtures.json','fixture_manifest_sha256'),('Tests/AudioFixtures/fixtures.tsv','fixture_inventory_sha256')):
        path=root/name
        if path.is_symlink() or hashlib.sha256(path.read_bytes()).hexdigest()!=proof.get(key):raise ValueError('Audio evidence policy/fixture provenance differs: '+name)
    return evidence

def build_package(root: Path,dist: Path,output: Path) -> Path:
    root=root.resolve(strict=True)
    version=json.loads((root/'dependencies.lock.json').read_text(encoding='utf-8')).get('project_version','')
    if not re.fullmatch(r'\d+\.\d+\.\d+(?:[-+][A-Za-z0-9.-]+)?',version):raise ValueError('Invalid project version')
    bundle,build=verified_payload(dist,version)
    audio_evidence=verified_audio_evidence(root,dist)
    diagnostic_spec=importlib.util.spec_from_file_location('flysnapper_windows_diagnostics',Path(__file__).with_name('windows_audio_diagnostics.py'))
    diagnostics=importlib.util.module_from_spec(diagnostic_spec);diagnostic_spec.loader.exec_module(diagnostics)
    diagnostic_evidence=diagnostics.verify_saved(root,dist,build)
    for p in (root/'Tools/windows_installer.ps1',root/'INSTALL_FLYSNAPPER.bat'):
        if p.is_symlink() or not p.is_file():raise ValueError('Native installer missing or linked')
    members={'Flysnapper.vst3/'+n:b for n,b in bundle.items()}
    members['Install-Flysnapper.ps1']=(root/'Tools/windows_installer.ps1').read_bytes()
    members['INSTALL_FLYSNAPPER.bat']=(root/'INSTALL_FLYSNAPPER.bat').read_bytes()
    for n in ('LICENSE.txt','THIRD_PARTY.md'):members[n]=(root/n).read_bytes()
    members['BRUKERVEILEDNING.md']=(root/'docs/BRUKERVEILEDNING.md').read_bytes()
    members['Validation/build-manifest.json']=json.dumps(build,indent=2,ensure_ascii=False).encode('utf-8')+b'\n'
    members['Validation/test-results.xml']=(dist/'test-results.xml').read_bytes()
    members['Validation/automated-tests.txt']=(dist/'automated-tests.txt').read_bytes()
    members.update({'Validation/audio-validation/'+name:data for name,data in audio_evidence.items()})
    members.update({'Validation/windows-audio-diagnostics/'+name:data for name,data in diagnostic_evidence.items()})
    members['LES_MEG.txt']=(f'''Flysnapper {version} - Windows x64 VST3\r\n\r\nPakk ut HELE ZIP-filen til en egen mappe, for eksempel pa D:.\r\nLukk Ableton og andre pluginverter. Dobbeltklikk INSTALL_FLYSNAPPER.bat.\r\nVelg VST3-mappen som Ableton bruker. Installeren kopierer bare Flysnapper.vst3.\r\nIngen Python, CMake eller Visual Studio kreves.\r\nVed erstatning beholdes en kontrollert sikkerhetskopi utenfor VST3-mappen.\r\nBuffkiller og andre plugins blir ikke endret.\r\n\r\nKanal 1 er spille-MIDI, kanal 16 er akkordkontroll. Velg sidechain-audio\r\ni vertens eget panel. Mono/poly velges i Flysnapper.\r\nSe BRUKERVEILEDNING.md for ruting, betjening og maalte grenser.\r\nKildekoden finnes i Source/Flysnapper-Source.zip.\r\nWindows-bygg og automatiske tester finnes under Validation. Dette er ikke\r\ni seg selv dokumentasjon av fysisk lydkortlatency eller en Ableton-test.\r\n''').encode('utf-8-sig')
    spec=importlib.util.spec_from_file_location('flysnapper_source_shipping',root/'Tools/package_source.py')
    source=importlib.util.module_from_spec(spec);spec.loader.exec_module(source)
    with tempfile.TemporaryDirectory(prefix='flysnapper-corresponding-source-') as temp:
        p=Path(temp)/'Flysnapper-Source.zip';source.build_package(root,p);members['Source/Flysnapper-Source.zip']=p.read_bytes()
    shipping={'schema':1,'product':'Flysnapper','version':version,'platform':'Windows-x64','tests':'passed',
              'source_commit':build.get('source_commit'),'files':{n:hashlib.sha256(b).hexdigest() for n,b in sorted(members.items())}}
    members['release-manifest.json']=json.dumps(shipping,indent=2,ensure_ascii=False).encode('utf-8')+b'\n'
    output=output.absolute()
    if output.is_symlink():raise ValueError('Refusing linked ZIP output')
    output.parent.mkdir(parents=True,exist_ok=True);prefix=f'Flysnapper-{version}-Windows-x64/'
    with tempfile.NamedTemporaryFile(dir=output.parent,prefix='.flysnapper-windows-',suffix='.tmp',delete=False) as f:temporary=Path(f.name)
    try:
        with ZipFile(temporary,'w',ZIP_DEFLATED,compresslevel=9) as archive:
            for n,b in sorted(members.items()):
                safe_relative(n);info=ZipInfo(prefix+n,(2026,11,3,0,0,0));info.create_system=3
                info.external_attr=(stat.S_IFREG|0o644)<<16;info.compress_type=ZIP_DEFLATED
                archive.writestr(info,b,compress_type=ZIP_DEFLATED,compresslevel=9)
        verify_package(temporary);temporary.replace(output)
    finally:temporary.unlink(missing_ok=True)
    return output

def verify_package(path: Path) -> None:
    with ZipFile(path) as archive:
        names=archive.namelist()
        if not names or len(set(names))!=len(names):raise ValueError('Missing or duplicate ZIP members')
        for n in names:
            safe_relative(n)
            if stat.S_ISLNK(archive.getinfo(n).external_attr>>16):raise ValueError('Linked ZIP member')
        prefixes={n.split('/',1)[0] for n in names}
        if len(prefixes)!=1:raise ValueError('Mixed ZIP roots')
        prefix=next(iter(prefixes))+'/'
        release=json.loads(archive.read(prefix+'release-manifest.json'))
        actual={n[len(prefix):] for n in names if n!=prefix+'release-manifest.json'}
        if actual!=release['files'].keys():raise ValueError('Release ZIP inventory differs from manifest')
        for n,d in release['files'].items():
            if hashlib.sha256(archive.read(prefix+n)).hexdigest()!=d:raise ValueError('Release ZIP checksum failed: '+n)
        verify_pe(archive.read(prefix+'Flysnapper.vst3/'+BINARY))

def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--root',type=Path,default=ROOT)
    parser.add_argument('--dist',type=Path);parser.add_argument('--output',type=Path);a=parser.parse_args(argv)
    try:result=build_package(a.root,a.dist or a.root/'dist',a.output or a.root/'dist/Flysnapper-Windows-x64.zip')
    except (ValueError,OSError,ET.ParseError) as e:parser.exit(1,f'Windows packaging failed: {e}\n')
    print('Verified Windows installation package:',result);print('SHA256:',hashlib.sha256(result.read_bytes()).hexdigest());return 0
if __name__=='__main__':raise SystemExit(main())
