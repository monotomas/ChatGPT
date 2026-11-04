#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Run from any directory; retains dependencies, not temporary Xvfb processes.
set -euo pipefail
_flysnapper_project="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
source "${_flysnapper_project}/Tools/cloud_env.sh"
cd "${_flysnapper_project}"
# Ninja dependency logs are writable even for some diagnostic invocations.
# Keep the complete build/test workflow exclusive; manual CMake/Ninja calls
# must use this same lock. Do not wrap this script in a second lock owner.
mkdir -p build/native-release
exec 9>build/native-release/.flysnapper-build.lock
flock -x 9
export DISPLAY=:97
_flysnapper_xvfb_pid=''
cleanup() {
  if [[ -n "${_flysnapper_xvfb_pid}" ]]; then
    kill "${_flysnapper_xvfb_pid}" 2>/dev/null || true
    wait "${_flysnapper_xvfb_pid}" 2>/dev/null || true
  fi
}
trap cleanup EXIT
if [[ ! -S /tmp/.X11-unix/X97 ]]; then
  mkdir -p build
  Xvfb :97 -screen 0 1280x1024x24 -nolisten tcp -ac > build/xvfb.log 2>&1 &
  _flysnapper_xvfb_pid=$!
fi
python - <<'PY'
import socket,time
for attempt in range(50):
    try:
        with socket.socket(socket.AF_UNIX) as connection:
            connection.connect('/tmp/.X11-unix/X97')
        break
    except OSError:
        if attempt==49: raise
        time.sleep(.1)
PY
python build_plugin.py --native --jobs "${CMAKE_BUILD_PARALLEL_LEVEL}" "$@"
