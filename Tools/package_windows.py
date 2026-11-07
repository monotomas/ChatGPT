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
BINARY='Contents/x86_64-win/Flysnapper.vst3'  # Legacy 0.3 package identity.
MIDI_BUNDLE='Flysnapper MIDI.vst3'
MIDI_BINARY='Contents/x86_64-win/Flysnapper MIDI.vst3'
MIDI_VERSIONS=('0.4.0','0.4.1','0.4.2','0.4.3')  # Fixed identity, including archived releases.

def dynamics_evidence_module():
    spec=importlib.util.spec_from_file_location('flysnapper_input_dynamics_evidence',Path(__file__).with_name('input_dynamics_evidence.py'))
    result=importlib.util.module_from_spec(spec);spec.loader.exec_module(result)
    return result

def midi_evidence_module():
    spec=importlib.util.spec_from_file_location('flysnapper_midi_only_evidence',Path(__file__).with_name('midi_only_evidence.py'))
    result=importlib.util.module_from_spec(spec);spec.loader.exec_module(result)
    return result

def bundle_identity(manifest):
    if manifest.get('version') in MIDI_VERSIONS:
        if manifest.get('bundle_name')!=MIDI_BUNDLE or manifest.get('product')!='Flysnapper MIDI':
            raise ValueError('The MIDI-only product must have its separate bundle identity')
        return MIDI_BUNDLE,MIDI_BINARY
    if manifest.get('bundle_name','Flysnapper.vst3')!='Flysnapper.vst3':
        raise ValueError('Unexpected legacy bundle identity')
    return 'Flysnapper.vst3',BINARY

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
    midi_only=version in MIDI_VERSIONS
    if midi_only:
        midi=midi_evidence_module()
        if (version!=midi.VERSION or manifest.get('pitch_detection_implemented') is not False or manifest.get('audio_processing_implemented') is not False
                or manifest.get('stage')!=midi.STAGE or manifest.get('evaluation_scope')!=midi.SCOPE
                or manifest.get('ableton_verified') is not False):
            raise ValueError('The MIDI-only native validation scope is missing or different')
        needed=set(midi.REQUIRED_TESTS)
    else:
        if manifest.get('pitch_detection_implemented') is not True or 'prototype' in str(manifest.get('stage','')).lower():
            raise ValueError('An unfinished prototype cannot be shipped as the complete product')
        dynamics=dynamics_evidence_module()
        if version=='0.3.0' and manifest.get('stage')!=dynamics.EVALUATION_STAGE:
            raise ValueError('Flysnapper 0.3.0 requires the qualified real-guitar evaluation stage')
        if manifest.get('stage')==dynamics.EVALUATION_STAGE and manifest.get('evaluation_scope')!=dynamics.EVALUATION_SCOPE:
            raise ValueError('The dated real-guitar evaluation scope is missing or different')
        needed={'FlysnapperCore','FlysnapperYin','FlysnapperPitch','FlysnapperPluginSmoke','FlysnapperProcessorAudio','FlysnapperActualVst3','FlysnapperAudioQuality','FlysnapperGuitarQuality','FlysnapperBendProcessor','FlysnapperNoise','FlysnapperInputDynamics','FlysnapperDynamicsProcessor','FlysnapperMpeOutput','FlysnapperMpeProcessor','FlysnapperMpeActualVst3'}
    required=manifest.get('required_test_names')
    if not isinstance(required,list) or len(set(required))!=len(required) or not needed<=set(required):
        raise ValueError('Missing required validation target names')
    bundle_name,binary_name=bundle_identity(manifest)
    report=dist/'test-results.xml'
    if report.is_symlink():raise ValueError('Symlink test report is not accepted')
    cases=ET.parse(report).findall('.//testcase')
    passed={c.get('name') for c in cases if c.find('failure') is None and c.find('error') is None
            and c.find('skipped') is None and c.get('status','run') not in ('notrun','disabled')}
    if not set(required)<=passed:raise ValueError('All required test cases must finish without failure or skip')
    if any(sum(c.get('name')==name for c in cases)!=1 for name in required):raise ValueError('Required test cases must occur exactly once')
    if any(c.find('failure') is not None or c.find('error') is not None for c in cases):raise ValueError('Test report contains a failure')
    bundle=files_below(dist/bundle_name)
    actual={n:hashlib.sha256(b).hexdigest() for n,b in bundle.items()}
    if not isinstance(manifest.get('bundle_sha256'),dict) or actual!=manifest['bundle_sha256']:
        raise ValueError('Built bundle inventory or SHA256 differs from passed-test manifest')
    if binary_name not in bundle:raise ValueError('Windows VST3 binary missing')
    verify_pe(bundle[binary_name])
    if hashlib.sha256(bundle[binary_name]).hexdigest()!=manifest.get('vst3_sha256'):
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
    midi_only=version in MIDI_VERSIONS
    bundle_name,binary_name=bundle_identity(build)
    if midi_only:
        midi_evidence=midi_evidence_module().verify(root,dist,build)
    else:
        audio_evidence=verified_audio_evidence(root,dist)
        dynamics=dynamics_evidence_module()
        dynamics_evidence=dynamics.verify(root,dist,build)
        diagnostic_spec=importlib.util.spec_from_file_location('flysnapper_windows_diagnostics',Path(__file__).with_name('windows_audio_diagnostics.py'))
        diagnostics=importlib.util.module_from_spec(diagnostic_spec);diagnostic_spec.loader.exec_module(diagnostics)
        diagnostic_evidence=diagnostics.verify_saved(root,dist,build)
    for p in (root/'Tools/windows_installer.ps1',root/'INSTALL_FLYSNAPPER.bat'):
        if p.is_symlink() or not p.is_file():raise ValueError('Native installer missing or linked')
    members={bundle_name+'/'+n:b for n,b in bundle.items()}
    members['Install-Flysnapper.ps1']=(root/'Tools/windows_installer.ps1').read_bytes()
    members['INSTALL_FLYSNAPPER.bat']=(root/'INSTALL_FLYSNAPPER.bat').read_bytes()
    for n in ('LICENSE.txt','THIRD_PARTY.md'):members[n]=(root/n).read_bytes()
    members['BRUKERVEILEDNING.md']=(root/'docs/BRUKERVEILEDNING.md').read_bytes()
    if not midi_only:members['INPUT_DYNAMICS.md']=(root/'docs/INPUT_DYNAMICS.md').read_bytes()
    members['Validation/build-manifest.json']=json.dumps(build,indent=2,ensure_ascii=False).encode('utf-8')+b'\n'
    members['Validation/test-results.xml']=(dist/'test-results.xml').read_bytes()
    members['Validation/automated-tests.txt']=(dist/'automated-tests.txt').read_bytes()
    if midi_only:
        members.update({'Validation/midi-validation/'+name:data for name,data in midi_evidence.items()})
    else:
        members.update({'Validation/audio-validation/'+name:data for name,data in audio_evidence.items()})
        members.update({'Validation/input-dynamics/'+name:data for name,data in dynamics_evidence.items()})
        members.update({'Validation/windows-audio-diagnostics/'+name:data for name,data in diagnostic_evidence.items()})
    members['LES_MEG.txt']=(f'''Flysnapper {version} - Windows x64 VST3\r\n\r\nPakk ut HELE ZIP-filen til en egen mappe, for eksempel pa D:.\r\nLukk Ableton og andre pluginverter. Dobbeltklikk INSTALL_FLYSNAPPER.bat.\r\nVelg VST3-mappen som Ableton bruker. Installeren kopierer bare Flysnapper.vst3.\r\nIngen Python, CMake eller Visual Studio kreves.\r\nVed erstatning beholdes en kontrollert sikkerhetskopi utenfor VST3-mappen.\r\nBuffkiller og andre plugins blir ikke endret.\r\n\r\nKanal 1 er spille-MIDI, kanal 16 er akkordkontroll. Velg sidechain-audio\r\ni vertens eget panel. Mono/poly velges i Flysnapper.\r\nSe BRUKERVEILEDNING.md for ruting, betjening og maalte grenser.\r\nKildekoden finnes i Source/Flysnapper-Source.zip.\r\nWindows-bygg og automatiske tester finnes under Validation. Dette er ikke\r\ni seg selv dokumentasjon av fysisk lydkortlatency eller en Ableton-test.\r\n''').encode('utf-8-sig')
    members['LES_MEG.txt'] += ('''\r\nFør første gitarprøve: Åpne Input dynamics og trykk Learn / 20 seconds.\r\nDemp strengene i 3 sekunder med vanlig håndkontakt. Spill deretter normalt\r\ni 17 sekunder, med svake/sterke anslag, akkorder og en lang utklang.\r\nEn vellykket prøve stiller de synlige kontrollene. Du kan justere dem etterpå.\r\nGenerert MIDI er dempet under prøven; eksternt spille-MIDI fortsetter.\r\nSe INPUT_DYNAMICS.md for kalibrering, gate, kompressor, limiter og hjelpetekster.\r\nMono har valgfri bending. Poly sender definerte halvtoner uten polyfonisk bend.\r\nForsinkelsen optimaliseres og rapporteres per tone og måleoppsett. Lydkort,\r\nvertens buffere/ruting og mottakerinstrument bidrar til totalforsinkelsen.\r\n''').encode('utf-8')
    if midi_only:
        members['LES_MEG.txt']=(f'''Flysnapper MIDI {version} - Windows x64 VST3

Pakk ut HELE ZIP-filen til en egen mappe. Lukk pluginvertene og kjor
INSTALL_FLYSNAPPER.bat. Velg VST3-mappen som Ableton bruker.
Ingen Python, CMake eller Visual Studio kreves for installasjon.

Denne MIDI-utgaven installeres som Flysnapper MIDI.vst3. Den erstatter
ikke den eldre Flysnapper.vst3-instrumentutgaven eller andre plugins.
Ved ny installasjon av MIDI-utgaven kontrolleres gjenopprettingskopien
utenfor vertens VST3-skannemappe.

Flysnapper MIDI er et VST3-instrument med stille stereo-audioutgang.
Utgangen er en vertstilpasning; pluginen behandler spille-MIDI og
akkordkontroll og genererer ingen instrumentlyd. Den har ingen audioinngang,
sidechain, pitchgjenkjenning eller input dynamics. Spille-MIDI bruker kanal 1;
akkordkontroll bruker kanal 16. Velg Flysnapper som MIDI-kilde for et annet
MIDI-spor, velg pluginens eget uttak og legg lydinstrumentet pa mottakersporet.
Den klassifiseres som instrument, ikke som en ren MIDI-effekt som kan kjedes
direkte med flere enheter pa et rent MIDI-spor.
Se BRUKERVEILEDNING.md for korreksjon, skalaer og ruting.

Validation inneholder native MIDI-prosessor- og faktisk VST3-testvertbevis,
med kilde-/modulhashes. Dette er ikke en utfort Ableton Live-test.
Kildekoden finnes i Source/Flysnapper-Source.zip.
''').replace('\n','\r\n').encode('utf-8-sig')
    spec=importlib.util.spec_from_file_location('flysnapper_source_shipping',root/'Tools/package_source.py')
    source=importlib.util.module_from_spec(spec);spec.loader.exec_module(source)
    with tempfile.TemporaryDirectory(prefix='flysnapper-corresponding-source-') as temp:
        p=Path(temp)/'Flysnapper-Source.zip';source.build_package(root,p);members['Source/Flysnapper-Source.zip']=p.read_bytes()
    shipping={'schema':1,'product':'Flysnapper','version':version,'platform':'Windows-x64','tests':'passed',
              'stage':build.get('stage'),'evaluation_scope':build.get('evaluation_scope'),
              'source_commit':build.get('source_commit'),'bundle_name':bundle_name,'product_name':build.get('product','Flysnapper'),'files':{n:hashlib.sha256(b).hexdigest() for n,b in sorted(members.items())}}
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
        if release.get('version') in MIDI_VERSIONS:
            if release.get('bundle_name')!=MIDI_BUNDLE or release.get('product_name')!='Flysnapper MIDI':raise ValueError('MIDI-only release identity differs')
            verify_pe(archive.read(prefix+MIDI_BUNDLE+'/'+MIDI_BINARY))
        else:
            verify_pe(archive.read(prefix+'Flysnapper.vst3/'+BINARY))

def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--root',type=Path,default=ROOT)
    parser.add_argument('--dist',type=Path);parser.add_argument('--output',type=Path);a=parser.parse_args(argv)
    try:result=build_package(a.root,a.dist or a.root/'dist',a.output or a.root/'dist/Flysnapper-Windows-x64.zip')
    except (ValueError,OSError,ET.ParseError) as e:parser.exit(1,f'Windows packaging failed: {e}\n')
    print('Verified Windows installation package:',result);print('SHA256:',hashlib.sha256(result.read_bytes()).hexdigest());return 0
if __name__=='__main__':raise SystemExit(main())
