#!/usr/bin/env python3
"""Build/test Flysnapper MIDI 0.4.4 instrument adapter. Standard library only.

SPDX-License-Identifier: AGPL-3.0-or-later
Windows tool discovery and Unicode patch adapted from Buffkiller 1.0.0.
Copyright notices and AGPL terms are retained in LICENSE.txt and THIRD_PARTY.md.

Windows: python build_plugin.py (build/test only); add --install to install.
Linux: python build_plugin.py --native (Linux validation, not a Windows build).
"""
from __future__ import annotations
import argparse
import base64
from contextlib import contextmanager
import ctypes
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import traceback
import urllib.parse
import urllib.request
import xml.etree.ElementTree as ET
from typing import Mapping, Sequence, NamedTuple
from Tools import input_dynamics_evidence  # Historical protocol fixtures only.
from Tools import midi_only_evidence

BUILDER_REVISION = "2026.11.07-midi-instrument-adapter"
ROOT = Path(__file__).resolve().parent
JUCE_TAG = "8.0.16-security"
JUCE_COMMIT = "ea77444750de45e036e0279bb0291144bf8e3688"
JUCE_URL = "https://github.com/juce-framework/JUCE.git"
LOG_DIR = ROOT / "logs"
DIST = ROOT / "dist"
_log = None

def console_print(text: str) -> None:
    # Redirected Windows Python 3.10-3.14 may use CP1252. Diagnostic text
    # must never abort a valid build or alter the captured command response.
    try:
        print(text, flush=True)
    except UnicodeEncodeError:
        encoding = getattr(sys.stdout, "encoding", None) or "ascii"
        escaped = text.encode(encoding, errors="backslashreplace").decode(encoding)
        print(escaped, flush=True)

def say(text: str) -> None:
    console_print(text)
    if _log is not None:
        _log.write(text + "\n"); _log.flush()

class CommandError(RuntimeError):
    """Keep captured output so only toolchain-discovery failures trigger fallback."""

    def __init__(self, command: Sequence[str], returncode: int, output: str):
        self.command = list(command)
        self.returncode = returncode
        self.output = output
        super().__init__(
            f"Kommandoen feilet (kode {returncode}): {command[0]}. "
            "Se loggen; ingen plugin ble installert."
        )


def run(args: Sequence[str | Path], cwd: Path | None = None, capture: bool = False,
        env: Mapping[str, str] | None = None) -> str:
    cmd = [str(a) for a in args]
    say("\n> " + subprocess.list2cmdline(cmd))
    child_environment = dict(os.environ if env is None else env)
    # Python subprocesses must emit the UTF-8 stream that this reader expects.
    # Preserve every other SDK/proxy/authentication binding and the caller dict.
    child_environment.update(PYTHONIOENCODING="utf-8", PYTHONUTF8="1")
    process = subprocess.Popen(
        cmd, cwd=str(cwd or ROOT), stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, encoding="utf-8", errors="replace",
        env=child_environment,
    )
    lines: list[str] = []
    assert process.stdout is not None
    try:
        for line in process.stdout:
            line = line.rstrip("\r\n")
            lines.append(line)
            if not capture:
                say(line)
            elif _log is not None:
                _log.write(line + "\n")
    finally:
        process.stdout.close()
    result = process.wait()
    output = "\n".join(lines)
    if _log is not None:
        _log.flush()
    if result != 0:
        if capture:
            console_print(output)
        raise CommandError(cmd, result, output)
    return output


def version_parts(value: object) -> tuple[int, ...]:
    text = str(value).strip()
    if not re.fullmatch(r"[0-9]+(?:\.[0-9]+){1,3}", text):
        return ()
    return tuple(int(part) for part in text.split("."))


def find_visual_studio_details() -> tuple[Path, int, str | None]:
    """Return path, major, and the *full* installationVersion from vswhere.

    A product display version (e.g. 18.11.3) is NOT the four-part build number
    that CMake's version= field requires. Do not fabricate a fourth component.
    No Visual Studio, SDK, registry or system environment files are changed.
    """
    bases = [Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")),
             Path(os.environ.get("ProgramFiles", r"C:\Program Files"))]
    installations: list[dict] = []
    for base in bases:
        vswhere = base / "Microsoft Visual Studio/Installer/vswhere.exe"
        if vswhere.is_file():
            text = run([vswhere, "-all", "-prerelease", "-products", "*",
                        "-format", "json", "-utf8"], capture=True)
            installations = json.loads(text.lstrip("\ufeff"))
            if not isinstance(installations, list):
                raise RuntimeError("Uventet JSON-svar fra vswhere. Ingen installasjoner endret.")
            break
    # Include Build Tools, prerelease, and incomplete registration, but require
    # the compiler on disk. Never rely on the component-ID filter that failed
    # on the user's machine.
    records = (item for item in installations if isinstance(item, dict))
    for item in sorted(records,
                       key=lambda x: version_parts(x.get("installationVersion", "")),
                       reverse=True):
        parts = version_parts(item.get("installationVersion", ""))
        location = item.get("installationPath")
        if not parts or parts[0] < 16 or not isinstance(location, str) or not location:
            continue
        path = Path(location)
        if list(path.glob("VC/Tools/MSVC/*/bin/Hostx64/x64/cl.exe")):
            full_version = str(item["installationVersion"]).strip() if len(parts) == 4 else None
            return path, parts[0], full_version
    # Compatibility fallback: the files may be usable even if vswhere returns
    # no instance. Direct command-line MSVC setup does not need a made-up version.
    for base in bases:
        for directory, major in (("18", 18), ("2022", 17), ("2019", 16)):
            path = base / "Microsoft Visual Studio" / directory / "BuildTools"
            if list(path.glob("VC/Tools/MSVC/*/bin/Hostx64/x64/cl.exe")):
                return path, major, None
    raise RuntimeError(
        "Fant ikke MSVC x64 pa disken. Skriptet endrer eller reparerer ingen installasjoner."
    )


def find_visual_studio() -> tuple[Path, int]:
    """Keep compatibility with the four tests in the original 0.1.0 package."""
    path, major, _ = find_visual_studio_details()
    return path, major


def generator_instance(path: Path, major: int, full_version: str | None) -> str:
    location = str(path)
    # CMake uses a comma as a delimiter, with no documented escaping. Such
    # installations can use the direct compiler route instead.
    if "," in location:
        raise ValueError("Visual Studio-stien inneholder komma; bruker direkte MSVC-bygging.")
    if full_version is None:
        return location
    parts = version_parts(full_version)
    if len(parts) != 4 or parts[0] != major:
        raise ValueError("CMake krever det faktiske firedelte installationVersion fra vswhere.")
    return f"{location},version={full_version}"


def is_instance_discovery_error(output: str) -> bool:
    text = output.lower()
    return any(message in text for message in (
        "could not find specified instance of visual studio",
        "could not find any instance of visual studio",
        "instance is not known to the visual studio installer",
    ))


def build_directory(major: int, route: str, identity: str) -> Path:
    """Use a new cache, never rewrite or delete the failed vs18-x64 directory."""
    token = hashlib.sha256(identity.encode("utf-8")).hexdigest()[:12]
    return ROOT / "build" / f"vs{major}-x64-{route}-fix1-{token}"


def parse_developer_environment(output: bytes) -> dict[str, str]:
    # cmd /u writes SET's output as UTF-16LE, preserving names such as Laegreid
    # with the actual Norwegian character. Locate the marker before decoding,
    # as a developer script can run tools which print a different encoding.
    marker = "__FLYSNAPPER_ENV_BEGIN__".encode("utf-16-le")
    start = output.find(marker)
    if start < 0:
        raise RuntimeError("Utviklermiljoet ble ikke returnert av Microsofts kommandofil.")
    text = output[start + len(marker):].decode("utf-16-le", errors="strict")
    values: dict[str, str] = {}
    for line in text.splitlines():
        name, sep, value = line.partition("=")
        if sep and name:  # skip cmd's hidden =C:=... pseudo variables
            values[name.upper()] = value
    values.pop("FLYSNAPPER_ENV_SCRIPT", None)
    if not all(values.get(key) for key in ("PATH", "INCLUDE", "LIB")):
        raise RuntimeError("Microsofts byggemiljo mangler PATH, INCLUDE eller LIB.")
    arch = values.get("VSCMD_ARG_TGT_ARCH", "").lower()
    if arch and arch not in ("x64", "amd64"):
        raise RuntimeError("Microsofts byggemiljo valgte ikke x64.")
    return values


