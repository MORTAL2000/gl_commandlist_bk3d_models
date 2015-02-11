cmake_minimum_required(VERSION 2.8)
set(PROJNAME gl_commandlist_bk3d_models)
Project(${PROJNAME})
Message(STATUS "-------------------------------")
Message(STATUS "Processing Project ${PROJNAME}:")

SET(MODELS_DOWNLOAD_FORCE OFF CACHE BOOL "Forces the download operation of 3D models needed for this demo" )
SET(MODELS_DOWNLOAD_DISABLED ON CACHE BOOL "UNCHECK IT to download heavy 3D models needed for this demo" )
#####################################################################################
# offer the choice of having shared_sources as a sub-folder... good for packaging a sample
#
if(NOT BASE_DIRECTORY) # if not defined, it means this cmake file was called as the first entry point and not included
  # check if the external repository is outside or inside the project (as a sub-module)
  # testing the file CMakeLists_include.txt because when sub-modules are not cloned, the folders are still there...
  # we also assume here that if shared_sources is there, shared_external is, too...
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/shared_sources/CMakeLists_include.txt)
    Message(STATUS "found sub-folder shared_sources")
    SET(BASE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/shared_sources/CMakeLists_include.txt)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/shared_sources ${CMAKE_BINARY_DIR}/shared_sources)
    # if the resource of downloaded meshes is local, keep this one. Otherwise, use the common one
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/downloaded_resources)
      SET(DOWNLOAD_TARGET_DIR "${BASE_DIRECTORY}/downloaded_resources")
    else()
      SET(DOWNLOAD_TARGET_DIR "${BASE_DIRECTORY}/../downloaded_resources")
    endif()
  else()
    SET(BASE_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..)
    INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../shared_sources/CMakeLists_include.txt)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../shared_sources ${CMAKE_BINARY_DIR}/shared_sources)
    SET(DOWNLOAD_TARGET_DIR "${BASE_DIRECTORY}/downloaded_resources")
  endif()
else(NOT BASE_DIRECTORY)
    SET(DOWNLOAD_TARGET_DIR "${BASE_DIRECTORY}/downloaded_resources")
endif(NOT BASE_DIRECTORY)
_add_project_definitions(${PROJNAME})

#####################################################################################
# additions from packages needed for this sample
# add refs  in LIBRARIES_OPTIMIZED
# add refs  in LIBRARIES_DEBUG
# add files in PACKAGE_SOURCE_FILES
#
#_add_package_NSight()
_add_package_MFCUI()
#_add_package_AntTweakBar()
_add_package_OpenGLText()
_add_package_ZLIB()
#_add_package_nvFx()

#####################################################################################
# Source files for this project
#
file(GLOB SOURCE_FILES *.cpp *.hpp *.inl *.h *.c)

#####################################################################################
# download model for this demo to run. Avoids using GitHub for this
# TODO: we need to put the models online. Should happen next week (>Dec.22)
#
if(NOT ${MODELS_DOWNLOAD_DISABLED})
  set(FILELIST 
  "Jet_134.bk3d.gz"
  "Driveline_v134.bk3d.gz"
  "Body_v134.bk3d.gz"
  "Camera_134.bk3d.gz"
  "ConceptCar_134.bk3d.gz"
  "Eiffel_133.bk3d.gz"
  "Smobby_134.bk3d.gz"
  "SubMarine_134.bk3d.gz"
  )
  _download_files(
    "${FILELIST}"
    "http://developer.download.nvidia.com/ProGraphics/nvpro-samples"
    ${DOWNLOAD_TARGET_DIR}
    ${MODELS_DOWNLOAD_FORCE}
  )
endif()
#####################################################################################
# additional files from helpers
#
#LIST(APPEND COMMON_SOURCE_FILES 
#  ${BASE_DIRECTORY}/shared_sources/nv_helpers_gl/WindowInertiaCamera.h
#  ${BASE_DIRECTORY}/shared_sources/nv_helpers/TimeSampler.h
#  ${BASE_DIRECTORY}/shared_sources/nv_helpers/InertiaCamera.h
#  ${BASE_DIRECTORY}/shared_sources/nv_math/nv_math.inl
#  ${BASE_DIRECTORY}/shared_sources/nv_math/nv_math.h
#  ${BASE_DIRECTORY}/shared_sources/nv_math/nv_math_types.h
#  ${BASE_DIRECTORY}/shared_sources/nv_helpers_gl/profiler.hpp
#  ${BASE_DIRECTORY}/shared_sources/nv_helpers_gl/profiler.cpp
#)
#####################################################################################
# Executable
#
if(WIN32 AND NOT GLUT_FOUND)
  add_definitions(/wd4267) #remove size_t to int warning
  add_definitions(/wd4996) #remove printf warning
  add_definitions(/wd4244) #remove double to float conversion warning
  add_definitions(/wd4305) #remove double to float truncation warning
  add_executable(${PROJNAME} WIN32 ${SOURCE_FILES} ${COMMON_SOURCE_FILES} ${PACKAGE_SOURCE_FILES})
else()
  # allow gcc to be tolerant on some issues. TODO:should remove this option
  add_definitions(-fpermissive)
  add_executable(${PROJNAME} ${SOURCE_FILES} ${COMMON_SOURCE_FILES} ${PACKAGE_SOURCE_FILES})
endif()

#####################################################################################
# common source code needed for this sample
#
source_group(common FILES 
  ${COMMON_SOURCE_FILES}
)
#####################################################################################
# Linkage
#
target_link_libraries(${PROJNAME} optimized
    ${LIBRARIES_OPTIMIZED}
    ${PLATFORM_LIBRARIES}
    shared_sources
)
target_link_libraries(${PROJNAME} debug
    ${LIBRARIES_DEBUG}
    ${PLATFORM_LIBRARIES}
    shared_sources
)

#####################################################################################
# copies binaries that need to be put next to the exe files (ZLib, etc.)
#
_copy_binaries_to_target( ${PROJNAME} )
