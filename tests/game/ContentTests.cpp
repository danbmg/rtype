#include <filesystem>
#include <fstream>
#include <map>
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include "game/Content.hpp"

namespace {

using Catch::Matchers::ContainsSubstring;
namespace fs = std::filesystem;

constexpr const char* ENEMIES = R"({
  "enemies": [
    { "name": "drone", "visual": "bydos", "health": 1 },
    { "name": "waver", "visual": "waver", "movement": "sine", "amplitude": 50 }
  ],
  "bosses": [
    { "name": "big", "visual": "final-boss", "health": 40, "trophy": true,
      "phases": [ { "below": 1.0, "attacks": ["aimed"] },
                  { "below": 0.5, "attacks": ["summon", "ring"], "summon": "drone" } ] }
  ]
})";
constexpr const char* STORY = R"({ "name": "Test", "levels": ["one.json", "two.json"] })";
constexpr const char* LEVEL_ONE = R"(// comments are allowed
{ "title": "One", "story": "Hello\nWorld", "theme": 3, "intro": 2,
  "waves": [ { "enemy": "drone", "count": 3, "interval": 0.5, "formation": "v" },
             { "enemy": "waver", "count": 2, "drop": 1.0 } ] })";
constexpr const char* LEVEL_TWO = R"({ "title": "Two", "waves": [ { "boss": "big" } ] })";
constexpr const char* ENDLESS =
    R"({ "enemies": ["drone", "waver"], "bosses": ["big"], "bossEvery": 2 })";

// A content directory in the temporary folder, with the given files replaced.
class Sandbox {
public:
  explicit Sandbox(const std::map<std::string, std::string>& overrides = {}) {
    static int counter = 0;
    _path = fs::temp_directory_path() / ("rtype-content-test-" + std::to_string(++counter));
    fs::remove_all(_path);
    fs::create_directories(_path);
    std::map<std::string, std::string> files{{"enemies.json", ENEMIES},
                                             {"story.json", STORY},
                                             {"one.json", LEVEL_ONE},
                                             {"two.json", LEVEL_TWO},
                                             {"endless.json", ENDLESS}};
    for (const auto& [name, text] : overrides) {
      files[name] = text;
    }
    for (const auto& [name, text] : files) {
      std::ofstream(_path / name) << text;
    }
  }
  ~Sandbox() {
    std::error_code ignored;
    fs::remove_all(_path, ignored);
  }
  Sandbox(const Sandbox&) = delete;
  Sandbox& operator=(const Sandbox&) = delete;
  Sandbox(Sandbox&&) = delete;
  Sandbox& operator=(Sandbox&&) = delete;

  const fs::path& path() const { return _path; }

private:
  fs::path _path;
};

std::string errorFor(const std::map<std::string, std::string>& overrides) {
  const Sandbox sandbox(overrides);
  try {
    game::loadContent(sandbox.path());
  } catch (const game::ContentError& error) {
    return error.what();
  }
  return "no error";
}

} // namespace

TEST_CASE("The shipped story loads: ten levels, bosses at levels 5 and 10", "[game][content]") {
  const auto content = game::loadContent(RTYPE_CONTENT_DIR);
  REQUIRE(content.story.finalLevel() == 10U);
  CHECK(content.campaignName == "Save the Summoner's Cup");
  for (std::uint32_t number = 1; number <= 10; ++number) {
    const auto level = content.story.get(number);
    CHECK_FALSE(level.title.empty());
    CHECK(level.title.size() <= game::MAX_LEVEL_TITLE);
    CHECK(level.story.size() <= game::MAX_LEVEL_STORY);
    const bool hasBoss = level.waves.back().boss;
    CHECK(hasBoss == (number == 5 || number == 10));
  }
  CHECK(content.story.get(5).waves.back().bossType == "gatekeeper");
  CHECK(content.story.get(10).waves.back().bossType == "cup-thief");
  const auto thief = content.findBoss("cup-thief");
  REQUIRE(thief);
  CHECK(content.boss(*thief).trophy);
  CHECK(content.boss(*thief).phases.size() == 3);
  CHECK(content.endless.generator().enemies.size() >= 4);
  CHECK_FALSE(content.endless.finalLevel());
}

TEST_CASE("Content files become levels, enemies and bosses", "[game][content]") {
  const Sandbox sandbox;
  const auto content = game::loadContent(sandbox.path());
  REQUIRE(content.enemies.size() == 2);
  const auto* waver = content.findEnemy("waver");
  REQUIRE(waver != nullptr);
  CHECK(waver->movement == rtype::common::components::MovementPattern::Sine);
  CHECK(waver->amplitude == 50.0F);
  CHECK(content.findEnemy("nobody") == nullptr);

  const auto one = content.story.get(1);
  CHECK(one.title == "One");
  CHECK(one.story == "Hello\nWorld");
  CHECK(one.theme == 3);
  CHECK(one.introSeconds == 2.0F);
  REQUIRE(one.waves.size() == 2);
  CHECK(one.waves[0].enemy == "drone");
  CHECK(one.waves[0].enemyCount == 3);
  CHECK(one.waves[0].formation == game::Formation::V);
  CHECK(one.waves[1].pickupDropChance == 1.0F);

  const auto two = content.story.get(2);
  REQUIRE(two.waves.size() == 1);
  CHECK(two.waves[0].boss);
  CHECK(two.waves[0].bossType == "big");
  CHECK(content.story.finalLevel() == 2U);

  // Endless: named bosses every second level, generic ones in between.
  CHECK(content.endless.get(1).waves.back().bossType.empty());
  CHECK(content.endless.get(2).waves.back().bossType == "big");
  CHECK_FALSE(content.endless.get(1).waves.front().enemy.empty());
}

