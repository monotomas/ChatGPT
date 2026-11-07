#!/usr/bin/env python3
"""Install the real Windows ZIP privately, then load the installed VST3 in its host test.
SPDX-License-Identifier: AGPL-3.0-or-later
No system installation, administrator request or interactive dialog is involved.
"""
from __future__ import annotations
import argparse,datetime as dt,hashlib,json,os,platform,re,shutil,subprocess,sys,tempfile
from pathlib import Path
from zipfile import ZipFile
from package_windows import BINARY,files_below,verify_package,verify_pe,bundle_identity

PS_DRIVER=r'''
$ErrorActionPreference = 'Stop'
[Console]::OutputEncoding = New-Object Text.UTF8Encoding($false)
. (Join-Path $env:FLYSNAPPER_VERIFY_PACKAGE 'Install-Flysnapper.ps1') -LibraryOnly
$package = $env:FLYSNAPPER_VERIFY_PACKAGE
$destination = $env:FLYSNAPPER_VERIFY_DEST
$backup = $env:FLYSNAPPER_VERIFY_BACKUP
$meta = Get-FlysnapperVerifiedPackage $package
$legacy = ''
if ($meta.BundleName -eq 'Flysnapper MIDI.vst3') {
    $legacy = Join-Path $destination 'Flysnapper.vst3/legacy-instrument-control.txt'
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($legacy)) | Out-Null
    [IO.File]::WriteAllText($legacy, 'legacy audio instrument must remain untouched')
}
$first = Install-FlysnapperPackage $package $destination $backup
$other = Join-Path $destination 'Buffkiller.vst3/installation-control.txt'
[IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($other)) | Out-Null
[IO.File]::WriteAllText($other, 'unrelated installation control')
$second = Install-FlysnapperPackage $package $destination $backup
if ([string]::IsNullOrEmpty($second.Backup)) { throw 'Actual replacement did not produce a recovery copy.' }
if ([IO.File]::ReadAllText($other) -ne 'unrelated installation control') { throw 'Replacement touched another plugin.' }
if ($legacy -ne '' -and [IO.File]::ReadAllText($legacy) -ne 'legacy audio instrument must remain untouched') { throw 'MIDI installation touched the legacy instrument.' }
$result = @{
    installed_bundle = $second.Installed
    recovery_bundle = $second.Backup
    version = $second.Version
    installed_files_sha256 = Get-FlysnapperInventory $second.Installed
    recovery_files_sha256 = Get-FlysnapperInventory $second.Backup
    unrelated_plugin_preserved = $true
    legacy_instrument_preserved = ($legacy -ne '')
    bundle_name = $meta.BundleName
}
$json = $result | ConvertTo-Json -Depth 10
[IO.File]::WriteAllText($env:FLYSNAPPER_VERIFY_RESULT, $json, (New-Object Text.UTF8Encoding($false)))
Write-Host 'Actual Windows package installed and replaced in private Unicode test directories.'
'''


def hashes(root: Path):return {name:hashlib.sha256(data).hexdigest() for name,data in files_below(root).items()}
def sha(path: Path):return hashlib.sha256(path.read_bytes()).hexdigest()

def find_host(source_root: Path) -> Path:
    candidates=sorted((source_root/'build').glob('w-*/FlysnapperVst3HostTests_artefacts/Release/FlysnapperVst3HostTests.exe'))
    if len(candidates)!=1:
        raise ValueError('Expected exactly one native Windows VST3 host executable from the current build')
    return candidates[0]

def passed_host_output(text: str,exit_code: int) -> int:
    match=re.search(r'RESULT actual VST3 host tests passed \((\d+) assertions\)\.',text)
    if exit_code!=0 or match is None or int(match.group(1))<=0 or 'PLATFORM VERIFIED: Windows x64 compiled VST3 loaded and executed by native test host.' not in text:
        raise ValueError('Installed Windows VST3 host validation did not complete successfully')
    return int(match.group(1))

def launcher_command(command_processor: Path,launcher: Path,destination: Path,backup: Path) -> str:
    # cmd.exe /s /c consumes the outer pair of quotes; each path retains its
    # own quotes. Use a raw CreateProcess command line, not CRT argv escaping,
    # so spaces, Unicode, parentheses and ampersands remain literal paths.
    paths=[str(path) for path in (command_processor,launcher,destination,backup)]
    if any(any(char in value for char in '"\r\n\0') for value in paths):raise ValueError('Unsafe launcher argument')
    return f'"{paths[0]}" /d /s /c ""{paths[1]}" -NoUI -Destination "{paths[2]}" -BackupDirectory "{paths[3]}""'

