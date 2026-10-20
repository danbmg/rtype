# engine-network: UDP socket on its own thread, binary serializer, thread-safe queue.
# Asio is linked PRIVATE: it never leaks into the headers seen by the game, client or server.
if(NOT TARGET asio)
  message(STATUS "engine-network skipped: the asio target comes from the root CMakeLists.txt")
  return()
endif()

add_library(engine-network STATIC UdpSocket.cpp)
add_library(engine::network ALIAS engine-network)
target_include_directories(engine-network PUBLIC "$<BUILD_INTERFACE:${ENGINE_INCLUDE_ROOT}>")
target_compile_features(engine-network PUBLIC cxx_std_20)
target_link_libraries(engine-network PUBLIC engine-core PRIVATE asio)
if(COMMAND rtype_set_warnings)
  rtype_set_warnings(engine-network)
endif()
