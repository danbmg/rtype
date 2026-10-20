# The server never links SFML: it only depends on the gameplay, the protocol and the network.
add_library(rtype-server STATIC Server.cpp SessionManager.cpp GameWorld.cpp)
add_library(rtype::server ALIAS rtype-server)
target_include_directories(rtype-server PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(rtype-server PUBLIC rtype::game rtype::protocol engine::network)
rtype_set_warnings(rtype-server)

add_executable(r-type_server main.cpp)
target_link_libraries(r-type_server PRIVATE rtype::server)
rtype_set_warnings(r-type_server)
rtype_output_to_root(r-type_server)