TEST_CASE("Content mistakes are reported with the file and the field",
          "[game][content][malformed]") {
  CHECK_THAT(errorFor({{"one.json", R"({ "title": "x", "sped": 3, "waves": [] })"}}),
             ContainsSubstring("one.json") && ContainsSubstring("unknown field \"sped\""));
  CHECK_THAT(errorFor({{"one.json", R"({ "title": "x", "waves": [ { "enemy": "ghost" } ] })"}}),
             ContainsSubstring("ghost"));
  CHECK_THAT(errorFor({{"one.json", R"({ "title": "x", "waves": [ { "count": 3 } ] })"}}),
             ContainsSubstring("either \"enemy\" or \"boss\""));
  CHECK_THAT(errorFor({{"one.json", R"({ "waves": [ { "enemy": "drone" } ] })"}}),
             ContainsSubstring("\"title\" is required"));
  CHECK_THAT(errorFor({{"one.json", "{ \"title\": \"x\", "}}), ContainsSubstring("invalid JSON"));
  CHECK_THAT(errorFor({{"one.json",
                        R"({ "title": "x", "waves": [ { "enemy": "drone", "count": 500 } ] })"}}),
             ContainsSubstring("\"count\" must be a whole number between 1 and 50"));
  CHECK_THAT(errorFor({{"one.json", R"({ "title": "café", "waves": [ { "enemy": "drone" } ] })"}}),
             ContainsSubstring("plain ASCII"));
  CHECK_THAT(errorFor({{"one.json", "{ \"title\": \"" + std::string(60, 'x') +
                                        "\", \"waves\": [ { \"enemy\": \"drone\" } ] }"}}),
             ContainsSubstring("too long"));
  CHECK_THAT(errorFor({{"two.json",
                        R"({ "title": "x", "waves": [ { "boss": "big", "count": 2 } ] })"}}),
             ContainsSubstring("does not apply to a boss wave"));
  CHECK_THAT(errorFor({{"story.json", R"({ "name": "x", "levels": ["../../etc/passwd"] })"}}),
             ContainsSubstring("inside the content directory"));
  CHECK_THAT(errorFor({{"story.json", R"({ "name": "x", "levels": ["missing.json"] })"}}),
             ContainsSubstring("missing.json"));
  CHECK_THAT(errorFor({{"enemies.json",
                        R"({ "enemies": [ { "name": "Drone" } ], "bosses": [] })"}}),
             ContainsSubstring("lowercase"));
  CHECK_THAT(errorFor({{"enemies.json",
                        R"({ "enemies": [ { "name": "a", "visual": "dragon" } ],
                             "bosses": [] })"}}),
             ContainsSubstring("\"visual\" must be one of"));
  CHECK_THAT(errorFor({{"enemies.json",
                        R"({ "enemies": [ { "name": "a" }, { "name": "a" } ],
                             "bosses": [] })"}}),
             ContainsSubstring("two enemies"));
  CHECK_THAT(errorFor({{"enemies.json", R"({ "enemies": [ { "name": "a" } ], "bosses": [
               { "name": "b", "phases": [ { "below": 0.5, "attacks": ["aimed"] } ] } ] })"}}),
             ContainsSubstring("must start with a phase"));
  CHECK_THAT(errorFor({{"enemies.json", R"({ "enemies": [ { "name": "a" } ], "bosses": [
               { "name": "b", "phases": [ { "attacks": ["summon"] } ] } ] })"}}),
             ContainsSubstring("summon"));
  CHECK_THAT(errorFor({{"endless.json", R"({ "enemies": ["drone", "nope"] })"}}),
             ContainsSubstring("nope"));
  CHECK_THROWS_AS(game::loadContent("/definitely/not/here"), game::ContentError);
}

TEST_CASE("Difficulty scales enemies and lives", "[game][content]") {
  const auto easy = game::difficultySettings(game::Difficulty::Easy);
  const auto normal = game::difficultySettings(game::Difficulty::Normal);
  const auto hard = game::difficultySettings(game::Difficulty::Hard);
  CHECK(easy.playerLives > normal.playerLives);
  CHECK(hard.playerLives < normal.playerLives);
  CHECK(easy.enemyHealth < normal.enemyHealth);
  CHECK(hard.enemyHealth > normal.enemyHealth);
  CHECK(easy.enemyFireRate < hard.enemyFireRate);
  CHECK(normal.enemyHealth == 1.0F);
}
