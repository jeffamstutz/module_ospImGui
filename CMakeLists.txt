## ======================================================================== ##
## Copyright 2009-2017 Intel Corporation                                    ##
##                                                                          ##
## Licensed under the Apache License, Version 2.0 (the "License");          ##
## you may not use this file except in compliance with the License.         ##
## You may obtain a copy of the License at                                  ##
##                                                                          ##
##     http://www.apache.org/licenses/LICENSE-2.0                           ##
##                                                                          ##
## Unless required by applicable law or agreed to in writing, software      ##
## distributed under the License is distributed on an "AS IS" BASIS,        ##
## WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. ##
## See the License for the specific language governing permissions and      ##
## limitations under the License.                                           ##
## ======================================================================== ##

cmake_minimum_required(VERSION 2.8)

project(ospImGui)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

# force 'Release' build type on initial configuration (otherwise is empty)
set(CONFIGURATION_TYPES "Debug" "Release" "RelWithDebInfo")
if (WIN32)
  if (NOT DEFAULT_CMAKE_CONFIGURATION_TYPES_SET)
    set(CMAKE_CONFIGURATION_TYPES "${CONFIGURATION_TYPES}" CACHE STRING
        "List of generated configurations." FORCE)
    set(DEFAULT_CMAKE_CONFIGURATION_TYPES_SET ON CACHE INTERNAL
        "Default CMake configuration types set.")
  endif()
else()
  if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS ${CONFIGURATION_TYPES})
  endif()
endif()

# unhide compiler to make it easier for users to see what they are using
mark_as_advanced(CLEAR CMAKE_CXX_COMPILER)

##############################################################
# Compiler configuration
##############################################################

# compiler selection for the non-MIC part
if (${CMAKE_CXX_COMPILER_ID} STREQUAL "Intel")
  include(${PROJECT_SOURCE_DIR}/cmake/icc.cmake)
elseif (${CMAKE_CXX_COMPILER_ID} STREQUAL "GNU")
  include(${PROJECT_SOURCE_DIR}/cmake/gcc.cmake)
elseif (${CMAKE_CXX_COMPILER_ID} STREQUAL "Clang")
  include(${PROJECT_SOURCE_DIR}/cmake/clang.cmake)
elseif (${CMAKE_CXX_COMPILER_ID} STREQUAL "MSVC")
  include(${PROJECT_SOURCE_DIR}/cmake/msvc.cmake)
else()
  message(FATAL_ERROR "Unsupported compiler: '${OSPRAY_COMPILER}'")
endif()

##############################################################
# Project configuration
##############################################################

set(LIBRARY_OUTPUT_PATH    ${CMAKE_BINARY_DIR})
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

find_package(OpenGL REQUIRED)

##############################################################
# Build support libs
##############################################################

add_subdirectory(common)

##############################################################
# Build app
##############################################################

set(APP_NAME ospImGui)

#find_package(ospray REQUIRED)
#include(${OSPRAY_USE_FILE})

include_directories(
  ${CMAKE_SOURCE_DIR}
  common/gl3w
  common/imgui
  ${OPENGL_INCLUDE_DIR}
  #${OSPRAY_INCLUDE_DIRS}
)

add_executable(${APP_NAME}
  imgui_impl_glfw_gl3.cpp
  main.cpp
)

target_link_libraries(${APP_NAME}
  ${OPENGL_LIBRARIES}
  glfw
  gl3w
  imgui
  #${OSPRAY_LIBRARIES}
)
