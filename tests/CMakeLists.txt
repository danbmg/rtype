include(Catch)

# One directory per tested module, owned by that module's owner (tests/<module>/CMakeLists.txt).
foreach(module engine game network server client)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${module}/CMakeLists.txt")
    add_subdirectory(${module})
  endif()
endforeach()
