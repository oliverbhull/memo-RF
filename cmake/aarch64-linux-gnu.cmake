# CMake toolchain for cross-compiling to aarch64 Linux (e.g. NVIDIA Jetson Orin Nano).
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=/path/to/cmake/aarch64-linux-gnu.cmake \
#         -DCMAKE_SYSROOT=/path/to/jetson/sysroot \
#         -DWHISPER_DIR=/path/to/whisper.cpp \
#         ..
#
# Set PKG_CONFIG_SYSROOT_DIR and PKG_CONFIG_PATH in the environment (or in the
# build script) so pkg-config finds target libraries in the sysroot.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Cross-compiler. Override with -DCROSS_COMPILE=aarch64-linux-gnu- or set
# AARCH64_CC/AARCH64_CXX when invoking cmake.
if(DEFINED ENV{CROSS_COMPILE})
  set(_cross_prefix "$ENV{CROSS_COMPILE}")
else()
  set(_cross_prefix "aarch64-linux-gnu-")
endif()

if(DEFINED ENV{AARCH64_CC})
  set(CMAKE_C_COMPILER "$ENV{AARCH64_CC}")
else()
  set(CMAKE_C_COMPILER "${_cross_prefix}gcc")
endif()

if(DEFINED ENV{AARCH64_CXX})
  set(CMAKE_CXX_COMPILER "$ENV{AARCH64_CXX}")
else()
  set(CMAKE_CXX_COMPILER "${_cross_prefix}g++")
endif()

# When CMAKE_SYSROOT is set (e.g. -DCMAKE_SYSROOT=... at configure time),
# CMake will search there for libraries and headers. pkg-config must be
# directed to the sysroot via PKG_CONFIG_SYSROOT_DIR and PKG_CONFIG_PATH
# (set by the build script or environment).

# Prefer finding libs/includes in the sysroot only when cross-compiling
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
