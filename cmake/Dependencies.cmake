include("${CMAKE_CURRENT_LIST_DIR}/CPM.cmake")

find_package(Threads REQUIRED)

CPMAddPackage(
  NAME rtype_engine
  SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../engine"
)

if(RTYPE_BUILD_TESTS)
  CPMAddPackage("gh:catchorg/Catch2@3.8.1")
endif()
