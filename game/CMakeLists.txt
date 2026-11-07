add_library(rtype-game STATIC
  GameSystems.cpp
  EquipmentSystems.cpp
  EnemyBehaviors.cpp
  Level.cpp
  LevelDirector.cpp
  Content.cpp)
add_library(rtype::game ALIAS rtype-game)
target_include_directories(rtype-game PUBLIC ${PROJECT_SOURCE_DIR})
target_link_libraries(rtype-game PUBLIC engine::core engine::ecs rtype::components
                                 PRIVATE nlohmann_json::nlohmann_json)
rtype_set_warnings(rtype-game)
