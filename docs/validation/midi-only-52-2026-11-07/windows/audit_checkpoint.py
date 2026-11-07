"""Offline byte audit of a retained Windows research checkpoint; no DAW claim."""
from pathlib import Path
import hashlib,io,json,re,subprocess,sys,zipfile

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT))
from Tools import midi_only_evidence as e, package_windows as package, package_source as source
from Tools.publish_windows_delivery import installed_package_proof

EXPECTED='e891c2eef2f85e87138d22bbec8165e2b44f4c25'
OUT=Path(__file__).resolve().parent/'completed-checkpoint'
def digest(data):return hashlib.sha256(data).hexdigest()
def require(condition,message):
    if not condition:raise ValueError(message)
def blob(commit,name):
    e.relative(name)
    return subprocess.check_output(['git','-C',str(ROOT),'show',commit+':'+name])
def retain(name,data):
    e.relative(name);p=OUT/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data);return p
def run(checkpoint):
    require(re.fullmatch('[0-9a-f]{40}',checkpoint),'Expected immutable checkpoint SHA')
    OUT.mkdir(exist_ok=True)
    status_data=blob(checkpoint,'status.json');status=json.loads(status_data);retain('status.json',status_data)
    require(status['source_commit']==EXPECTED and status['status']=='completed','Exact52-source checkpoint is not complete')
    require(status.get('publication_kind')=='research-checkpoint' and status.get('ableton_verified') is False,'Wrong release/DAW scope')
    artifact=status['artifact'];require(artifact['path']=='artifacts/Flysnapper-MIDI-Windows-x64.zip','Unexpected artifact path')
    zip_data=blob(checkpoint,artifact['path']);require(digest(zip_data)==artifact['sha256'] and len(zip_data)==artifact['bytes'],'Published ZIP hash/size differs')
    zip_path=retain(artifact['path'],zip_data);package.verify_package(zip_path)
    raw={};source_data=None
    with zipfile.ZipFile(io.BytesIO(zip_data)) as z:
        prefix=z.namelist()[0].split('/',1)[0]+'/'
        release=json.loads(z.read(prefix+'release-manifest.json'))
        require(release['source_commit']==EXPECTED,'Release source context differs')
        for name in z.namelist():
            relative=name[len(prefix):]
            if relative.startswith('Validation/'):
                key=relative[len('Validation/'):];data=z.read(name);raw[key]=data;retain('package-validation/'+key,data)
                require(blob(checkpoint,'validation/package/'+key)==data,'Git/package Validation member differs: '+key)
        build=json.loads(raw['build-manifest.json']);proof=json.loads(raw['midi-validation/report-manifest.json'])
        require(build['source_commit']==EXPECTED and proof['source_commit']==EXPECTED,'Build/native source context differs')
        require(build['platform']=='Windows-x64' and build['tests']=='passed' and build['windows_verified'] is True,'Not a passed Windows build')
        require(build['evaluation_scope']==e.SCOPE and proof['scope']==e.SCOPE and e.SCOPE['chord_types']==52,'Not31/52 MIDI-only scope')
        require(build.get('audio_processing_implemented') is False and build.get('pitch_detection_implemented') is False and build.get('ableton_verified') is False,'Unexpected audio or DAW claim')
        require(proof['kind']=='MidiOnlyNativeExecution' and proof['status']=='passed' and proof['platform']=='Windows-x64','Invalid native proof kind/platform')
        require(build['midi_only_evidence']=={'path':'midi-validation/report-manifest.json','sha256':digest(raw['midi-validation/report-manifest.json'])},'Native proof manifest binding differs')
        e.check_ctest(raw['test-results.xml']);require(digest(raw['test-results.xml'])==proof['ctest_sha256'],'CTest binding differs')
        external_build=blob(checkpoint,'validation/windows-build-manifest.json')
        retain('validation/windows-build-manifest.json',external_build)
        require(json.loads(external_build)==build and external_build.replace(b'\r\n',b'\n')==raw['build-manifest.json'],
                'External build manifest differs beyond Windows newline serialization')
        require(blob(checkpoint,'validation/windows-test-results.xml')==raw['test-results.xml'],'External CTest differs')
        bundle=e.BUNDLE+'/'
        actual_bundle={n[len(prefix+bundle):]:digest(z.read(n)) for n in z.namelist() if n.startswith(prefix+bundle)}
        require(actual_bundle==build['bundle_sha256'],'Compiled bundle inventory differs')
        dll=z.read(prefix+bundle+e.WINDOWS_BINARY);package.verify_pe(dll)
        require(digest(dll)==build['vst3_sha256']==proof['vst3_sha256'],'Compiled DLL binding differs')
        info=z.read(prefix+bundle+'Contents/Resources/moduleinfo.json')
        require(digest(info)==proof['moduleinfo_sha256'] and e.module_info(info)==proof['module'],'Module identity/category binding differs')
        source_data=z.read(prefix+'Source/Flysnapper-Source.zip');source_path=retain('Source/Flysnapper-Source.zip',source_data);source.verify_package(source_path)
        source_prefix=source.PACKAGE_ROOT+'/'
        require(len(proof['source_sha256'])==21,'Unexpected selected project-input inventory')
        with zipfile.ZipFile(io.BytesIO(source_data)) as sz:
            for name,expected_hash in proof['source_sha256'].items():
                expected=blob(EXPECTED,name)
                require(digest(expected)==expected_hash and sz.read(source_prefix+name)==expected,'Native21/sourceZIP/committed input differs: '+name)
        launcher_hash=digest(z.read(prefix+'INSTALL_FLYSNAPPER.bat'))
    reports={}
    require(set(proof['executions'])=={spec[0] for spec in e.SPECS},'Wrong native executable inventory')
    for test,target,report_name,event_name,kind in e.SPECS:
        report_bytes=raw['midi-validation/'+report_name];events_bytes=raw['midi-validation/'+event_name]
        report=e.check_report(report_bytes,events_bytes,kind,event_name);execution=proof['executions'][test]
        require(execution['target']==target and execution['report_sha256']==digest(report_bytes) and execution['events_sha256']==digest(events_bytes),'Raw report/event binding differs')
        require(execution['checks']==report['checks'] and execution['events_rows']==report['events_rows'],'Execution counts differ')
        require(re.fullmatch('[0-9a-f]{64}',execution['binary_sha256']) and execution['binary_bytes']>64,'Missing executable hash/length record')
        reports[test]={'checks':report['checks'],'cases':len(report['cases']),'events_rows':report['events_rows']}
    installation_path=status['installation_validation']['path'];installation_data=blob(checkpoint,installation_path)
    require(digest(installation_data)==status['installation_validation']['sha256'],'Installation proof status binding differs')
    ip=retain(installation_path,installation_data);installation=json.loads(installation_data)
    for name in ('installation-powershell.txt','installed-vst3-host.txt','installation-launcher.txt'):
        retain('validation/'+name,blob(checkpoint,'validation/'+name))
    installed_package_proof(ip,EXPECTED,artifact['sha256'],build,launcher_hash)
    require(installation.get('ableton_verified') is False and installation['artifact_bytes']==len(zip_data),'Installation scope/ZIP size differs')
    require(installation['host_binary_sha256']==proof['executions']['FlysnapperActualVst3']['binary_sha256'],'Installed-host executable hash differs from CTest host')
    omitted_data=blob(checkpoint,status['omitted_executables']['path']);retain(status['omitted_executables']['path'],omitted_data)
    require(digest(omitted_data)==status['omitted_executables']['sha256'],'Omission inventory status binding differs')
    omitted=json.loads(omitted_data);require(omitted['source_commit']==EXPECTED,'Omitted executable source differs')
    require({x['role'] for x in omitted['executables']}==set(proof['executions']),'Omitted executable role inventory differs')
    for item in omitted['executables']:
        execution=proof['executions'][item['role']]
        require(all(item[k]==execution[v] for k,v in [('compiled_path','binary'),('sha256','binary_sha256'),('bytes','binary_bytes'),('report_sha256','report_sha256'),('events_sha256','events_sha256')]),'Omitted executable metadata differs')
    log_data=blob(checkpoint,status['log']);retain(status['log'],log_data)
    result={'schema':1,'kind':'WindowsResearchCheckpointOfflineByteAudit','status':'passed','source_commit':EXPECTED,'checkpoint_commit':checkpoint,'run_id':status['run_id'],'artifact':artifact,'vst3_sha256':build['vst3_sha256'],'selected_project_inputs_verified':21,'reports':reports,'host_assertions':installation['host_assertions'],'installer_exit_code':installation['installer_exit_code'],'installed_host_exit_code':installation['installed_host_exit_code'],'bat_launcher_exit_code':installation['bat_launcher_exit_code'],'unicode_paths_verified':installation['unicode_paths_verified'],'legacy_instrument_preserved':installation['legacy_instrument_preserved'],'unrelated_plugin_preserved':installation['unrelated_plugin_preserved'],'build_manifest_serialization':'External Windows manifest has CRLF; packaged manifest has LF. Parsed objects are equal and replacing CRLF with LF makes the entire bytes equal. Each original byte hash is retained separately.','verified_features':['ZIP inventory and every member checksum','embedded complete source ZIP inventory and every source checksum','selected21 inputs exactly match committed Git blobs and native proof','native raw reports/events and CTest bindings','Windows x64 DLL and moduleinfo identity/category bytes','PS/BAT install,replace,recovery and installed DLL inventories/log bindings','installed actual-host completion and executable hash record','explicit omitted native-executable inventory'],'native_executables_rehashed_offline':False,'limitation':'Two native test executables were deliberately omitted from research Git. Their recorded hashes/lengths/report bindings match; this offline audit cannot rehash or rerun omitted executables. CI rehashed both before packaging/publication. Native Windows/compiled-host proof does not verify Ableton Live routing or MIDI-effect placement.','ableton_verified':False,'files_sha256':{p.relative_to(OUT).as_posix():digest(p.read_bytes()) for p in OUT.rglob('*') if p.is_file() and p.name!='offline-byte-audit.json'}}
    retain('offline-byte-audit.json',(json.dumps(result,indent=2)+'\n').encode());print(json.dumps(result,indent=2))
if __name__=='__main__':run(sys.argv[1])
