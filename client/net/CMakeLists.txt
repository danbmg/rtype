# rtype-client-net: what the graphical client links to talk to the server (no SFML here).
# client/CMakeLists.txt links it with: target_link_libraries(r-type_client PRIVATE rtype::client-net)
add_library(rtype-client-net STATIC NetworkClient.cpp)
add_library(rtype::client-net ALIAS rtype-client-net)
target_include_directories(rtype-client-net PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(rtype-client-net PUBLIC rtype::protocol engine::network)
rtype_set_warnings(rtype-client-net)

# Headless test client: bots that connect, move and shoot, to load the server without a window.
add_executable(r-type_bot bot/main.cpp)
target_link_libraries(r-type_bot PRIVATE rtype::client-net)
set_target_properties(r-type_bot PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
rtype_set_warnings(r-type_bot)
