#!/usr/bin/env python3
"""Bind MIDI processing and silent instrument-adapter tests to compiled inputs.

SPDX-License-Identifier: AGPL-3.0-or-later
Native test-host execution is not an Ableton Live execution claim.
"""
from __future__ import annotations
import csv
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import re
import shutil
import struct
import xml.etree.ElementTree as ET

COMPILED_INPUTS = 'flysnapper-midi-compiled-inputs.json'
VERSION = '0.4.3'
DIRECTORY = 'midi-validation'
MANIFEST = 'report-manifest.json'
STAGE = 'midi-only-native-validation'
SCOPE = {'feature_set':'midi-to-midi-only', 'audio_processing':False,
         'pitch_to_midi':False, 'scales':38, 'chord_types':52,
         'host_device_role':'instrument', 'silent_audio_output':True,
         'host_scope':'native-processor-and-compiled-vst3-test-host',
         'ableton_verified':False}
BUNDLE = 'Flysnapper MIDI.vst3'
WINDOWS_BINARY = 'Contents/x86_64-win/Flysnapper MIDI.vst3'
LINUX_BINARY = 'Contents/x86_64-linux/Flysnapper MIDI.so'
REQUIRED_TESTS = frozenset({'FlysnapperCore','FlysnapperPluginSmoke',
                           'FlysnapperMidiOnlyProcessor','FlysnapperActualVst3'})
SPECS = (
    ('FlysnapperMidiOnlyProcessor','FlysnapperMidiOnlyProcessorTests',
     'Flysnapper-midi-processor-evaluation.json','Flysnapper-midi-processor-events.csv',
     'FlysnapperMidiOnlyProcessor'),
    ('FlysnapperActualVst3','FlysnapperVst3HostTests',
     'Flysnapper-midi-vst3-evaluation.json','Flysnapper-midi-vst3-events.csv',
     'FlysnapperMidiOnlyVst3'),
)
FEATURES = frozenset({'silent_stereo_audio_output','chord_channel_16','chord_relay',
                      'serial_correction','internal_correction_bypass','instance_state',
                      'legato_directional_ties','same_pitch_rearticulation',
                      'unrecognized_chord_chromatic_fallback',
                      'stopped_transport_chromatic_fallback'})
EXTRA_INPUTS = ('CMakeLists.txt','dependencies.lock.json','build_plugin.py',
                'Tests/CoreTests.cpp','Tests/PluginTests.cpp',
                'Tests/MidiOnlyProcessorTests.cpp','Tests/Vst3HostTests.cpp',
                'Tools/midi_only_evidence.py','Tools/package_windows.py',
                'Tools/package_source.py')


def require(value, message):
    if not value:
        raise ValueError('MIDI-only evidence: ' + message)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def regular(path):
    require(path.is_file() and not path.is_symlink(), 'missing or linked file: ' + str(path))
    return path.read_bytes()


def relative(name):
    require(isinstance(name,str) and name, 'invalid relative path')
    p = PurePosixPath(name)
    require(not p.is_absolute() and all(x not in ('','.','..') for x in name.split('/'))
            and not any(x in name for x in '\\\r\n\0:'), 'unsafe relative path')
    return name


def source_hashes(root):
    paths = {p for p in (root/'Source').rglob('*') if p.suffix in ('.cpp','.h')}
    paths.update(root/n for n in EXTRA_INPUTS)
    require(paths, 'missing project inputs')
    return {p.relative_to(root).as_posix():sha(regular(p)) for p in sorted(paths)}


def native_binary(data, platform):
    if platform == 'Windows-x64':
        require(len(data)>=64 and data[:2]==b'MZ', 'not a Windows native binary')
        at=struct.unpack_from('<I',data,60)[0]
        require(at+6<=len(data) and data[at:at+6]==b'PE\0\0\x64\x86', 'not Windows x64')
    elif platform == 'Linux-x86_64':
        require(len(data)>=64 and data[:6]==b'\x7fELF\x02\x01'
                and struct.unpack_from('<H',data,18)[0]==62, 'not Linux x64')
    else:
        require(False, 'unknown native execution platform')


def module_info(data):
    # JUCE's own moduleinfo generator permits trailing commas. Remove only
    # commas outside quoted strings; do not reinterpret arbitrary string data.
    s=data.decode('utf-8');out=[];quoted=False;escape=False
    for i,c in enumerate(s):
        if quoted:
            out.append(c)
            if escape:escape=False
            elif c=='\\':escape=True
            elif c=='"':quoted=False
        elif c=='"':quoted=True;out.append(c)
        elif c==',':
            at=i+1
            while at<len(s) and s[at].isspace():at+=1
            if at==len(s) or s[at] not in '}]':out.append(c)
        else:out.append(c)
    result=json.loads(''.join(out))
    classes=[c for c in result.get('Classes',[]) if c.get('Category')=='Audio Module Class']
    require(len(classes)==1, 'expected one VST3 component class')
    component=classes[0];categories=component.get('Sub Categories')
    require(component.get('Name')=='Flysnapper MIDI'
            and component.get('Version')==VERSION, 'module product/version differs')
    require(categories==['Instrument'], 'module is not advertised as the silent VST3 instrument adapter')
    require(component.get('CID')=='ABCDEF019182FAEB466C797346736D31',
            'MIDI component identity must retain Fsm1 for the 0.4.0 repair')
    return {'name':component['Name'],'version':component['Version'],
            'component_cid':component['CID'],'sub_categories':categories}


