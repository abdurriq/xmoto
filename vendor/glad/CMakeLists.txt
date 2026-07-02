cmake_minimum_required(VERSION 3.7 FATAL_ERROR)
project(glad C)

if(EMSCRIPTEN_BUILD)
  # Emscripten provides WebGL2 symbols natively; glad is not needed.
  add_library(glad STATIC src/glad_web.c)
else()
  add_library(glad STATIC src/glad.c)
endif()

target_include_directories(glad
  PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)

