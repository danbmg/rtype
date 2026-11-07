#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include "common/components/Behavior.hpp"
#include "game/Level.hpp"

namespace game {

// A kind of regular enemy, as described in enemies.json.
struct EnemyType {
  std::string name;
  rtype::common::components::Visual visual = rtype::common::components::Visual::Bydos;
  int health = 1;
  int points = 100;
  float width = 32.0F;
  float height = 24.0F;
  rtype::common::components::MovementPattern movement =
      rtype::common::components::MovementPattern::Straight;
  float speed = 80.0F;
  float amplitude = 0.0F;
  float frequency = 0.0F;
  float fireRate = 0.0F;
  float shotSpeed = 220.0F;
  std::uint8_t burst = 1;
  float dropChance = 0.15F;
};

// Aimed: `bullets` shots at a player, slightly spread. Spread: a fan of `bullets` shots to the
// left. Ring: `bullets` shots in every direction. Wall: a column of shots with a gap to fly
// through. Summon: two enemies of type `summon` appear beside the boss.
enum class BossAttack : std::uint8_t { Aimed, Spread, Ring, Wall, Summon };

// Active while the boss health ratio is at or below `below` (the first phase uses 1.0).
struct BossPhase {
  float below = 1.0F;
  std::vector<BossAttack> attacks{BossAttack::Aimed};
  float interval = 1.5F; // seconds between attacks
  std::uint8_t bullets = 5;
  float shotSpeed = 240.0F;
  float moveSpeed = 60.0F; // vertical sweep speed
  std::string summon;      // enemy type for Summon attacks
};

struct BossType {
  std::string name;
  rtype::common::components::Visual visual = rtype::common::components::Visual::Boss;
  int health = 60;
  int points = 5000;
  // Clients draw a boss at the size of its sprite: 120 x 96 for "boss", 150 x 120 for
  // "final-boss". Keep the hitbox the same so that what hits is what is seen.
  float width = 120.0F;
  float height = 96.0F;
  bool trophy = false; // drops the Summoner's Cup at the end of a finite campaign
  std::vector<BossPhase> phases{BossPhase{}};
};

enum class Difficulty : std::uint8_t { Easy = 0, Normal = 1, Hard = 2 };

// Multipliers applied when enemies spawn, and the lives each player starts with.
struct DifficultySettings {
  float enemyHealth = 1.0F;
  float enemyFireRate = 1.0F;
  float shotSpeed = 1.0F;
  float enemySpeed = 1.0F;
  float dropChance = 1.0F;
  int playerLives = 3;
};

DifficultySettings difficultySettings(Difficulty difficulty);

// Everything a game designer can change without recompiling. A default-constructed GameContent
// has no named enemy or boss: generated levels then use generic Bydos and bosses.
struct GameContent {
  std::vector<EnemyType> enemies;
  std::vector<BossType> bosses;
  std::string campaignName = "Campaign";
  LevelCatalog story;   // the finite campaign, final level set
  LevelCatalog endless; // generated, never ends

  const EnemyType* findEnemy(std::string_view name) const;
  std::optional<std::uint16_t> findBoss(std::string_view name) const;
  // Index from findBoss(), or anything else for the built-in generic boss.
  const BossType& boss(std::uint16_t index) const;
};

constexpr std::uint16_t GENERIC_BOSS = 0xFFFF;

// A content file is missing, is not valid JSON, or breaks a rule. The message names the file and
// the field, written for a game designer.
class ContentError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

constexpr std::size_t MAX_LEVEL_TITLE = 48;
constexpr std::size_t MAX_LEVEL_STORY = 512;

// Reads <directory>/enemies.json, story.json (and the level files it lists) and endless.json.
// Throws ContentError on the first problem found.
GameContent loadContent(const std::filesystem::path& directory);

} // namespace game
