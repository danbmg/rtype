add_executable(game_tests GameplayTests.cpp)
target_link_libraries(game_tests PRIVATE rtype::game Catch2::Catch2WithMain)
rtype_set_warnings(game_tests)
catch_discover_tests(game_tests)
