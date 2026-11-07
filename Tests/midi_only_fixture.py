"""Synthetic packaging-protocol fixture, NEVER a native plugin execution proof.

The real build must separately compile and execute CTest/native VST3 hosts.
"""
from pathlib import Path
import hashlib,json,shutil
from types import SimpleNamespace
from Tools import midi_only_evidence as evidence
from Tools import package_source as source


def make(base, repository):
    root=base/'source';root.mkdir(parents=True)
    for name in source.REQUIRED_FILES:
        p=root/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_text('protocol fixture '+name+'\n',encoding='utf-8')
    for name in ('Tools/package_source.py','Tools/windows_installer.ps1','INSTALL_FLYSNAPPER.bat'):
        shutil.copyfile(repository/name,root/name)
    (root/'dependencies.lock.json').write_text(json.dumps({'project_version':'0.4.4'}))
    dist=root/'dist';bundle=dist/evidence.BUNDLE;binary=bundle/evidence.WINDOWS_BINARY;binary.parent.mkdir(parents=True)
    pe=bytearray(512);pe[:2]=b'MZ';pe[60:64]=(128).to_bytes(4,'little');pe[128:134]=b'PE\0\0\x64\x86';pe.extend(b'PROTOCOL FIXTURE: NOT A NATIVE PLUGIN')
    binary.write_bytes(pe)
    info={'Name':'Flysnapper MIDI','Version':'0.4.4','Classes':[{'CID':'ABCDEF019182FAEB466C797346736D31','Category':'Audio Module Class','Name':'Flysnapper MIDI','Version':'0.4.4','Sub Categories':['Instrument']}]}
    p=bundle/'Contents/Resources/moduleinfo.json';p.parent.mkdir(parents=True);p.write_text(json.dumps(info))
    manifest={'version':'0.4.4','product':'Flysnapper MIDI','bundle_name':evidence.BUNDLE,
              'stage':evidence.STAGE,'evaluation_scope':dict(evidence.SCOPE),'platform':'Windows-x64','tests':'passed',
              'windows_verified':True,'toolchain_probe':'passed','juceaide_unicode_probe':'passed',
              'pitch_detection_implemented':False,'audio_processing_implemented':False,'ableton_verified':False,
              'required_test_names':sorted(evidence.REQUIRED_TESTS),'source_commit':'a'*40,
              'vst3_sha256':hashlib.sha256(pe).hexdigest(),
              'bundle_sha256':{p.relative_to(bundle).as_posix():hashlib.sha256(p.read_bytes()).hexdigest() for p in bundle.rglob('*') if p.is_file()}}
    (dist/'test-results.xml').write_text('<testsuite>'+''.join(f'<testcase name="{n}" status="run"/>' for n in sorted(evidence.REQUIRED_TESTS))+'</testsuite>')
    (dist/'automated-tests.txt').write_text('Synthetic protocol fixture, not native Windows execution.\n')
    build=root/'build/w-fixture';build.mkdir(parents=True)
    (build/evidence.COMPILED_INPUTS).write_text(json.dumps(evidence.source_hashes(root)))
    for test,target,report_name,event_name,kind in evidence.SPECS:
        p=build/(target+'_artefacts')/'Release'/(target+'.exe');p.parent.mkdir(parents=True);p.write_bytes(pe+target.encode())
        name='synthetic_protocol_fixture_'+target
        report={'schema':1,'kind':kind,'status':'passed','checks':123,'cases':[{'name':name,'status':'passed'}],
                'ableton_verified':False,'pitch_detection_implemented':False,'audio_input_channels':0,'audio_output_channels':2,
                'audio_output_silent':True,'host_device_role':'instrument',
                'scale_count':38,'chord_type_count':52,'verified_features':sorted(evidence.FEATURES|{'chord_filter'}),
                'events_file':event_name,'events_rows':2}
        events='case,rate,block,sample,type,channel,note,value\n'+name+',48000,64,3,N,1,60,100\n'+name+',48000,64,31,F,1,60,0\n'
        (build/report_name).write_text(json.dumps(report));(build/event_name).write_text(events)
    manifest['midi_only_evidence']=evidence.collect(root,build,dist,manifest)
    result=SimpleNamespace(root=root,dist=dist,bundle=bundle,build=build,manifest=manifest,output=base/'MIDI.zip')
    save(result);return result


def save(f):
    (f.dist/'build-manifest.json').write_text(json.dumps(f.manifest))


def rebind_proof(f,proof):
    path=f.dist/evidence.DIRECTORY/evidence.MANIFEST;path.write_text(json.dumps(proof))
    f.manifest['midi_only_evidence']['sha256']=hashlib.sha256(path.read_bytes()).hexdigest();save(f)


def installation_proof(f,base,commit):
    """Construct protocol bytes only; native installer/host must run in CI."""
    ps=base/'installation-powershell.txt';ps.write_text('Synthetic protocol fixture, not Windows execution.')
    host=base/'installed-vst3-host.txt';host.write_text('RESULT actual VST3 host tests passed (123 assertions).\nPLATFORM VERIFIED: Windows x64 compiled VST3 loaded and executed by native test host.\n')
    launcher=base/'installation-launcher.txt';marker='Installed Flysnapper 0.4.4:';launcher.write_text(marker+' synthetic fixture\n')
    h=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
    proof={'schema':1,'status':'passed','execution_platform':'Windows','source_commit':commit,
           'artifact_sha256':h(f.output),'installer_exit_code':0,'installed_host_exit_code':0,'host_assertions':123,
           'installed_files_sha256':f.manifest['bundle_sha256'],'recovery_files_sha256':f.manifest['bundle_sha256'],
           'unicode_paths_verified':True,'unrelated_plugin_preserved':True,'legacy_instrument_preserved':True,
           'bundle_name':evidence.BUNDLE,'powershell_log_sha256':h(ps),'host_log_sha256':h(host),
           'bat_launcher_verified':True,'bat_launcher_exit_code':0,'bat_launcher_marker':marker,
           'bat_launcher_sha256':h(f.root/'INSTALL_FLYSNAPPER.bat'),
           'launcher_recovery_files_sha256':f.manifest['bundle_sha256'],'launcher_log_sha256':h(launcher)}
    p=base/'installation-validation.json';p.write_text(json.dumps(proof));return p