def developer_environment(installation: Path) -> dict[str, str]:
    """Run Microsoft's installed setup script in a child process only.

    No manual PATH editing, PowerShell policy changes, or new tools are needed.
    Environment variables are passed only to our build/test subprocesses and
    their contents are not logged (they can contain private tokens).
    """
    candidates = [
        (installation / "VC/Auxiliary/Build/vcvarsall.bat", "x64"),
        (installation / "Common7/Tools/VsDevCmd.bat", "-no_logo -arch=x64 -host_arch=x64"),
    ]
    comspec = os.environ.get("COMSPEC", str(Path(os.environ.get("SystemRoot", r"C:\Windows")) / "System32/cmd.exe"))
    failures: list[str] = []
    for script, arguments in candidates:
        if not script.is_file():
            continue
        say("Klargjor Microsofts x64-kommandolinjemiljo: " + str(script))
        base_environment = dict(os.environ)
        base_environment["FLYSNAPPER_ENV_SCRIPT"] = str(script)
        with tempfile.TemporaryDirectory(prefix="flysnapper-env-") as tmp:
            # A simple filename and temporary cwd avoid cmd /c path-quoting
            # ambiguities. The installed path is passed in the environment.
            batch = Path(tmp) / "capture.cmd"
            batch.write_bytes((
                "@echo off\r\n"
                "setlocal DisableDelayedExpansion\r\n"
                f'call "%FLYSNAPPER_ENV_SCRIPT%" {arguments}\r\n'
                "if errorlevel 1 exit /b 1\r\n"
                "echo __FLYSNAPPER_ENV_BEGIN__\r\n"
                "set\r\n"
            ).encode("ascii"))
            result = subprocess.run([comspec, "/d", "/u", "/v:off", "/c", "capture.cmd"],
                                    cwd=tmp, env=base_environment,
                                    stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                                    timeout=120, check=False)
        if result.returncode:
            # Developer setup output, unlike the SET dump, is useful diagnosis.
            raw = result.stdout.split("__FLYSNAPPER_ENV_BEGIN__".encode("utf-16-le"), 1)[0]
            encoding = "utf-16-le" if b"\0" in raw[:100] else "utf-8"
            message = raw.decode(encoding, errors="replace").strip()
            failures.append(f"{script.name}: kode {result.returncode}\n{message}")
            continue
        try:
            env = parse_developer_environment(result.stdout)
            compiler = find_environment_tool(env, "cl.exe")
            if compiler is None or not compiler.is_relative_to(installation.resolve()):
                raise RuntimeError("Utviklermiljoet peker ikke pa kompilatoren i valgt Build Tools.")
            if find_environment_tool(env, "rc.exe") is None:
                raise RuntimeError("Windows SDK-verktøyet rc.exe er ikke tilgjengelig i byggemiljoet.")
            say("Direkte x64-kompilator: " + str(compiler))
            return env
        except RuntimeError as error:
            failures.append(f"{script.name}: {error}")
    details = "\n".join(failures) if failures else "Fant verken vcvarsall.bat eller VsDevCmd.bat i installasjonen."
    raise RuntimeError("Kunne ikke klargjore Microsofts x64-byggemiljo.\n" + details)


def find_environment_tool(env: Mapping[str, str], name: str) -> Path | None:
    # Explicit PATH, never implicitly use the parent's Linux/WSL compiler.
    value = shutil.which(name, path=env.get("PATH", ""))
    return Path(value).resolve() if value else None


def choose_direct_generator(installation: Path, env: Mapping[str, str],
                            generators: Sequence[str]) -> tuple[str, Path]:
    ninja = installation / "Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe"
    if not ninja.is_file():
        ninja = find_environment_tool(env, "ninja.exe")
    if "Ninja" in generators and ninja is not None and ninja.is_file():
        return "Ninja", ninja
    nmake = find_environment_tool(env, "nmake.exe")
    if "NMake Makefiles" in generators and nmake is not None:
        return "NMake Makefiles", nmake
    raise RuntimeError("Verken Ninja eller nmake.exe ble funnet i Microsofts byggemiljo.")


def build_command(cmake: str, build: Path, generator: str, jobs: int) -> list[str]:
    command = [cmake, "--build", str(build), "--config", "Release", "--target",
               "Flysnapper_VST3", "FlysnapperCoreTests", "FlysnapperPluginTests",
               "FlysnapperMidiOnlyProcessorTests", "FlysnapperVst3HostTests"]
    if generator != "NMake Makefiles":
        command.extend(["--parallel", str(max(1, jobs))])
    return command


def acquire_juce(git: str) -> Path:
    deps=ROOT/"deps";deps.mkdir(exist_ok=True);dest=deps/"JUCE"
    if not dest.exists():
        run([git,"-c","core.longpaths=true","clone","--depth","1","--branch",JUCE_TAG,JUCE_URL,dest])
    elif not (dest/".git").is_dir():
        raise RuntimeError(f"{dest} finnes, men er ikke den forventede Git-kopien. Ingen filer overskrevet.")
    run([git,"-C",dest,"config","core.longpaths","true"])
    current=run([git,"-C",dest,"rev-parse","HEAD"],capture=True).strip()
    dirty=run([git,"-C",dest,"status","--porcelain"],capture=True).strip()
    if dirty: raise RuntimeError("JUCE-kopien inneholder lokale endringer. De blir ikke overskrevet.")
    if current != JUCE_COMMIT:
        run([git,"-C",dest,"fetch","--depth","1","origin",JUCE_COMMIT])
        run([git,"-C",dest,"checkout","--detach",JUCE_COMMIT])
    actual=run([git,"-C",dest,"rev-parse","HEAD"],capture=True).strip()
    if actual != JUCE_COMMIT: raise RuntimeError("JUCE-revisjonen stemmer ikke. Avbryter byggingen.")
    say(f"JUCE {JUCE_TAG} kontrollert: {actual}")
    return dest

def bundle_hashes(folder: Path) -> dict[str, str]:
    """Hash every regular bundle file; never install symlinked external contents."""
    if folder.is_symlink():
        raise RuntimeError("VST3-pakken er en symbolsk lenke. Installerer ikke.")
    result = {}
    for path in sorted(folder.rglob("*")):
        if path.is_symlink():
            raise RuntimeError("VST3-pakken inneholder en symbolsk lenke. Installerer ikke.")
        if path.is_file():
            result[path.relative_to(folder).as_posix()] = hashlib.sha256(path.read_bytes()).hexdigest()
    return result


def install_bundle(install_dir: Path | None = None) -> None:
    manifest_path = DIST / "build-manifest.json"
    if not manifest_path.is_file():
        raise RuntimeError("Byggemanifest mangler. Kjor full bygging forst.")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    midi_only = manifest.get("version") in ("0.4.0", "0.4.1", "0.4.2", "0.4.3", "0.4.4")
    bundle_name = midi_only_evidence.BUNDLE if midi_only else "Flysnapper.vst3"
    suffix = midi_only_evidence.WINDOWS_BINARY if midi_only else "Contents/x86_64-win/Flysnapper.vst3"
    if midi_only and manifest.get("bundle_name") != bundle_name:
        raise RuntimeError("MIDI-pakkens identitet er feil.")
    source = DIST / bundle_name
    binary = source / suffix
    if not binary.is_file():
        raise RuntimeError("Ingen ferdig Windows x64 VST3-pakke i dist. Kjor full Windows-bygging forst.")
    data = binary.read_bytes()
    if len(data) < 64 or data[:2] != b"MZ":
        raise RuntimeError("Byggeresultatet er ikke en Windows-binaerfil.")
    pe = int.from_bytes(data[60:64], "little")
    if pe + 6 > len(data) or data[pe:pe+6] != b"PE\0\0\x64\x86":
        raise RuntimeError("Pluginen er ikke en gyldig x64 Windows-binaerfil.")
    if (manifest.get("tests") != "passed" or manifest.get("platform") != "Windows-x64"
            or manifest.get("vst3_sha256") != hashlib.sha256(data).hexdigest()
            or manifest.get("bundle_sha256") != bundle_hashes(source)):
        raise RuntimeError("Pluginpakken avviker fra det testede Windows-resultatet. Installerer ikke.")
    if midi_only:
        midi_only_evidence.verify(ROOT, DIST, manifest)
    common = Path(os.environ.get("CommonProgramFiles", r"C:\Program Files\Common Files"))
    destination = validate_install_directory(install_dir) if install_dir is not None else common / "VST3"
    target = destination / bundle_name
    if target.resolve() == source.resolve():
        raise RuntimeError("Velg en egen VST3-mappe, ikke prosjektets dist-mappe.")
    destination.mkdir(parents=True, exist_ok=True)
    staging = destination / (bundle_name.removesuffix(".vst3") + ".installing")
    rollback = destination / (bundle_name.removesuffix(".vst3") + ".rollback")
    if staging.exists() or rollback.exists():
        raise RuntimeError("En tidligere Flysnapper.installing/rollback finnes. Ingen eksisterende filer overskrives.")
    backup = None
    moved_old = False
    try:
        shutil.copytree(source, staging)
        if bundle_hashes(staging) != manifest["bundle_sha256"]:
            raise RuntimeError("Kopieringen bestod ikke kontrollsummene.")
        if target.exists():
            # Recovery stays outside the DAW scan folder; do not leave duplicate plugins.
            backup = DIST / "previous-installations" / dt.datetime.now().strftime("%Y%m%d-%H%M%S-%f") / bundle_name
            backup.parent.mkdir(parents=True)
            shutil.copytree(target, backup)
            if bundle_hashes(backup) != bundle_hashes(target):
                raise RuntimeError("Kunne ikke verifisere gjenopprettingskopien. Gammel plugin beholdes.")
            target.rename(rollback)
            moved_old = True
        try:
            staging.rename(target)
        except Exception:
            if moved_old and not target.exists():
                rollback.rename(target)
                moved_old = False
            raise
    finally:
        if staging.exists():
            shutil.rmtree(staging)
    if moved_old:
        shutil.rmtree(rollback)
    say("INSTALLERT: " + str(target))
    if backup is not None:
        say("Forrige versjon for gjenoppretting: " + str(backup))
    (DIST / "installation.json").write_text(json.dumps({"path": str(target), "backup": str(backup) if backup else None,
        "time": dt.datetime.now().isoformat()}, indent=2), encoding="utf-8")


