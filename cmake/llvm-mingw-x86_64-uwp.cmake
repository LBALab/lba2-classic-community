# Experimental: cross-compile for Windows UWP (Xbox Dev Mode) with llvm-mingw.
# The *-w64-mingw32uwp-* wrappers set WINAPI_FAMILY=WINAPI_FAMILY_APP and link
# against the UWP CRT (ucrtapp) and windowsapp instead of desktop kernel32.
# Point LLVM_MINGW_ROOT at an unpacked llvm-mingw release.

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(LLVM_MINGW_ROOT "$ENV{LLVM_MINGW_ROOT}" CACHE PATH "llvm-mingw install root")
set(LLVM_MINGW_TRIPLE x86_64-w64-mingw32uwp CACHE STRING "llvm-mingw target prefix (x86_64-w64-mingw32 for a desktop control build)")
set(_triple ${LLVM_MINGW_TRIPLE})

set(CMAKE_C_COMPILER   "${LLVM_MINGW_ROOT}/bin/${_triple}-clang")
set(CMAKE_CXX_COMPILER "${LLVM_MINGW_ROOT}/bin/${_triple}-clang++")
set(CMAKE_RC_COMPILER  "${LLVM_MINGW_ROOT}/bin/${_triple}-windres")

set(CMAKE_FIND_ROOT_PATH "${LLVM_MINGW_ROOT}/x86_64-w64-mingw32")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