def verify(artifact: Path,source_root: Path,output: Path,source_commit: str,host: Path | None = None,temp_base: Path | None = None):
    if os.name!='nt' or sys.maxsize<=2**32:
        raise ValueError('The real-package validation must execute on native 64-bit Windows')
    if not re.fullmatch(r'[0-9a-f]{40}',source_commit):raise ValueError('Expected the exact CI source commit')
    artifact=artifact.resolve(strict=True);source_root=source_root.resolve(strict=True)
    host=(host or find_host(source_root)).resolve(strict=True)
    verify_pe(host.read_bytes());verify_package(artifact)
    initial_artifact_sha=sha(artifact)
    output=output.absolute();output.parent.mkdir(parents=True,exist_ok=True)
    if output.exists():raise ValueError('Installation proof already exists; refusing stale evidence')
    ps_log=output.parent/'installation-powershell.txt';host_log=output.parent/'installed-vst3-host.txt';launcher_log=output.parent/'installation-launcher.txt'
    proof={'schema':1,'status':'failed','execution_platform':platform.system(),'execution_architecture':platform.machine(),
           'source_commit':source_commit,'artifact_sha256':initial_artifact_sha,'artifact_bytes':artifact.stat().st_size,
           'recorded_at_utc':dt.datetime.now(dt.timezone.utc).isoformat(),'installer_exit_code':None,'installed_host_exit_code':None,
           'host_assertions':0,'host_binary_sha256':sha(host),'ableton_verified':False,'bat_launcher_verified':False,'bat_launcher_exit_code':None,
           'scope':'Actual packaged Windows installer and installed compiled VST3; no system install or DAW/driver validation'}
    try:
        shell=shutil.which('powershell.exe')
        if not shell:raise ValueError('Builtin Windows PowerShell is required')
        base=temp_base or Path(os.environ.get('RUNNER_TEMP') or tempfile.gettempdir())
        with tempfile.TemporaryDirectory(prefix='flysnapper-install-',dir=base) as temporary:
            private=Path(temporary);unicode=private/'Olav Lægreid (test) & routing'
            extracted=unicode/'Utpakket';extracted.mkdir(parents=True)
            with ZipFile(artifact) as archive:
                archive.extractall(extracted)
                package=extracted/archive.namelist()[0].split('/',1)[0]
            release=json.loads((package/'release-manifest.json').read_text(encoding='utf-8'))
            build=json.loads((package/'Validation/build-manifest.json').read_text(encoding='utf-8'))
            if release.get('source_commit')!=source_commit or build.get('source_commit')!=source_commit:
                raise ValueError('Packaged source proof differs from the requested CI revision')
            bundle_name,binary_name=bundle_identity(build)
            prefix=bundle_name+'/'
            expected={name[len(prefix):]:digest for name,digest in release['files'].items() if name.startswith(prefix)}
            proof['bundle_name']=bundle_name
            if expected!=build.get('bundle_sha256'):raise ValueError('Build and package bundle inventories disagree')
            target=unicode/'Separate VST3';backup=unicode/'Recovery outside scan'
            result_path=private/'installation-result.json';driver=private/'install-verification.ps1';driver.write_text(PS_DRIVER,encoding='utf-8-sig')
            env=os.environ.copy();env.update(FLYSNAPPER_VERIFY_PACKAGE=str(package),FLYSNAPPER_VERIFY_DEST=str(target),
                FLYSNAPPER_VERIFY_BACKUP=str(backup),FLYSNAPPER_VERIFY_RESULT=str(result_path),PYTHONUTF8='1',PYTHONIOENCODING='utf-8')
            with ps_log.open('wb') as log:
                result=subprocess.run([shell,'-NoLogo','-NoProfile','-ExecutionPolicy','Bypass','-STA','-File',str(driver)],
                                      cwd=private,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=180)
            proof['installer_exit_code']=result.returncode
            if result.returncode!=0 or not result_path.is_file():raise ValueError('Actual packaged PowerShell installer failed; see installation-powershell.txt')
            install=json.loads(result_path.read_text(encoding='utf-8-sig'))
            installed=Path(install['installed_bundle']);recovery=Path(install['recovery_bundle'])
            if installed.resolve()!= (target/bundle_name).resolve():raise ValueError('Installer did not use the private VST3 destination')
            if not recovery.resolve().is_relative_to(backup.resolve()) or recovery.resolve().is_relative_to(target.resolve()):
                raise ValueError('Installer recovery copy is not outside the scan directory')
            actual=hashes(installed);saved=hashes(recovery)
            if actual!=expected or saved!=expected or install.get('installed_files_sha256')!=expected or install.get('recovery_files_sha256')!=expected:
                raise ValueError('Installed or recovery Windows bundle differs from the verified package')
            if install.get('unrelated_plugin_preserved') is not True:raise ValueError('Unrelated plugin preservation was not verified')
            if build.get('version') in ('0.4.0','0.4.1','0.4.2'):
                legacy=target/'Flysnapper.vst3/legacy-instrument-control.txt'
                if install.get('bundle_name')!=bundle_name or install.get('legacy_instrument_preserved') is not True or legacy.read_text()!='legacy audio instrument must remain untouched':
                    raise ValueError('MIDI-only installation did not preserve the old instrument')
                proof['legacy_instrument_preserved']=True
            proof.update(installed_files_sha256=actual,recovery_files_sha256=saved,unrelated_plugin_preserved=True,
                extraction_path=str(package),installed_bundle_path=str(installed),recovery_bundle_path=str(recovery),
                unicode_paths_verified='æ' in str(package) and 'æ' in str(installed))
            if build.get('version')!='0.2.0':
                launcher=package/'INSTALL_FLYSNAPPER.bat'
                command_processor=Path(os.environ.get('SystemRoot','C:/Windows'))/'System32/cmd.exe'
                if not command_processor.is_file():raise ValueError('Builtin Windows command processor is missing')
                before={path.resolve() for path in backup.iterdir()} if backup.exists() else set()
                command_line=launcher_command(command_processor,launcher,target,backup);proof['bat_launcher_command']=command_line;proof['bat_launcher_sha256']=sha(launcher)
                with launcher_log.open('wb') as log:
                    result=subprocess.run(command_line,executable=str(command_processor),cwd=package,env=env,stdin=subprocess.DEVNULL,stdout=log,stderr=subprocess.STDOUT,timeout=180)
                proof['bat_launcher_exit_code']=result.returncode
                marker=('Installed Flysnapper '+str(build['version'])+':').encode('ascii')
                if result.returncode!=0 or marker not in launcher_log.read_bytes():raise ValueError('Actual packaged BAT launcher failed or did not report installation; see installation-launcher.txt')
                created=[path for path in backup.iterdir() if path.resolve() not in before]
                if len(created)!=1 or not created[0].is_dir():raise ValueError('BAT replacement did not create exactly one private recovery copy')
                launcher_recovery=created[0]/bundle_name;launcher_saved=hashes(launcher_recovery)
                if hashes(installed)!=expected or launcher_saved!=expected:raise ValueError('BAT-installed/recovery payload differs from the package')
                if (target/'Buffkiller.vst3/installation-control.txt').read_text()!='unrelated installation control':raise ValueError('BAT replacement changed another plugin')
                if build.get('version') in ('0.4.0','0.4.1','0.4.2') and (target/'Flysnapper.vst3/legacy-instrument-control.txt').read_text()!='legacy audio instrument must remain untouched':
                    raise ValueError('BAT MIDI installation touched the legacy instrument')
                proof.update(bat_launcher_verified=True,bat_launcher_marker=marker.decode('ascii'),launcher_recovery_files_sha256=launcher_saved,launcher_recovery_bundle_path=str(launcher_recovery))
            command=[str(host),str(installed)];proof['installed_host_command']=command
            with host_log.open('wb') as log:
                result=subprocess.run(command,cwd=private,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=300)
            proof['installed_host_exit_code']=result.returncode
            proof['host_assertions']=passed_host_output(host_log.read_text(encoding='utf-8',errors='replace'),result.returncode)
            if hashes(installed)!=expected:raise ValueError('Installed VST3 was modified during host validation')
            if sha(artifact)!=initial_artifact_sha:raise ValueError('Installation ZIP changed during verification')
            proof['status']='passed'
    except (ValueError,OSError,subprocess.SubprocessError) as error:
        proof['status']='failed';proof['error']=str(error)
        raise
    finally:
        if ps_log.is_file():proof['powershell_log_sha256']=sha(ps_log)
        if host_log.is_file():proof['host_log_sha256']=sha(host_log)
        if launcher_log.is_file():proof['launcher_log_sha256']=sha(launcher_log)
        output.write_text(json.dumps(proof,indent=2,ensure_ascii=False)+'\n',encoding='utf-8')
    return proof


def main(argv=None):
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--artifact',type=Path,required=True);p.add_argument('--source-root',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True);p.add_argument('--source-commit',required=True);p.add_argument('--host',type=Path);p.add_argument('--temp-base',type=Path)
    a=p.parse_args(argv)
    if hasattr(sys.stdout,'reconfigure'):sys.stdout.reconfigure(encoding='utf-8',errors='backslashreplace')
    try:proof=verify(a.artifact,a.source_root,a.output,a.source_commit,a.host,a.temp_base)
    except (ValueError,OSError,subprocess.SubprocessError) as error:p.exit(1,'Real Windows installation validation failed: '+str(error)+'\n')
    print('Actual Windows installation and installed-VST3 validation passed:',proof['host_assertions'],'assertions')
    print('Proof:',a.output);return 0
if __name__=='__main__':raise SystemExit(main())