def check_report(report_data, event_data, kind, event_filename):
    r=json.loads(report_data.decode('utf-8'))
    require(isinstance(r,dict) and r.get('schema')==1 and r.get('kind')==kind,
            'unexpected report schema/kind')
    require(r.get('status')=='passed' and type(r.get('checks')) is int and r['checks']>0,
            'native checks did not complete')
    require(r.get('ableton_verified') is False and r.get('pitch_detection_implemented') is False,
            'incorrect DAW/audio scope claim')
    require(r.get('audio_input_channels')==0 and r.get('audio_output_channels')==2
            and r.get('audio_output_silent') is True and r.get('host_device_role')=='instrument'
            and r.get('scale_count')==38 and r.get('chord_type_count')==52,
            'MIDI-only bus/music scope differs')
    features=r.get('verified_features')
    require(isinstance(features,list) and len(features)==len(set(features))
            and FEATURES<=set(features), 'missing required native feature checks')
    if kind=='FlysnapperMidiOnlyProcessor':
        require('chord_filter' in features, 'final chord-control filter was not tested')
    cases=r.get('cases')
    require(isinstance(cases,list) and cases, 'no executed native cases')
    names=[]
    for c in cases:
        require(isinstance(c,dict) and isinstance(c.get('name'),str) and c['name']
                and c.get('status')=='passed', 'unfinished native case')
        names.append(c['name'])
    require(len(names)==len(set(names)), 'duplicate native cases')
    # A logical case can contain several independently clocked processor
    # instances. Bind their actual raw-stream names without collapsing them
    # into the summary case or losing per-instance chronology.
    event_cases=r.get('event_cases',names)
    require(isinstance(event_cases,list) and event_cases
            and all(isinstance(name,str) and name for name in event_cases)
            and len(event_cases)==len(set(event_cases)), 'invalid raw event case inventory')
    require(r.get('events_file')==event_filename and type(r.get('events_rows')) is int
            and r['events_rows']>0, 'missing raw native MIDI events')
    reader=csv.DictReader(io.StringIO(event_data.decode('utf-8'),newline=''))
    fields={'case','rate','block','sample','type','channel','note','value'}
    require(reader.fieldnames and len(reader.fieldnames)==len(set(reader.fieldnames))
            and fields<=set(reader.fieldnames), 'raw event CSV schema')
    rows=0;previous={};seen=set()
    for row in reader:
        require(set(row)==set(reader.fieldnames) and all(isinstance(x,str) for x in row.values()),
                'truncated raw event row')
        require(row['case'] in event_cases, 'raw MIDI case absent from report')
        seen.add(row['case'])
        rate,block,sample,channel,note,value=(int(row[f]) for f in ('rate','block','sample','channel','note','value'))
        require(8000<=rate<=384000 and 1<=block<=8192 and sample>=0
                and 0<=channel<=16 and 0<=note<=127 and 0<=value<=16383,
                'invalid event timing/data')
        require(row['type'] in {'N','F','C','P','A','S','O','note_on','note_off',
                                'control','pitch','aftertouch','sysex','other'},
                'unknown MIDI event type')
        key=(row['case'],rate,block,row.get('instance',''),row.get('stage',''))
        require(sample>=previous.get(key,-1), 'raw event times move backwards')
        previous[key]=sample;rows+=1
    require(rows==r['events_rows'], 'raw event count differs')
    if 'event_cases' in r:
        require(seen==set(event_cases), 'raw event case inventory differs')
    return r


def check_ctest(data):
    cases=ET.fromstring(data).findall('.//testcase')
    for name in REQUIRED_TESTS:
        selected=[c for c in cases if c.get('name')==name]
        require(len(selected)==1, 'required CTest must occur exactly once: '+name)
        c=selected[0]
        require(all(c.find(tag) is None for tag in ('failure','error','skipped'))
                and c.get('status','run') not in ('notrun','disabled'),
                'required CTest did not pass: '+name)
    require(all(c.find('failure') is None and c.find('error') is None for c in cases),
            'CTest report contains a failure')


def _module(dist, manifest):
    require(manifest.get('version')==VERSION, 'instrument adapter version differs')
    platform=manifest.get('platform');name=WINDOWS_BINARY if platform=='Windows-x64' else LINUX_BINARY
    require(manifest.get('bundle_name')==BUNDLE, 'MIDI-only bundle identity differs')
    data=regular(dist/BUNDLE/name);native_binary(data,platform)
    require(sha(data)==manifest.get('vst3_sha256'), 'built module hash differs')
    info_data=regular(dist/BUNDLE/'Contents/Resources/moduleinfo.json')
    return module_info(info_data),sha(info_data)