def elevate_install(install_dir: Path | None = None) -> bool:
    """Elevate only the copy step, not Git/CMake/MSVC; wait for the result."""
    from ctypes import wintypes
    class SHELLEXECUTEINFOW(ctypes.Structure):
        _fields_=[("cbSize",wintypes.DWORD),("fMask",ctypes.c_ulong),("hwnd",wintypes.HWND),
                  ("lpVerb",wintypes.LPCWSTR),("lpFile",wintypes.LPCWSTR),("lpParameters",wintypes.LPCWSTR),
                  ("lpDirectory",wintypes.LPCWSTR),("nShow",ctypes.c_int),("hInstApp",wintypes.HINSTANCE),
                  ("lpIDList",ctypes.c_void_p),("lpClass",wintypes.LPCWSTR),("hkeyClass",wintypes.HKEY),
                  ("dwHotKey",wintypes.DWORD),("hIcon",wintypes.HANDLE),("hProcess",wintypes.HANDLE)]
    info=SHELLEXECUTEINFOW();info.cbSize=ctypes.sizeof(info);info.fMask=0x00000040
    info.lpVerb="runas";info.lpFile=sys.executable
    install_args = [str(ROOT/"build_plugin.py"), "--install-only"]
    if install_dir is not None:
        install_args += ["--install-dir", str(install_dir)]
    info.lpParameters=subprocess.list2cmdline(install_args)
    info.lpDirectory=str(ROOT);info.nShow=1
    shell=ctypes.WinDLL("shell32",use_last_error=True);kernel=ctypes.WinDLL("kernel32",use_last_error=True)
    shell.ShellExecuteExW.argtypes=[ctypes.POINTER(SHELLEXECUTEINFOW)];shell.ShellExecuteExW.restype=wintypes.BOOL
    kernel.WaitForSingleObject.argtypes=[wintypes.HANDLE,wintypes.DWORD];kernel.WaitForSingleObject.restype=wintypes.DWORD
    kernel.GetExitCodeProcess.argtypes=[wintypes.HANDLE,ctypes.POINTER(wintypes.DWORD)];kernel.GetExitCodeProcess.restype=wintypes.BOOL
    kernel.CloseHandle.argtypes=[wintypes.HANDLE]
    if not shell.ShellExecuteExW(ctypes.byref(info)):
        say("Administratorgodkjenning ble ikke gitt. Pluginpakken er fortsatt i dist.");return False
    kernel.WaitForSingleObject(info.hProcess,0xffffffff);code=wintypes.DWORD(1)
    kernel.GetExitCodeProcess(info.hProcess,ctypes.byref(code));kernel.CloseHandle(info.hProcess)
    return code.value==0

# --- Checked Windows toolchain (fix2) ----------------------------------------
# These helpers deliberately do not edit the registry or persistent PATH.
# SDK installation is opt-in through --install-sdk. All other environment
# changes apply only to compiler/build subprocesses.

SDK_DOWNLOAD_URL = (
    "https://download.microsoft.com/download/46742ab5-6592-4968-a793-129e7f3bc55a/"
    "KIT_BUNDLE_WINDOWSSDK_MEDIACREATION/winsdksetup.exe"
)
SDK_INSTALLER_VERSION = "10.0.26100.9457"
SDK_DOWNLOAD_PAGE = "https://learn.microsoft.com/en-us/windows/apps/windows-sdk/downloads"
SDK_REQUIRED_FILES = (
    "Include/{version}/um/Windows.h",
    "Include/{version}/um/winresrc.h",
    "Include/{version}/shared/sdkddkver.h",
    "Include/{version}/ucrt/stdio.h",
    "Include/{version}/ucrt/corecrt.h",
    "Lib/{version}/um/x64/kernel32.lib",
    "Lib/{version}/um/x64/user32.lib",
    "Lib/{version}/ucrt/x64/libucrt.lib",
    "Lib/{version}/ucrt/x64/ucrt.lib",
)
MSVC_REQUIRED_FILES = (
    "bin/Hostx64/x64/cl.exe", "bin/Hostx64/x64/link.exe",
    "bin/Hostx64/x64/lib.exe", "bin/Hostx64/x64/cvtres.exe",
    "include/vector", "include/vcruntime.h",
    "lib/x64/libcmt.lib", "lib/x64/libcpmt.lib", "lib/x64/libvcruntime.lib",
)


class WindowsSdk(NamedTuple):
    root: Path
    version: str
    bin_dir: Path


class CheckedTools(NamedTuple):
    environment: dict[str, str]
    msvc: Path
    sdk: WindowsSdk
    generator: str
    make_program: Path


class SdkNotFoundError(RuntimeError):
    pass


def unique_paths(paths: Sequence[Path]) -> list[Path]:
    seen: set[str] = set()
    output: list[Path] = []
    for path in paths:
        key = str(path.resolve()).casefold()
        if key not in seen:
            seen.add(key)
            output.append(path)
    return output


def registry_sdk_roots() -> list[Path]:
    """Read both registry views; a missing/denied key is not an installation."""
    try:
        import winreg
    except ImportError:
        return []
    found: list[Path] = []
    keys = (
        (r"SOFTWARE\Microsoft\Windows Kits\Installed Roots", "KitsRoot10"),
        (r"SOFTWARE\Microsoft\Microsoft SDKs\Windows\v10.0", "InstallationFolder"),
    )
    for hive in (winreg.HKEY_LOCAL_MACHINE, winreg.HKEY_CURRENT_USER):
        for view in (winreg.KEY_WOW64_32KEY, winreg.KEY_WOW64_64KEY):
            for key_name, value_name in keys:
                try:
                    with winreg.OpenKey(hive, key_name, 0, winreg.KEY_READ | view) as key:
                        value, _ = winreg.QueryValueEx(key, value_name)
                    if isinstance(value, str) and value.strip():
                        found.append(Path(os.path.expandvars(value.strip().strip('"'))))
                except OSError:
                    continue
    return unique_paths(found)


def sdk_roots(environment: Mapping[str, str]) -> list[Path]:
    env = {k.upper(): v for k, v in environment.items()}
    roots: list[Path] = []
    for key in ("FLYSNAPPER_SDK_DIR", "WINDOWSSDKDIR", "UNIVERSALCRTSDKDIR",
                "CMAKE_WINDOWS_KITS_10_DIR"):
        if env.get(key):
            roots.append(Path(env[key].strip().strip('"')))
    roots.extend(registry_sdk_roots())
    for key, default in (("PROGRAMFILES(X86)", r"C:\Program Files (x86)"),
                         ("PROGRAMFILES", r"C:\Program Files")):
        roots.append(Path(env.get(key, default)) / "Windows Kits/10")
    # Handle a custom SDK location already in a developer environment, even if
    # no registry record or WindowsSdkDir survived setup.
    for raw in env.get("PATH", "").split(";"):
        if not raw.strip():
            continue
        path = Path(raw.strip().strip('"'))
        if path.name.lower() not in ("x64", "x86", "arm64"):
            continue
        if version_parts(path.parent.name) and path.parent.parent.name.lower() == "bin":
            roots.append(path.parent.parent.parent)
        elif path.parent.name.lower() == "bin":
            roots.append(path.parent.parent)
    return unique_paths(roots)


def inspect_sdk(root: Path, version: str) -> tuple[WindowsSdk | None, list[str]]:
    """Select one coherent SDK, never mix headers/libs/tools from versions."""
    missing = [item.format(version=version) for item in SDK_REQUIRED_FILES
               if not (root / item.format(version=version)).is_file()]
    bin_dir = None
    for candidate in (root / "bin" / version / "x64", root / "bin" / version / "x86",
                      root / "bin/x64", root / "bin/x86"):
        if (candidate / "rc.exe").is_file() and (candidate / "mt.exe").is_file():
            bin_dir = candidate
            break
    if bin_dir is None:
        missing.append(f"bin/{version}/x64 (eller x86): rc.exe OG mt.exe")
    if missing:
        return None, missing
    assert bin_dir is not None
    return WindowsSdk(root.resolve(), version, bin_dir.resolve()), []


