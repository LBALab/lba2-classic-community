# Toolchain for use under Linux when cross-compiling targeting Windows 32-bits using MinGW

set(CMAKE_SYSTEM_NAME Windows)

# Defaults
set(MINGW_COMPILER_PREFIX "i686-w64-mingw32" CACHE STRING "MinGW compiler prefix")
set(MINGW_SYSROOT "/usr/${MINGW_COMPILER_PREFIX}" CACHE STRING "MinGW Sysroot")

# Which compilers to use for C and C++
find_program(CMAKE_RC_COMPILER NAMES ${MINGW_COMPILER_PREFIX}-windres)
find_program(CMAKE_C_COMPILER NAMES ${MINGW_COMPILER_PREFIX}-gcc)
find_program(CMAKE_CXX_COMPILER NAMES ${MINGW_COMPILER_PREFIX}-g++)

# Where is the target environment located
set(CMAKE_FIND_ROOT_PATH ${MINGW_SYSROOT})

# Adjust the default behavior of the FIND_XXX() commands:
## Search programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

## Search headers and libraries in the target environment
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Adjust the default flags
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -static-libgcc")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libgcc -static-libstdc++ -static")
set(CMAKE_SHARED_LIBRARY_LINK_C_FLAGS "${CMAKE_SHARED_LIBRARY_LINK_C_FLAGS} -static-libgcc -static")
set(CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS "${CMAKE_SHARED_LIBRARY_LINK_CXX_FLAGS} -static-libgcc -static-libstdc++ -static")
