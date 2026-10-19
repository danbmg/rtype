set(CPM_DOWNLOAD_VERSION 0.42.3)

if(CPM_SOURCE_CACHE)
  set(CPM_DIR "${CPM_SOURCE_CACHE}/cpm")
else()
  set(CPM_DIR "${CMAKE_BINARY_DIR}/_cpm")
endif()

file(MAKE_DIRECTORY "${CPM_DIR}")
set(CPM_CMAKE "${CPM_DIR}/CPM.cmake")

if(NOT EXISTS "${CPM_CMAKE}")
  file(DOWNLOAD
       "https://github.com/cpm-cmake/CPM.cmake/releases/download/v${CPM_DOWNLOAD_VERSION}/CPM.cmake"
       "${CPM_CMAKE}"
       TIMEOUT 60
       STATUS _cpm_status)
  list(GET _cpm_status 0 _cpm_exit)
  if(NOT _cpm_exit EQUAL 0)
    message(FATAL_ERROR "Failed to download CPM.cmake: ${_cpm_status}")
  endif()
endif()

include("${CPM_CMAKE}")
