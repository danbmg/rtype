add_executable(game_tests GameplayTests.cpp LevelTests.cpp EquipmentTests.cpp ContentTests.cpp
                          BehaviorTests.cpp)
target_link_libraries(game_tests PRIVATE rtype::game Catch2::Catch2WithMain)
# The shipped content files are tested as they are: a broken level fails the build's tests.
target_compile_definitions(game_tests PRIVATE RTYPE_CONTENT_DIR="${PROJECT_SOURCE_DIR}/assets/levels")
rtype_set_warnings(game_tests)
catch_discover_tests(game_tests)