def find_windows_sdk(environment: Mapping[str, str]) -> WindowsSdk:
    roots = sdk_roots(environment)
    valid: list[WindowsSdk] = []
    diagnostics: list[str] = []
    for root in roots:
        if not root.is_dir():
            diagnostics.append(f"  {root}: ingen SDK-mappe")
            continue
        try:
            versions = sorted({p.name for section in ("Include", "Lib", "bin")
                               if (root / section).is_dir()
                               for p in (root / section).iterdir()
                               if p.is_dir() and len(version_parts(p.name)) == 4
                               and version_parts(p.name)[:2] == (10, 0)},
                              key=version_parts, reverse=True)
        except OSError as error:
            diagnostics.append(f"  {root}: kan ikke lese SDK-mappen ({error})")
            continue
        if not versions:
            diagnostics.append(f"  {root}: ingen versjonerte Windows 10/11 SDK-filer")
        for version in versions:
            sdk, missing = inspect_sdk(root, version)
            if sdk is not None:
                valid.append(sdk)
            else:
                diagnostics.append(f"  {root} | {version}: mangler " + ", ".join(missing))
    if valid:
        return max(valid, key=lambda sdk: version_parts(sdk.version))
    raise SdkNotFoundError(
        "Fant ingen komplett Windows SDK for x64 i registrerte eller vanlige SDK-mapper.\n"
        + "\n".join(diagnostics)
        + "\nDette er ikke Visual C++ Redistributable. Ingen eksisterende komponenter er fjernet.\n"
        "Kjor: python .\\build_plugin.py --install-sdk\n"
        "Da hentes Microsofts SDK-installasjonsprogram bare dersom SDK fortsatt mangler.\n"
        "Ved en egendefinert, uregistrert SDK-plassering kan FLYSNAPPER_SDK_DIR angi roten."
    )


def trusted_sdk_url(url: str) -> bool:
    value = urllib.parse.urlsplit(url)
    return (value.scheme == "https" and value.hostname == "download.microsoft.com"
            and value.username is None and value.password is None)


class MicrosoftSdkRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        if not trusted_sdk_url(newurl):
            raise RuntimeError("Avviser SDK-omdirigering utenfor download.microsoft.com over HTTPS.")
        return super().redirect_request(req, fp, code, msg, headers, newurl)


def powershell_executable() -> Path:
    # Use Windows' own PowerShell rather than an executable from a project dir.
    value = Path(os.environ.get("SystemRoot", r"C:\Windows")) / "System32/WindowsPowerShell/v1.0/powershell.exe"
    if not value.is_file():
        raise RuntimeError("Fant ikke Windows PowerShell for signaturkontroll av Microsoft-installasjonsfilen.")
    return value


def verify_microsoft_signature(path: Path) -> None:
    """Trust Windows Authenticode, not only an HTTPS URL or an EXE extension."""
    command = (
        "$ErrorActionPreference='Stop'; "
        "$s=Get-AuthenticodeSignature -LiteralPath $env:FLYSNAPPER_SDK_SETUP; "
        "if ($s.Status -ne 'Valid') {throw ('SDK signature: '+$s.Status)}; "
        "if (-not $s.SignerCertificate -or "
        "$s.SignerCertificate.GetNameInfo([System.Security.Cryptography.X509Certificates.X509NameType]::SimpleName,$false) "
        "-ne 'Microsoft Corporation') {throw 'SDK installer is not signed by Microsoft Corporation'}; "
        "Write-Output 'FLYSNAPPER_MICROSOFT_SIGNATURE_OK'"
    )
    env = dict(os.environ)
    env["FLYSNAPPER_SDK_SETUP"] = str(path)
    output = run([powershell_executable(), "-NoLogo", "-NoProfile", "-NonInteractive", "-Command", command],
                 capture=True, env=env)
    if "FLYSNAPPER_MICROSOFT_SIGNATURE_OK" not in output:
        raise RuntimeError("Microsoft-signaturen ble ikke bekreftet. Installasjonsfilen blir ikke kjort.")


def download_sdk_installer(destination: Path) -> Path:
    destination.mkdir(parents=True, exist_ok=True)
    setup = destination / "winsdksetup.exe"
    partial = destination / "winsdksetup.download"
    if not trusted_sdk_url(SDK_DOWNLOAD_URL):
        raise RuntimeError("Ugyldig Microsoft SDK-nedlastingsadresse i byggeskriptet.")
    say("Henter Microsoft Windows SDK " + SDK_INSTALLER_VERSION + " fra download.microsoft.com.")
    request = urllib.request.Request(SDK_DOWNLOAD_URL, headers={"User-Agent": "Flysnapper-Build/" + BUILDER_REVISION})
    try:
        opener = urllib.request.build_opener(MicrosoftSdkRedirect())
        with opener.open(request, timeout=120) as response, partial.open("wb") as out:
            if not trusted_sdk_url(response.geturl()):
                raise RuntimeError("Nedlastingen kom ikke fra den tillatte Microsoft-serveren.")
            total = 0
            while True:
                chunk = response.read(1024 * 1024)
                if not chunk:
                    break
                total += len(chunk)
                if total > 64 * 1024 * 1024:
                    raise RuntimeError("SDK-bootstrapperen er uventet stor. Avbryter nedlastingen.")
                out.write(chunk)
        with partial.open("rb") as handle:
            if handle.read(2) != b"MZ":
                raise RuntimeError("Microsoft-nedlastingen er ikke en Windows-installasjonsfil.")
        partial.replace(setup)
        verify_microsoft_signature(setup)
        say("Microsoft-signaturen er kontrollert.")
        return setup
    finally:
        if partial.exists():
            partial.unlink()


def install_windows_sdk() -> None:
    """Only called after an explicit --install-sdk and a failed SDK inventory.

    Uses Microsoft's standalone SDK bootstrapper, NOT Visual Studio repair.
    The downloaded EXE and its dependencies are not bundled with Flysnapper.
    Requires UAC consent. /norestart prevents an unrequested reboot.
    """
    LOG_DIR.mkdir(parents=True, exist_ok=True)
    sdk_log = LOG_DIR / ("sdk-install-" + dt.datetime.now().strftime("%Y%m%d-%H%M%S") + ".log")
    # Keep the EXE in a unique directory for this run, not in a reusable cache.
    # No embedded source strings are formed from user-supplied paths.
    with tempfile.TemporaryDirectory(prefix="flysnapper-sdk-") as tmp:
        setup = download_sdk_installer(Path(tmp))
        args = ["/q", "/norestart", "/ceip", "off", "/features",
                "OptionId.DesktopCPPx64", "/log", str(sdk_log)]
        env = dict(os.environ)
        env["FLYSNAPPER_SDK_SETUP"] = str(setup)
        env["FLYSNAPPER_SDK_ARGS"] = subprocess.list2cmdline(args)
        command = (
            "$ErrorActionPreference='Stop'; "
            "$s=Get-AuthenticodeSignature -LiteralPath $env:FLYSNAPPER_SDK_SETUP; "
            "if ($s.Status -ne 'Valid' -or -not $s.SignerCertificate -or "
            "$s.SignerCertificate.GetNameInfo([System.Security.Cryptography.X509Certificates.X509NameType]::SimpleName,$false) "
            "-ne 'Microsoft Corporation') {throw 'SDK signature check failed'}; "
            "$p=Start-Process -FilePath $env:FLYSNAPPER_SDK_SETUP "
            "-ArgumentList $env:FLYSNAPPER_SDK_ARGS -Verb RunAs -PassThru -Wait; "
            "Write-Output ('FLYSNAPPER_SDK_EXIT='+$p.ExitCode)"
        )
        say("Installerer bare Windows SDK-komponenten for Desktop C++ x64. Windows ber om godkjenning.")
        say("Ingen automatisk omstart. Microsofts installasjonslogg: " + str(sdk_log))
        output = run([powershell_executable(), "-NoLogo", "-NoProfile", "-NonInteractive", "-Command", command],
                     capture=True, env=env)
        match = re.search(r"FLYSNAPPER_SDK_EXIT=(-?\d+)", output)
        if not match:
            raise RuntimeError("SDK-installasjonen returnerte ingen sluttkode. Se " + str(sdk_log))
        code = int(match.group(1))
        if code == 3010:
            raise RuntimeError("Windows SDK er installert, men Microsoft ber om omstart. "
                               "Start Windows pa nytt og kjor python .\\build_plugin.py igjen. Ingen plugin er installert.")
        if code != 0:
            raise RuntimeError(f"Microsofts SDK-installasjon feilet (kode {code}). "
                               f"Ingen automatisk reparasjon eller omstart. Se {sdk_log}")
    say("Microsofts SDK-installasjon er ferdig. Kontrollerer filene pa nytt.")


def ensure_windows_sdk(environment: Mapping[str, str], allow_install: bool) -> WindowsSdk:
    try:
        return find_windows_sdk(environment)
    except SdkNotFoundError as error:
        if not allow_install:
            raise
        say(str(error))
        install_windows_sdk()
        # SDK registry keys are read afresh. Do not trust installer exit code
        # alone, nor download/install repeatedly if files remain missing.
        return find_windows_sdk(environment)