def collect(root, build, dist, manifest):
    inputs=json.loads(regular(build/COMPILED_INPUTS))
    require(inputs==source_hashes(root), 'inputs changed after compilation started')
    ctest_data=regular(dist/'test-results.xml');check_ctest(ctest_data)
    info,info_sha=_module(dist,manifest)
    files={};executions={}
    for test,target,report_name,event_name,kind in SPECS:
        report_data=regular(build/report_name);events_data=regular(build/event_name)
        report=check_report(report_data,events_data,kind,event_name)
        binary=build/(target+'_artefacts')/'Release'/(target+('.exe' if manifest['platform']=='Windows-x64' else ''))
        binary_data=regular(binary);native_binary(binary_data,manifest['platform'])
        executions[test]={'target':target,'binary':binary.relative_to(root).as_posix(),
                          'binary_sha256':sha(binary_data),'binary_bytes':len(binary_data),'report_sha256':sha(report_data),
                          'events_sha256':sha(events_data),'checks':report['checks'],
                          'events_rows':report['events_rows']}
        files[report_name]=report_data;files[event_name]=events_data
    proof={'schema':1,'kind':'MidiOnlyNativeExecution','status':'passed',
           'scope':dict(SCOPE),'platform':manifest['platform'],
           'source_commit':manifest.get('source_commit'),'source_sha256':inputs,
           'ctest_sha256':sha(ctest_data),'vst3_sha256':manifest['vst3_sha256'],
           'moduleinfo_sha256':info_sha,'module':info,'executions':executions}
    proof_data=(json.dumps(proof,indent=2,ensure_ascii=False)+'\n').encode('utf-8')
    destination=dist/DIRECTORY
    require(not destination.is_symlink(), 'linked evidence destination')
    if destination.exists():shutil.rmtree(destination)
    destination.mkdir(parents=True)
    for name,data in files.items():(destination/name).write_bytes(data)
    (destination/MANIFEST).write_bytes(proof_data)
    return {'path':DIRECTORY+'/'+MANIFEST,'sha256':sha(proof_data)}


def verify(root, dist, manifest):
    base=dist/DIRECTORY
    require(base.is_dir() and not base.is_symlink(), 'missing or linked proof directory')
    files={}
    for p in sorted(base.rglob('*')):
        require(not p.is_symlink(), 'linked proof entry')
        if p.is_file():files[relative(p.relative_to(base).as_posix())]=p.read_bytes()
    expected={MANIFEST}|{n for spec in SPECS for n in spec[2:4]}
    require(set(files)==expected, 'proof inventory differs')
    proof=json.loads(files[MANIFEST])
    require(proof.get('schema')==1 and proof.get('kind')=='MidiOnlyNativeExecution'
            and proof.get('status')=='passed' and proof.get('scope')==SCOPE,
            'native execution scope differs')
    require(proof.get('platform')==manifest.get('platform')
            and proof.get('source_commit')==manifest.get('source_commit'),
            'native build/source context differs')
    require(proof.get('source_sha256')==source_hashes(root), 'current source differs from native execution')
    ctest_data=regular(dist/'test-results.xml');check_ctest(ctest_data)
    require(proof.get('ctest_sha256')==sha(ctest_data), 'CTest proof differs')
    info,info_sha=_module(dist,manifest)
    require(proof.get('vst3_sha256')==manifest.get('vst3_sha256')
            and proof.get('moduleinfo_sha256')==info_sha and proof.get('module')==info,
            'compiled module identity/binding differs')
    executions=proof.get('executions')
    require(isinstance(executions,dict) and set(executions)=={s[0] for s in SPECS},
            'native executable inventory differs')
    for test,target,report_name,event_name,kind in SPECS:
        report=check_report(files[report_name],files[event_name],kind,event_name);run=executions[test]
        require(run.get('target')==target and run.get('report_sha256')==sha(files[report_name])
                and run.get('events_sha256')==sha(files[event_name])
                and run.get('checks')==report['checks'] and run.get('events_rows')==report['events_rows'],
                'native report binding differs')
        name=relative(run.get('binary',''));p=PurePosixPath(name)
        require(p.parts[0]=='build' and p.name==target+('.exe' if manifest['platform']=='Windows-x64' else ''),
                'unexpected native executable')
        data=regular(root/name);native_binary(data,manifest['platform'])
        require(sha(data)==run.get('binary_sha256') and len(data)==run.get('binary_bytes'), 'native executable changed or unavailable')
    require(manifest.get('midi_only_evidence')=={'path':DIRECTORY+'/'+MANIFEST,'sha256':sha(files[MANIFEST])},
            'build does not bind current native MIDI evidence')
    return files
