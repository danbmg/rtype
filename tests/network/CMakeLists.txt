add_executable(network_tests SerializerTests.cpp PacketCodecTests.cpp ThreadSafeQueueTests.cpp
                             UdpSocketTests.cpp)
target_link_libraries(network_tests PRIVATE rtype::protocol engine::network Catch2::Catch2WithMain)
rtype_set_warnings(network_tests)
catch_discover_tests(network_tests)