def find_complete_msvc(installation: Path) -> Path:
    base = installation / "VC/Tools/MSVC"
    versions = sorted((p for p in base.glob("*") if p.is_dir() and version_parts(p.name)),
                      key=lambda p: version_parts(p.name), reverse=True)
    incomplete: list[str] = []
    for folder in versions:
        missing = [rel for rel in MSVC_REQUIRED_FILES if not (folder / rel).is_file()]
        if not missing:
            return folder.resolve()
        incomplete.append(f"  {folder}: " + ", ".join(missing))
    raise RuntimeError("MSVC-kompilatoren alene er ikke et komplett x64-byggesett.\n"
                       + "\n".join(incomplete)
                       + "\nLegg til C++ x64/x86 build tools i den eksisterende Build Tools-installasjonen. "
                         "Ikke endre Visual C++ Redistributable.")


def explicit_build_environment(base: Mapping[str, str], msvc: Path, sdk: WindowsSdk) -> dict[str, str]:
    env = {k.upper(): v for k, v in base.items()}
    toolbin = msvc / "bin/Hostx64/x64"
    # Existing SDK/CRT include/lib paths are deliberately NOT mixed with the
    # selected version. There are no external libraries required by Flysnapper.
    includes = [msvc / "include"]
    libs = [msvc / "lib/x64"]
    if (msvc / "atlmfc/include").is_dir():
        includes.append(msvc / "atlmfc/include")
    if (msvc / "atlmfc/lib/x64").is_dir():
        libs.append(msvc / "atlmfc/lib/x64")
    includes.extend(sdk.root / "Include" / sdk.version / part
                    for part in ("ucrt", "shared", "um", "winrt", "cppwinrt")
                    if (sdk.root / "Include" / sdk.version / part).is_dir())
    libs.extend(sdk.root / "Lib" / sdk.version / part / "x64" for part in ("ucrt", "um"))
    env["INCLUDE"] = ";".join(map(str, includes))
    env["LIB"] = ";".join(map(str, libs))
    env["LIBPATH"] = env["LIB"]
    prefix = ";".join(map(str, (toolbin, sdk.bin_dir)))
    env["PATH"] = prefix + (";" + env["PATH"] if env.get("PATH") else "")
    env["VCTOOLSINSTALLDIR"] = str(msvc) + "\\"
    env["VCTOOLSVERSION"] = msvc.name
    env["WINDOWSSDKDIR"] = str(sdk.root) + "\\"
    env["WINDOWSSDKVERSION"] = sdk.version + "\\"
    env["WINDOWSSDKLIBVERSION"] = sdk.version + "\\"
    env["WINDOWSSDKBINPATH"] = str(sdk.root / "bin") + "\\"
    env["WINDOWSSDKVERBINPATH"] = str(sdk.root / "bin" / sdk.version) + "\\"
    env["UNIVERSALCRTSDKDIR"] = str(sdk.root) + "\\"
    env["UCRTVERSION"] = sdk.version
    env["VSCMD_ARG_TGT_ARCH"] = "x64"
    env["VSCMD_ARG_HOST_ARCH"] = "x64"
    for key in ("CMAKE_GENERATOR", "CMAKE_GENERATOR_INSTANCE", "CMAKE_GENERATOR_PLATFORM",
                "CMAKE_GENERATOR_TOOLSET", "CC", "CXX", "RC", "CL", "_CL_", "LINK", "_LINK_",
                "CFLAGS", "CXXFLAGS", "LDFLAGS"):
        env.pop(key, None)
    return env


def prepare_checked_tools(installation: Path, generators: Sequence[str], allow_sdk_install: bool) -> CheckedTools:
    # Microsoft's script remains preferred for auxiliary variables. But a
    # missing SDK in that script's PATH is NOT proof that the SDK is absent.
    try:
        base = developer_environment(installation)
    except RuntimeError as error:
        say("Microsofts automatiske miljooppsett var ufullstendig. Kontrollerer verktøy og SDK-filer direkte.")
        say(str(error))
        base = dict(os.environ)
    msvc = find_complete_msvc(installation)
    sdk = ensure_windows_sdk(base, allow_sdk_install)
    env = explicit_build_environment(base, msvc, sdk)
    # Prefer the known installed make executable over an unrelated PATH entry.
    ninja = installation / "Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe"
    nmake = msvc / "bin/Hostx64/x64/nmake.exe"
    if "Ninja" in generators and ninja.is_file():
        generator, make_program = "Ninja", ninja
    elif "NMake Makefiles" in generators and nmake.is_file():
        generator, make_program = "NMake Makefiles", nmake
    else:
        generator, make_program = choose_direct_generator(installation, env, generators)
    say("MSVC x64: " + str(msvc))
    say("Windows SDK: " + sdk.version + " | " + str(sdk.root))
    say("Ressurskompilator: " + str(sdk.bin_dir / "rc.exe"))
    say("Manifestverktoy: " + str(sdk.bin_dir / "mt.exe"))
    say("Byggemetode: " + generator + " (direkte MSVC; ingen Visual Studio-instansregistrering kreves)")
    return CheckedTools(env, msvc, sdk, generator, make_program.resolve())


def checked_cmake_flags(tools: CheckedTools) -> list[str]:
    binpath = tools.msvc / "bin/Hostx64/x64"
    return [
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_OBJECT_PATH_MAX=220",
        "-DCMAKE_C_COMPILER:FILEPATH=" + (binpath / "cl.exe").as_posix(),
        "-DCMAKE_CXX_COMPILER:FILEPATH=" + (binpath / "cl.exe").as_posix(),
        "-DCMAKE_RC_COMPILER:FILEPATH=" + (tools.sdk.bin_dir / "rc.exe").as_posix(),
        "-DCMAKE_MT:FILEPATH=" + (tools.sdk.bin_dir / "mt.exe").as_posix(),
        "-DCMAKE_LINKER:FILEPATH=" + (binpath / "link.exe").as_posix(),
        "-DCMAKE_AR:FILEPATH=" + (binpath / "lib.exe").as_posix(),
        "-DCMAKE_MAKE_PROGRAM:FILEPATH=" + tools.make_program.as_posix(),
        "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded$<$<CONFIG:Debug>:Debug>",
    ]


def checked_build_directory(tools: CheckedTools, route: str) -> Path:
    identity = "|".join((str(ROOT), str(tools.msvc), str(tools.sdk.root), tools.sdk.version,
                         str(tools.sdk.bin_dir), tools.generator, str(tools.make_program)))
    token = hashlib.sha256(identity.encode("utf-8")).hexdigest()[:12]
    return ROOT / "build" / f"x64-{route}-fix4-{token}"


def preflight_toolchain(cmake: str, tools: CheckedTools) -> None:
    """Actually compile RC + C++/CRT, link a Windows EXE and run it.

    Tests both SDK headers/libraries and rc/mt/cvtres before starting JUCE.
    It cannot be replaced by 'the files exist' or a mocked discovery test.
    """
    build = checked_build_directory(tools, "sdk-check")
    source = build / "source"
    source.mkdir(parents=True, exist_ok=True)
    (source / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.22)\n"
        "project(FlysnapperToolchainCheck LANGUAGES C CXX RC)\n"
        "set(CMAKE_CXX_STANDARD 17)\nset(CMAKE_CXX_STANDARD_REQUIRED ON)\n"
        "add_executable(FlysnapperToolchainCheck probe.cpp probe.rc)\n"
        "target_link_libraries(FlysnapperToolchainCheck PRIVATE user32)\n"
        "target_compile_options(FlysnapperToolchainCheck PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/utf-8>)\n",
        encoding="utf-8")
    (source / "probe.cpp").write_text(
        '#include <windows.h>\n#include <vector>\n#include <thread>\n#include <cstdio>\n'
        'int main() {\n'
        ' std::vector<int> v{1, 2, 3}; int result=0;\n'
        ' std::thread t([&]{for (int n : v) result+=n;}); t.join();\n'
        ' wchar_t text[128]{};\n'
        ' if (!GetCurrentProcessId() || result!=6 || LoadStringW(GetModuleHandleW(nullptr),101,text,128)<=0) return 1;\n'
        ' std::puts("FLYSNAPPER_TOOLCHAIN_OK: C++17, CRT, Windows SDK, RC and linker");\n'
        ' return 0;\n}\n', encoding="utf-8")
    (source / "probe.rc").write_text(
        '#include <winresrc.h>\nSTRINGTABLE\nBEGIN\n101 "Flysnapper SDK check"\nEND\n', encoding="ascii")
    say("Kontrollerer byggemiljoet med faktisk C++- og ressurskompilering, linking og kjøring.")
    run([cmake, "-S", source, "-B", build, "-G", tools.generator, *checked_cmake_flags(tools)], env=tools.environment)
    run([cmake, "--build", build, "--config", "Release", "--target", "FlysnapperToolchainCheck"], env=tools.environment)
    exe = build / "FlysnapperToolchainCheck.exe"
    if not exe.is_file():
        raise RuntimeError("SDK-kontrollen produserte ingen Windows-testfil. Stopper for Flysnapper-byggingen.")
    output = run([exe], env=tools.environment, capture=True)
    if "FLYSNAPPER_TOOLCHAIN_OK" not in output:
        raise RuntimeError("Den kompilerte SDK-kontrollen bestod ikke. Stopper for Flysnapper-byggingen.")
    say(output)


