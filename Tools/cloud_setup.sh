#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Unprivileged Debian 13 x86_64 build prerequisites. No application/OS sources changed.
set -euo pipefail
case "${1:-}" in
  ""|--refresh) ;;
  *) printf 'Usage: bash Tools/cloud_setup.sh [--refresh]\n' >&2; exit 2 ;;
esac
if [[ "$(dpkg --print-architecture)" != amd64 ]] || ! python - <<'PY'
from pathlib import Path
values=dict(line.split('=',1) for line in Path('/etc/os-release').read_text().splitlines() if '=' in line)
raise SystemExit(0 if values.get('ID','').strip('"')=='debian' and values.get('VERSION_CODENAME','').strip('"')=='trixie' else 1)
PY
then
  printf 'This cloud setup requires Debian 13 (trixie), amd64. Use the Windows builder on Windows.\n' >&2
  exit 2
fi
_flysnapper_here="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
_flysnapper_tools="${FLYSNAPPER_TOOLS:-/workspace/.flysnapper-tools}"
_flysnapper_sysroot="${FLYSNAPPER_SYSROOT:-/workspace/sysroot}"
_flysnapper_apt="${FLYSNAPPER_APT_CACHE:-/workspace/apt}"
for _flysnapper_path in "${_flysnapper_tools}" "${_flysnapper_sysroot}" "${_flysnapper_apt}"; do
  [[ "${_flysnapper_path}" == /* && "${_flysnapper_path}" != / ]] || { printf 'Setup paths must be absolute directories.\n' >&2; exit 2; }
done
python -m venv "${_flysnapper_tools}"
"${_flysnapper_tools}/bin/python" -m pip install --disable-pip-version-check --no-cache-dir cmake==4.4.4 ninja==1.13.2
mkdir -p "${_flysnapper_apt}/state/lists/partial" "${_flysnapper_apt}/cache/archives/partial" "${_flysnapper_apt}/log" "${_flysnapper_sysroot}"
# APT sees the real installed runtime libraries, but writes only local metadata.
cp /var/lib/dpkg/status "${_flysnapper_apt}/state/status"
_flysnapper_source='deb [signed-by=/usr/share/keyrings/debian-archive-keyring.gpg] https://deb.debian.org/debian trixie main'
if [[ -f "${_flysnapper_apt}/sources.list" ]] && [[ "$(cat "${_flysnapper_apt}/sources.list")" != "${_flysnapper_source}" ]]; then
  printf 'APT cache has a different source configuration; refusing to overwrite it.\n' >&2
  exit 1
fi
printf '%s\n' "${_flysnapper_source}" > "${_flysnapper_apt}/sources.list"
_flysnapper_apt_options=(
  -o Dir::Etc::parts=-
  -o "Dir::Etc::sourcelist=${_flysnapper_apt}/sources.list"
  -o Dir::Etc::sourceparts=-
  -o "Dir::State=${_flysnapper_apt}/state"
  -o "Dir::State::status=${_flysnapper_apt}/state/status"
  -o "Dir::Cache=${_flysnapper_apt}/cache"
  -o "Dir::Log=${_flysnapper_apt}/log"
  -o Debug::NoLocking=1
  -o "APT::Sandbox::User=$(id -un)"
)
# Respect the managed HTTPS proxy and the Debian archive keyring. Do not relax
# TLS validation, Debian signatures, package hashes or Valid-Until checks.
if [[ -n "${HTTPS_PROXY:-}" ]]; then
  _flysnapper_apt_options+=(-o "Acquire::https::Proxy=${HTTPS_PROXY}")
fi
if [[ "${1:-}" == --refresh ]] || ! compgen -G "${_flysnapper_apt}/state/lists/*_InRelease" >/dev/null; then
  apt-get "${_flysnapper_apt_options[@]}" update
fi
apt-get "${_flysnapper_apt_options[@]}" --download-only -y install \
  libasound2-dev libx11-dev libxext-dev libxinerama-dev libxrandr-dev libxcursor-dev \
  libfreetype6-dev libfontconfig1-dev libgl1-mesa-dev xvfb
# Verify every cached archive against the signed APT package index, including
# archives reused from earlier runs. Refuse unrecognised cache contents.
python - "${_flysnapper_apt}" "${_flysnapper_sysroot}" <<'PY'
from pathlib import Path
import hashlib,subprocess,sys
apt,sysroot=map(Path,sys.argv[1:])
expected=set()
for index in (apt/'state/lists').glob('*_Packages*'):
    reader=subprocess.Popen(['/usr/lib/apt/apt-helper','cat-file',str(index)],stdout=subprocess.PIPE,text=True)
    for line in reader.stdout:
        if line.startswith('SHA256: '): expected.add(line.split()[1])
    if reader.wait()!=0: raise SystemExit('Cannot read APT package index')
archives=sorted((apt/'cache/archives').glob('*.deb'))
if not archives or not expected: raise SystemExit('Missing signed package metadata or archives')
for archive in archives:
    if hashlib.sha256(archive.read_bytes()).hexdigest() not in expected:
        raise SystemExit('Archive does not match signed APT metadata: '+archive.name)
for archive in archives:
    subprocess.run(['dpkg-deb','-x',str(archive),str(sysroot)],check=True)
PY
# Development packages often point to a runtime .so already installed by the
# base image. Repair only dangling development symlinks to a real matching
# system runtime. No system files or libraries are copied/replaced.
python - "${_flysnapper_sysroot}" <<'PY'
from pathlib import Path
import os,sys
folder=Path(sys.argv[1])/'usr/lib/x86_64-linux-gnu'
for link in folder.glob('*.so'):
    if link.is_symlink() and not link.exists():
        basename=Path(os.readlink(link)).name
        runtime=Path('/usr/lib/x86_64-linux-gnu')/basename
        if runtime.is_file():
            link.unlink()
            link.symlink_to(runtime)
PY
export FLYSNAPPER_SYSROOT="${_flysnapper_sysroot}" FLYSNAPPER_TOOLS="${_flysnapper_tools}"
source "${_flysnapper_here}/cloud_env.sh"
cmake --version
ninja --version
pkg-config --exists alsa x11 xext xinerama xrandr xcursor freetype2 fontconfig gl bzip2
[[ -x "${_flysnapper_sysroot}/usr/bin/Xvfb" ]]
printf 'Cloud prerequisites ready. Source Tools/cloud_env.sh in each new shell.\n'
printf 'GUI tests additionally need Xvfb; startup instructions are in the environment configuration.\n'
