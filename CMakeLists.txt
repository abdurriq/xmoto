cmake_minimum_required(VERSION 3.12 FATAL_ERROR)
project(xmoto C CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/CMake")

option(EMSCRIPTEN_BUILD "Build for the browser via Emscripten" OFF)

set(APP_NAME "X-Moto")
set(APP_COPYRIGHT_YEAR "2025")

set(APP_VERSION_MAJOR 0)
set(APP_VERSION_MINOR 6)
set(APP_VERSION_PATCH 3)

set(APP_VERSION_STRING "\
${APP_VERSION_MAJOR}.\
${APP_VERSION_MINOR}.\
${APP_VERSION_PATCH}")

include(CheckCXXCompilerFlag)
include(CheckPrototypeDefinition)
include(CheckSymbolExists)
if(NOT EMSCRIPTEN_BUILD)
  include(TestBigEndian)
endif()

include(CMake/mxe.cmake)

add_subdirectory(src)

# we set some cpack variables in src/CMakeLists.txt so include this after src/
if(NOT EMSCRIPTEN_BUILD)
  include(cpack/cpack.cmake)
  include(CPack)
endif()

if(NOT EMSCRIPTEN_BUILD)
  add_subdirectory(bin)
  add_subdirectory(doc)
  add_subdirectory(extra)
endif()