def configure_checked_project(cmake: str, juce: Path, tools: CheckedTools) -> Path:
    previous = checked_build_directory(tools, "direct")
    # The actual Windows CI failure was a 264-character /Fo path. The longer
    # ProcessorAudioTests target crossed MAX_PATH while shorter targets passed.
    # Keep original Unicode source paths and resource probes, but use a compact,
    # separate build namespace and force CMake's object-directory hashing.
    token = hashlib.sha256((str(previous) + "|unicode-short-v1|" + JUCE_COMMIT).encode("utf-8")).hexdigest()[:12]
    build = ROOT / "build" / ("w-" + token)
    # Even a directory-hashed object needs this fixed target/hash/basename tail.
    # Fail before expensive compilation if the checkout is too deep for it.
    minimum_tail = "CMakeFiles/FlysnapperDynamicsProcessorTests.dir/" + "0" * 32 + "/juce_audio_processors_headless_lv2_libs.cpp.obj"
    if len(str(build)) + 1 + len(minimum_tail) > 220:
        raise RuntimeError("Windows-kildebanen er for dyp for sikre MSVC-objektfiler. Bruk den ferdigbygde Windows-pakken; utviklerbygg krever en kortere utsjekkingsbane. Ingen kildefiler er flyttet eller endret.")
    run([cmake, "-S", ROOT, "-B", build, "-G", tools.generator,
         *checked_cmake_flags(tools), f"-DJUCE_DIR={juce}",
         "-DFLYSNAPPER_CORE_ONLY=OFF", "-DFLYSNAPPER_BUILD_TESTS=ON"], env=tools.environment)
    return build
# --- End checked Windows toolchain ------------------------------------------


# --- JUCE Windows command-line Unicode fix ---------------------------------
# JUCE 8.0.16-security's juceaide interprets char** argv as UTF-8. On Windows
# main() receives the active ANSI code page, so e.g. the CP1252 byte for ae
# (U+00E6) is NOT UTF-8. Use wmain() for this build tool; do not change Windows
# locale settings, the user's profile, the DSP, or the clean dependency clone.
JUCEAIDE_PATCH_REVISION = "windows-wide-argv-v1"
JUCEAIDE_MAIN = "extras/Build/juceaide/Main.cpp"
JUCEAIDE_PATCH_MANIFEST = ".flysnapper-unicode-patch.json"

_JUCE_MAIN_BEFORE = "int main (int argc, char** argv)\n{"
_JUCE_MAIN_AFTER = """// Flysnapper build-tool patch: Windows paths arrive losslessly as UTF-16.
#if JUCE_WINDOWS
int wmain (int argc, wchar_t** argv)
#else
int main (int argc, char** argv)
#endif
{"""
_JUCE_ARGUMENTS_BEFORE = """        const auto getString = [&] (const char* text)
        {
            return juce::String (juce::CharPointer_UTF8 (text));
        };"""
_JUCE_ARGUMENTS_AFTER = """       #if JUCE_WINDOWS
        const auto getString = [] (const wchar_t* text)
        {
            return juce::String (text);
        };
       #else
        const auto getString = [] (const char* text)
        {
            return juce::String (juce::CharPointer_UTF8 (text));
        };
       #endif"""
_JUCE_DISPATCH_BEFORE = "            return it->second (std::move (argumentList));"
_JUCE_DISPATCH_AFTER = """            // Preserve JUCE's descriptive file/argument errors instead of
            // allowing the catch-all below to replace them by 'Unhandled exception'.
            return juce::ConsoleApplication::invokeCatchingFailures ([&]
            {
                return it->second (std::move (argumentList));
            });"""


def patched_juceaide_source(original: str) -> str:
    """Apply only the three verified snippets from the pinned JUCE revision.

    Fail closed on any unexpected source; never perform a fuzzy replacement.
    The original upstream licence header and all other source are preserved.
    """
    original = original.replace("\r\n", "\n")
    replacements = (
        (_JUCE_MAIN_BEFORE, _JUCE_MAIN_AFTER),
        (_JUCE_ARGUMENTS_BEFORE, _JUCE_ARGUMENTS_AFTER),
        (_JUCE_DISPATCH_BEFORE, _JUCE_DISPATCH_AFTER),
    )
    for before, _ in replacements:
        if original.count(before) != 1:
            raise RuntimeError(
                "JUCE-hjelpeverktoyet har uventet kildekode. "
                "Unicode-rettelsen brukes ikke pa en ukjent versjon. Ingen originalfiler endret."
            )
    patched = original
    for before, after in replacements:
        patched = patched.replace(before, after, 1)
    return patched


def source_file_hashes(root: Path) -> dict[str, str]:
    """Content inventory of the dependency; exclude Git metadata, not sources."""
    result: dict[str, str] = {}
    for directory, dirs, names in os.walk(root, followlinks=False):
        dirs[:] = sorted(d for d in dirs if d != ".git")
        for name in dirs:
            if (Path(directory) / name).is_symlink():
                raise RuntimeError("Uventet symbolsk mappe i JUCE-kilden; ingen filer endret.")
        for name in sorted(names):
            path = Path(directory) / name
            relative = path.relative_to(root).as_posix()
            if name == ".git" or relative == JUCEAIDE_PATCH_MANIFEST:
                continue
            if path.is_symlink():
                raise RuntimeError("Uventet symbolsk fil i JUCE-kilden; ingen filer endret.")
            digest = hashlib.sha256()
            with path.open("rb") as source:
                for block in iter(lambda: source.read(1024 * 1024), b""):
                    digest.update(block)
            result[relative] = digest.hexdigest()
    return result


def prepare_unicode_juce(juce: Path) -> Path:
    """Create a verified private build copy. Leave deps/JUCE completely untouched.

    The directory is content-addressed and published only after all source
    hashes match. A partial/interrupted copy is never used as a completed copy.
    No network requests, system changes, or modifications of the user's source.
    """
    source = juce / JUCEAIDE_MAIN
    patched = patched_juceaide_source(source.read_text(encoding="utf-8"))
    expected = source_file_hashes(juce)
    for required in (JUCEAIDE_MAIN, "CMakeLists.txt", "LICENSE.md"):
        if required not in expected:
            raise RuntimeError("JUCE-kilden mangler " + required + ". Ingen originalfiler endret.")
    original_main_hash = expected[JUCEAIDE_MAIN]
    expected[JUCEAIDE_MAIN] = hashlib.sha256(patched.encode("utf-8")).hexdigest()
    identity = json.dumps(
        {"commit": JUCE_COMMIT, "patch": JUCEAIDE_PATCH_REVISION, "files": expected},
        sort_keys=True, separators=(",", ":"), ensure_ascii=True,
    )
    token = hashlib.sha256(identity.encode("utf-8")).hexdigest()[:20]
    parent = ROOT / "build" / "j"
    destination = parent / ("u-" + token)
    manifest = {
        "upstream_commit": JUCE_COMMIT, "patch": JUCEAIDE_PATCH_REVISION,
        "original_main_sha256": original_main_hash,
        "patched_main_sha256": expected[JUCEAIDE_MAIN],
        "source_files": len(expected), "identity_sha256": hashlib.sha256(identity.encode("utf-8")).hexdigest(),
    }
    if destination.exists():
        marker = destination / JUCEAIDE_PATCH_MANIFEST
        try:
            saved = json.loads(marker.read_text(encoding="utf-8"))
        except (OSError, ValueError) as error:
            raise RuntimeError("Byggekopien av JUCE mangler gyldig kontrollfil. Ingen filer overskrevet.") from error
        if saved != manifest or source_file_hashes(destination) != expected:
            raise RuntimeError("Byggekopien av JUCE er endret etter kontroll. Ingen filer overskrevet.")
        say("Kontrollert JUCE-byggekopi med Windows Unicode-rettelse brukes pa nytt.")
        return destination
    parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="juce-unicode-staging-", dir=parent) as temp:
        staging = Path(temp) / "JUCE"
        shutil.copytree(juce, staging, ignore=shutil.ignore_patterns(".git"))
        # Binary write makes the hash independent of Python's Windows newline
        # translation. The original Git checkout is never patched.
        (staging / JUCEAIDE_MAIN).write_bytes(patched.encode("utf-8"))
        if source_file_hashes(staging) != expected:
            raise RuntimeError("JUCE-byggekopien bestod ikke filkontrollen. Stopper for kompilering.")
        (staging / JUCEAIDE_PATCH_MANIFEST).write_text(
            json.dumps(manifest, indent=2, ensure_ascii=True) + "\n", encoding="utf-8")
        staging.rename(destination)
    say("JUCE-hjelpeverktoyet er rettet for Unicode-filbaner i en separat byggekopi.")
    return destination


