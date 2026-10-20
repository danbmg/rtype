if(NOT TARGET rtype::client-net)
  message(FATAL_ERROR "The graphical client needs Dan's client/net and common/protocol modules.")
endif()
add_library(rtype-client-options STATIC Options.cpp)
target_include_directories(rtype-client-options PUBLIC ${PROJECT_SOURCE_DIR})
rtype_set_warnings(rtype-client-options)

add_library(rtype-client-scene STATIC Scene.cpp)
target_link_libraries(rtype-client-scene PUBLIC rtype::client-net)
rtype_set_warnings(rtype-client-scene)

add_executable(r-type_client main.cpp)
target_link_libraries(r-type_client PRIVATE rtype-client-options rtype-client-scene
  engine::render engine::input engine::audio)
rtype_set_warnings(r-type_client)
rtype_output_to_root(r-type_client)
