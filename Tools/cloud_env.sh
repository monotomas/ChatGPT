#!/usr/bin/env bash
# SPDX-License-Identifier: AGPL-3.0-or-later
# Source this file in each fresh shell. It does not start services.
_flysnapper_env_sysroot="${FLYSNAPPER_SYSROOT:-/workspace/sysroot}"
_flysnapper_env_tools="${FLYSNAPPER_TOOLS:-/workspace/.flysnapper-tools}"
export PATH="${_flysnapper_env_tools}/bin:${_flysnapper_env_sysroot}/usr/bin:${PATH}"
export CPLUS_INCLUDE_PATH="${_flysnapper_env_sysroot}/usr/include:${_flysnapper_env_sysroot}/usr/include/freetype2${CPLUS_INCLUDE_PATH:+:${CPLUS_INCLUDE_PATH}}"
export C_INCLUDE_PATH="${_flysnapper_env_sysroot}/usr/include:${_flysnapper_env_sysroot}/usr/include/freetype2${C_INCLUDE_PATH:+:${C_INCLUDE_PATH}}"
export LIBRARY_PATH="${_flysnapper_env_sysroot}/usr/lib/x86_64-linux-gnu${LIBRARY_PATH:+:${LIBRARY_PATH}}"
export PKG_CONFIG_PATH="${_flysnapper_env_sysroot}/usr/lib/x86_64-linux-gnu/pkgconfig:${_flysnapper_env_sysroot}/usr/share/pkgconfig${PKG_CONFIG_PATH:+:${PKG_CONFIG_PATH}}"
export PKG_CONFIG_SYSROOT_DIR="${_flysnapper_env_sysroot}"
export CMAKE_BUILD_PARALLEL_LEVEL="${CMAKE_BUILD_PARALLEL_LEVEL:-3}"
unset _flysnapper_env_sysroot _flysnapper_env_tools
