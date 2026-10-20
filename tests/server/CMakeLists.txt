add_executable(server_tests SessionManagerTests.cpp GameWorldTests.cpp ServerIntegrationTests.cpp)
target_link_libraries(server_tests PRIVATE rtype::server rtype::client-net Catch2::Catch2WithMain)
rtype_set_warnings(server_tests)
catch_discover_tests(server_tests)