def preflight_juceaide_unicode(build: Path, tools: CheckedTools) -> None:
    """Run the actual just-built Windows juceaide and resource compiler.

    This test is automatic. It checks the operation that failed in the user's
    log, with spaces and Norwegian letters in BOTH input and output filenames.
    It does not mistake an old output, a mocked return code, or just 'version'
    for a successful resource generation.
    """
    helper = build / "juce/tools/extras/Build/juceaide/juceaide_artefacts/Custom/juceaide.exe"
    if not helper.is_file():
        candidates = list((build / "juce").rglob("juceaide.exe"))
        if len(candidates) != 1:
            raise RuntimeError("Fant ikke det entydige kompilerte juceaide.exe for ressurskontrollen.")
        helper = candidates[0]
    info = build / "Flysnapper_artefacts/JuceLibraryCode/Info.txt"
    if not info.is_file():
        raise RuntimeError("CMake produserte ikke Flysnappers Info.txt. Ingen plugin installert.")
    # Escapes keep this Python source portable, but create real Unicode names.
    folder = build / "juceaide-unicode-check" / "Olav L\u00e6greid"
    folder.mkdir(parents=True, exist_ok=True)
    input_file = folder / "Info \u00e6\u00f8\u00e5.txt"
    output_file = folder / "Flysnapper \u00e6\u00f8\u00e5.rc"
    resource_file = folder / "Flysnapper \u00e6\u00f8\u00e5.res"
    input_file.write_bytes(info.read_bytes())
    for file in (output_file, resource_file):
        file.unlink(missing_ok=True)
    say("Kontrollerer JUCEs faktiske ressursgenerering med norske tegn i filbanene.")
    run([helper, "rcfile", input_file, output_file], env=tools.environment, capture=True)
    if not output_file.is_file():
        raise RuntimeError("JUCE-ressurskontrollen opprettet ingen fil. Ingen plugin installert.")
    data = output_file.read_bytes()
    if b"VERSIONINFO" not in data or b"Flysnapper" not in data:
        raise RuntimeError("JUCE-ressurskontrollen ga uventet innhold. Ingen plugin installert.")
    run([tools.sdk.bin_dir / "rc.exe", "/nologo", "/fo", resource_file, output_file],
        env=tools.environment, capture=True)
    if not resource_file.is_file() or resource_file.stat().st_size == 0:
        raise RuntimeError("Windows ressurskompilator produserte ingen ressurs. Ingen plugin installert.")
    say("FLYSNAPPER_UNICODE_OK: JUCE-generering og RC-kompilering med norske filnavn bestatt.")
# --- End JUCE Windows Unicode fix ------------------------------------------


# --- Local build workspace / chosen VST3 destination (fix4) -------------------
@contextmanager
def local_temporary_directory():
    """Use only this process's project-local TEMP/TMP; restore on every exit.

    No Windows user/system environment settings are changed. Child tools inherit
    this directory, so a project on D: also keeps its temporary build data on D:.
    Existing Python, Git, MSVC and Windows SDK installations stay where they are.
    """
    folder = ROOT / "build" / "temp"
    folder.mkdir(parents=True, exist_ok=True)
    keys = ("TEMP", "TMP", "TMPDIR")
    previous = {key: os.environ.get(key) for key in keys}
    previous_python = tempfile.tempdir
    try:
        for key in keys:
            os.environ[key] = str(folder)
        tempfile.tempdir = str(folder)
        yield folder
    finally:
        tempfile.tempdir = previous_python
        for key, value in previous.items():
            if value is None:
                os.environ.pop(key, None)
            else:
                os.environ[key] = value


def validate_install_directory(folder: Path) -> Path:
    folder = Path(folder)
    if not folder.is_absolute():
        raise RuntimeError("VST3-mappen ma oppgis som en full filbane.")
    folder = folder.resolve()
    if folder.suffix.lower() == ".vst3":
        raise RuntimeError("Velg mappen som inneholder VST3-plugins, ikke en .vst3-pakke.")
    if folder == ROOT or ROOT in folder.parents:
        raise RuntimeError("Velg en separat VST3-mappe, ikke kildekode-, build- eller dist-mappen.")
    if folder.exists() and not folder.is_dir():
        raise RuntimeError("Valgt VST3-plassering er ikke en mappe.")
    return folder


def choose_install_directory() -> Path | None:
    """Native Windows folder picker; output uses base64 to preserve Unicode.

    This only selects a destination. It does not change Ableton settings or move
    other plugins. Cancel retains the completed, tested bundle in dist.
    """
    env = dict(os.environ)
    env["FLYSNAPPER_PICKER_START"] = str(ROOT.anchor)
    record = DIST / "installation.json"
    if record.is_file():
        try:
            installed = Path(json.loads(record.read_text(encoding="utf-8"))["path"]).parent
            if installed.is_dir():
                env["FLYSNAPPER_PICKER_START"] = str(installed)
        except (ValueError, TypeError, KeyError, OSError):
            pass
    command = (
        "$ErrorActionPreference='Stop'; "
        "Add-Type -AssemblyName System.Windows.Forms; "
        "[System.Windows.Forms.Application]::EnableVisualStyles(); "
        "$d=New-Object System.Windows.Forms.FolderBrowserDialog; "
        "$d.Description='Flysnapper: Select your existing VST3 plug-in folder. "
        "Do not select a VST2 folder or the Flysnapper project folder.'; "
        "$d.ShowNewFolderButton=$true; "
        "$d.SelectedPath=$env:FLYSNAPPER_PICKER_START; "
        "try { if ($d.ShowDialog() -eq [System.Windows.Forms.DialogResult]::OK) { "
        "[Console]::WriteLine('FLYSNAPPER_FOLDER='+[Convert]::ToBase64String("
        "[Text.Encoding]::UTF8.GetBytes($d.SelectedPath))) "
        "} else { [Console]::WriteLine('FLYSNAPPER_FOLDER_CANCELLED') } } "
        "finally { $d.Dispose() }"
    )
    say("Velg din eksisterende VST3-mappe i vinduet som apnes. Ingen Ableton-innstillinger endres.")
    output = run([powershell_executable(), "-NoLogo", "-NoProfile", "-STA", "-Command", command],
                 capture=True, env=env)
    match = re.search(r"(?m)^FLYSNAPPER_FOLDER=([A-Za-z0-9+/=]+)\s*$", output)
    if match:
        decoded = base64.b64decode(match.group(1), validate=True).decode("utf-8")
        return validate_install_directory(Path(decoded))
    if "FLYSNAPPER_FOLDER_CANCELLED" in output:
        return None
    raise RuntimeError("Mappevalget ga ikke et gyldig svar. Den ferdige pluginpakken beholdes i dist.")


def finish_installation(choose: bool, directory: Path | None) -> bool:
    if choose:
        directory = choose_install_directory()
        if directory is None:
            say("Ingen installasjon valgt. Testet pluginpakke er beholdt i dist.")
            return False
    if directory is not None:
        directory = validate_install_directory(directory)
        try:
            install_bundle(directory)
            return True
        except PermissionError:
            say("Windows ber om godkjenning for kopiering til den valgte mappen.")
            return elevate_install(directory)
    if ctypes.windll.shell32.IsUserAnAdmin():
        install_bundle()
        return True
    say("Windows ber om administratorgodkjenning bare for kopiering til VST3-systemmappen.")
    return elevate_install()


REQUIRED_TESTS = set(midi_only_evidence.REQUIRED_TESTS)


def verified_juce(git: str, directory: Path | None) -> Path:
    if directory is None:
        return acquire_juce(git)
    directory = directory.resolve()
    actual = run([git, "-C", directory, "rev-parse", "HEAD"], capture=True).strip()
    dirty = run([git, "-C", directory, "status", "--porcelain"], capture=True).strip()
    if actual != JUCE_COMMIT or dirty:
        raise RuntimeError("--juce-dir ma peke pa den uendrede, fastlasste JUCE-revisjonen.")
    return directory


def run_required_tests(ctest: str | Path, build: Path, build_env=None) -> None:
    inventory = json.loads(run([ctest, "--test-dir", build, "-C", "Release", "--show-only=json-v1"],
                               capture=True, env=build_env))
    names = {test["name"] for test in inventory.get("tests", [])}
    if not REQUIRED_TESTS <= names:
        raise RuntimeError("Testoppsettet mangler obligatoriske tester: " + ", ".join(sorted(REQUIRED_TESTS - names)))
    result_file = build / "flysnapper-ctest.xml"
    result_file.unlink(missing_ok=True)
    # A passing test inventory must not reuse an earlier frontend capture.
    for name in (n for spec in midi_only_evidence.SPECS for n in spec[2:4]):
        (build / name).unlink(missing_ok=True)
    run([ctest, "--test-dir", build, "-C", "Release", "--no-tests=error", "--verbose", "--output-on-failure",
         "--output-junit", result_file],
        env=build_env)
    if not result_file.is_file():
        raise RuntimeError("CTest produserte ingen ny resultatfil. Installerer ikke.")
    cases = ET.parse(result_file).findall(".//testcase")
    passed = {case.get("name") for case in cases
              if case.find("skipped") is None and case.find("failure") is None and case.find("error") is None
              and case.get("status", "run") not in ("notrun", "disabled")}
    if not REQUIRED_TESTS <= passed:
        raise RuntimeError("Obligatoriske tester ble ikke fullfort uten feil eller skip: " + ", ".join(sorted(REQUIRED_TESTS - passed)))


