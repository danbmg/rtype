add_executable(engine_tests CoreTests.cpp RegistryTests.cpp)
target_link_libraries(engine_tests PRIVATE engine::engine Catch2::Catch2WithMain)
rtype_set_warnings(engine_tests)
catch_discover_tests(engine_tests)