def collect_result(build: Path, native: bool, extra_manifest=None) -> Path:
    platform_name = "Linux-x86_64" if native else "Windows-x64"
    suffix = midi_only_evidence.LINUX_BINARY if native else midi_only_evidence.WINDOWS_BINARY
    source = build / "Flysnapper_artefacts/Release/VST3" / midi_only_evidence.BUNDLE
    if not (source / suffix).is_file():
        raise RuntimeError("Fant ikke forventet bygget VST3-pakke: " + str(source / suffix))
    output = DIST / "linux-validation" if native else DIST
    output.mkdir(parents=True, exist_ok=True)
    target = output / midi_only_evidence.BUNDLE
    staging = output / (midi_only_evidence.BUNDLE + ".staging")
    if staging.exists():
        raise RuntimeError("En ufullstendig dist-kopi finnes. Ingen filer overskrives: " + str(staging))
    shutil.copytree(source, staging)
    if bundle_hashes(staging) != bundle_hashes(source):
        raise RuntimeError("Dist-kopien bestod ikke kontrollsummene.")
    if target.exists():
        shutil.rmtree(target)
    staging.rename(target)
    for name in ("README.md", "LICENSE.txt", "THIRD_PARTY.md"):
        if (ROOT / name).is_file():
            shutil.copy2(ROOT / name, output / name)
    test_log = build / "Testing/Temporary/LastTest.log"
    if not test_log.is_file():
        raise RuntimeError("Ingen testlogg fra denne byggingen.")
    shutil.copy2(test_log, output / "automated-tests.txt")
    shutil.copy2(build / "flysnapper-ctest.xml", output / "test-results.xml")
    manifest = {
        "version": midi_only_evidence.VERSION, "product": "Flysnapper MIDI", "bundle_name": midi_only_evidence.BUNDLE,
        "stage": midi_only_evidence.STAGE, "evaluation_scope": dict(midi_only_evidence.SCOPE),
        "builder_revision": BUILDER_REVISION, "platform": platform_name, "juce_commit": JUCE_COMMIT,
        "built_at": dt.datetime.now(dt.timezone.utc).isoformat(), "python": platform.python_version(),
        "source_commit": os.environ.get("GITHUB_SHA"),
        "vst3_sha256": hashlib.sha256((target / suffix).read_bytes()).hexdigest(),
        "bundle_sha256": bundle_hashes(target), "tests": "passed",
        "required_test_names": sorted(REQUIRED_TESTS), "ableton_verified": False,
        "pitch_detection_implemented": False, "audio_processing_implemented": False,
    }
    manifest.update(extra_manifest or {})
    manifest["midi_only_evidence"] = midi_only_evidence.collect(ROOT, build, output, manifest)
    midi_only_evidence.verify(ROOT, output, manifest)
    (output / "build-manifest.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return target


def argument_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--native", action="store_true", help="Build/test native Linux x86_64; not a Windows plugin")
    parser.add_argument("--juce-dir", type=Path, help="Existing clean checkout of the exact pinned JUCE commit")
    parser.add_argument("--jobs", type=int, default=min(2, os.cpu_count() or 2), help="Compile jobs; default 2 to bound memory")
    parser.add_argument("--install-sdk", action="store_true", help="Windows: explicitly permit missing SDK installation with UAC")
    mode = parser.add_mutually_exclusive_group()
    mode.add_argument("--install", action="store_true", help="Windows: install only after all build/test checks pass")
    mode.add_argument("--install-only", action="store_true", help="Windows: install a previously tested, hash-verified dist bundle")
    dest = parser.add_mutually_exclusive_group()
    dest.add_argument("--choose-install-dir", action="store_true", help="With --install/--install-only: choose VST3 folder")
    dest.add_argument("--install-dir", type=Path, help="With --install/--install-only: VST3 parent directory")
    return parser


def main(argv=None) -> int:
    global _log
    parser = argument_parser()
    args = parser.parse_args(argv)
    install_requested = args.install or args.install_only
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    if (args.choose_install_dir or args.install_dir is not None) and not install_requested:
        parser.error("installation destination requires --install or --install-only")
    if args.native and (os.name == "nt" or install_requested or args.install_sdk):
        parser.error("--native is Linux validation only and cannot install")
    if not args.native and os.name != "nt":
        print("Windows-pluginen ma bygges i Windows x64 med MSVC. Bruk --native for Linux-validering.")
        return 2
    if sys.version_info < (3, 10) or sys.maxsize <= 2**32:
        print("Python 3.10 eller nyere, 64-bit, kreves.")
        return 2
    if args.native and (platform.system() != "Linux" or platform.machine().lower() not in ("x86_64", "amd64")):
        print("Native-validering er avgrenset til Linux x86_64.")
        return 2
    LOG_DIR.mkdir(exist_ok=True)
    log = LOG_DIR / ("build-" + dt.datetime.now().strftime("%Y%m%d-%H%M%S-%f") + ".log")
    _log = log.open("w", encoding="utf-8")
    try:
        with local_temporary_directory():
            say("FLYSNAPPER MIDI " + midi_only_evidence.VERSION + " | " + BUILDER_REVISION + "\nLogg: " + str(log))
            if args.install_only:
                return 0 if finish_installation(args.choose_install_dir, args.install_dir) else 1
            run([sys.executable, "-m", "unittest", "discover", "-s", ROOT / "Tests", "-p", "test_builder*.py", "-v"])
            cmake = shutil.which("cmake")
            git = shutil.which("git")
            ctest = shutil.which("ctest")
            if not cmake or not git or not ctest:
                raise RuntimeError("CMake >=3.22, CTest og Git ma finnes i PATH. Installer disse verktoyene og kjor pa nytt.")
            run([cmake, "--version"])
            run([git, "--version"])
            extra = {}
            if args.native:
                juce = verified_juce(git, args.juce_dir)
                build = ROOT / "build/native-release"
                run([cmake, "-S", ROOT, "-B", build, "-DCMAKE_BUILD_TYPE=Release", "-DJUCE_DIR=" + str(juce),
                     "-DFLYSNAPPER_CORE_ONLY=OFF", "-DFLYSNAPPER_BUILD_TESTS=ON"])
                build_env = None
                generator = "native"
                extra["windows_verified"] = False
            else:
                installation, major, full_version = find_visual_studio_details()
                generators = [g["name"] for g in json.loads(run([cmake, "-E", "capabilities"], capture=True))["generators"]]
                tools = prepare_checked_tools(installation, generators, args.install_sdk)
                preflight_toolchain(cmake, tools)
                juce = prepare_unicode_juce(verified_juce(git, args.juce_dir))
                build = configure_checked_project(cmake, juce, tools)
                preflight_juceaide_unicode(build, tools)
                build_env, generator = tools.environment, tools.generator
                extra.update({"visual_studio_build_version": full_version, "windows_sdk_version": tools.sdk.version,
                              "toolchain_probe": "passed", "juceaide_unicode_probe": "passed",
                              "juce_unicode_patch": JUCEAIDE_PATCH_REVISION, "windows_verified": True})
            compiled_inputs = midi_only_evidence.source_hashes(ROOT)
            (build / midi_only_evidence.COMPILED_INPUTS).write_text(
                json.dumps(compiled_inputs, indent=2) + "\n", encoding="utf-8")
            run(build_command(cmake, build, generator, args.jobs), env=build_env)
            if midi_only_evidence.source_hashes(ROOT) != compiled_inputs:
                raise RuntimeError("Kildekoden ble endret under byggingen. Ingen resultatpakke opprettes.")
            run_required_tests(ctest, build, build_env)
            target = collect_result(build, args.native, extra)
            say("BYGG OG AUTOMATISKE TESTER BESTATT: " + str(target))
            say("Ableton Live og fysisk end-to-end-latency er ikke verifisert av byggeskriptet.")
            if install_requested:
                return 0 if finish_installation(args.choose_install_dir, args.install_dir) else 1
            say("Ingen installasjon utført. Windows: bruk --install eller --install-only naar du onsker installasjon.")
            return 0
    except Exception as error:
        say("STOPPET: " + str(error))
        say(traceback.format_exc())
        say("Ingen utestet plugin installeres. Feillogg: " + str(log))
        return 1
    finally:
        _log.close()
        _log = None


if __name__ == "__main__":
    raise SystemExit(main())

