#include <algorithm>
#include <cmath>
#include <random>

#include <catch2/catch_test_macros.hpp>

#include "engine/core/SystemScheduler.hpp"
#include "game/EnemyBehaviors.hpp"
#include "game/EquipmentSystems.hpp"
#include "game/LevelDirector.hpp"

namespace {

using namespace rtype::common::components;
using Entity = engine::ecs::Entity;
constexpr float TICK = 1.0F / 60.0F;
constexpr game::WorldBounds BOUNDS{800.0F, 600.0F};

struct World {
  engine::ecs::Registry registry;
  std::mt19937 random{42};
  game::DifficultySettings normal{};

  World() { game::registerSystems(registry); }

  Entity player(float x, float y) {
    const auto entity = registry.spawnEntity();
    registry.addComponent(entity, Player{0, 3, 0});
    registry.addComponent(entity, Health{3, 3});
    registry.addComponent(entity, Position{x, y});
    registry.addComponent(entity, Velocity{});
    registry.addComponent(entity, Hitbox{33.0F, 17.0F});
    registry.addComponent(entity, Equipment{});
    return entity;
  }

  // Behaviours, then movement: what the server does every tick.
  void run(float seconds, const game::GameContent* content = nullptr) {
    for (float elapsed = 0.0F; elapsed < seconds; elapsed += TICK) {
      game::updateEnemyBehaviors(registry, TICK, BOUNDS, random);
      if (content != nullptr) {
        game::updateBosses(registry, TICK, BOUNDS, *content, normal, random);
      }
      game::updateMovement(registry, TICK);
    }
  }

  std::size_t enemyShots() {
    const auto shots = registry.view<Projectile>();
    return static_cast<std::size_t>(std::ranges::count_if(shots, [this](Entity shot) {
      return !registry.getComponent<Projectile>(shot)->friendly;
    }));
  }
};

game::EnemyType type(MovementPattern movement) {
  game::EnemyType enemy;
  enemy.name = "test";
  enemy.movement = movement;
  enemy.speed = 60.0F;
  enemy.amplitude = 50.0F;
  enemy.frequency = 1.0F;
  return enemy;
}

} // namespace

TEST_CASE("Sine enemies ride a wave around their spawn height", "[game][behaviour]") {
  World world;
  const auto enemy = game::spawnEnemyOfType(world.registry, type(MovementPattern::Sine), 700.0F,
                                            300.0F, world.normal, 1.0F, world.random);
  float lowest = 300.0F;
  float highest = 300.0F;
  for (int step = 0; step < 120; ++step) {
    world.run(TICK);
    const auto y = world.registry.getComponent<Position>(enemy)->y;
    lowest = std::min(lowest, y);
    highest = std::max(highest, y);
  }
  CHECK(highest > 340.0F);
  CHECK(lowest < 260.0F);
  CHECK(highest < 355.0F); // amplitude 50, never further
  CHECK(lowest > 245.0F);
  CHECK(world.registry.getComponent<Position>(enemy)->x < 700.0F - 100.0F);
}

TEST_CASE("Chargers steer towards the closest ship, zigzags turn back", "[game][behaviour]") {
  World world;
  world.player(100.0F, 500.0F);
  const auto charger = game::spawnEnemyOfType(world.registry, type(MovementPattern::Chase),
                                              700.0F, 100.0F, world.normal, 1.0F, world.random);
  const auto zigzag = game::spawnEnemyOfType(world.registry, type(MovementPattern::Zigzag),
                                             700.0F, 300.0F, world.normal, 1.0F, world.random);
  world.run(0.4F);
  CHECK(world.registry.getComponent<Position>(charger)->y > 110.0F);
  const auto down = world.registry.getComponent<Velocity>(zigzag)->vy;
  world.run(1.0F);
  CHECK(world.registry.getComponent<Velocity>(zigzag)->vy * down < 0.0F);
}

TEST_CASE("Enemies fire volleys only at ships on screen", "[game][behaviour]") {
  World world;
  auto turret = type(MovementPattern::Turret);
  turret.fireRate = 2.0F;
  turret.burst = 3;
  game::spawnEnemyOfType(world.registry, turret, 900.0F, 300.0F, world.normal, 1.0F, world.random);
  world.player(100.0F, 300.0F);
  world.run(1.0F); // still off screen
  CHECK(world.enemyShots() == 0);

  World inView;
  game::spawnEnemyOfType(inView.registry, turret, 600.0F, 300.0F, inView.normal, 1.0F,
                         inView.random);
  inView.run(1.0F); // nobody to shoot at
  CHECK(inView.enemyShots() == 0);
  inView.player(100.0F, 300.0F);
  inView.run(1.05F);
  CHECK(inView.enemyShots() >= 3);
  CHECK(inView.enemyShots() % 3 == 0); // whole volleys of three
}

TEST_CASE("Difficulty scales the health of the enemies it spawns", "[game][behaviour]") {
  World world;
  auto tough = type(MovementPattern::Straight);
  tough.health = 10;
  const auto easy = game::spawnEnemyOfType(world.registry, tough, 700, 100,
                                           game::difficultySettings(game::Difficulty::Easy), 1.0F,
                                           world.random);
  const auto hard = game::spawnEnemyOfType(world.registry, tough, 700, 300,
                                           game::difficultySettings(game::Difficulty::Hard), 1.0F,
                                           world.random);
  CHECK(world.registry.getComponent<Health>(easy)->max == 6);
  CHECK(world.registry.getComponent<Health>(hard)->max == 15);
}

TEST_CASE("Bosses fly in, change phase with their health and summon help", "[game][boss]") {
  World world;
  game::GameContent content;
  content.enemies.push_back(type(MovementPattern::Straight));
  game::BossType boss;
  boss.name = "boss";
  boss.health = 100;
  game::BossPhase calm;
  calm.attacks = {game::BossAttack::Spread};
  calm.interval = 0.5F;
  calm.bullets = 4;
  game::BossPhase angry;
  angry.below = 0.5F;
  angry.attacks = {game::BossAttack::Summon};
  angry.summon = "test";
  angry.interval = 0.5F;
  boss.phases = {calm, angry};
  content.bosses.push_back(boss);

  const auto entity = game::spawnBoss(world.registry, content, 0, 0, world.normal, BOUNDS);
  CHECK(world.registry.getComponent<Position>(entity)->x >= BOUNDS.width);
  CHECK(world.registry.getComponent<Enemy>(entity)->pattern == Enemy::BOSS_PATTERN);
  world.run(4.0F, &content);
  const auto* position = world.registry.getComponent<Position>(entity);
  CHECK(position->x + world.registry.getComponent<Hitbox>(entity)->width <= BOUNDS.width);
  CHECK(world.enemyShots() > 0); // spread fires even with nobody in front
  CHECK(world.registry.view<Enemy>().size() == 1);

  CHECK(game::bossPhaseFor(content.bosses[0], 1.0F) == 0);
  CHECK(game::bossPhaseFor(content.bosses[0], 0.5F) == 1);
  world.registry.getComponent<Health>(entity)->current = 40;
  world.run(1.0F, &content);
  CHECK(world.registry.getComponent<BossBehavior>(entity)->phase == 1);
  CHECK(world.registry.view<Enemy>().size() > 1); // minions summoned
  for (int step = 0; step < 600; ++step) {
    world.run(TICK, &content);
    const auto* boxed = world.registry.getComponent<Position>(entity);
    REQUIRE(boxed->y >= 0.0F);
    REQUIRE(boxed->y + world.registry.getComponent<Hitbox>(entity)->height <= BOUNDS.height);
  }
}

TEST_CASE("A shield absorbs one hit, a respawned ship is briefly untouchable",
          "[game][equipment]") {
  World world;
  const auto ship = world.player(100.0F, 100.0F);
  game::applyPickup(world.registry, ship, PickupType::Shield);
  const auto bydos = game::spawnEnemy(world.registry, 300.0F, 100.0F, 0.0F, 0.0F);
  game::fireEnemyProjectile(world.registry, bydos, ship, 200.0F);
  const auto shot = world.registry.view<Projectile>().front();
  *world.registry.getComponent<Position>(shot) = *world.registry.getComponent<Position>(ship);
  CHECK(game::resolveProjectileCollisions(world.registry) == 1);
  CHECK(world.registry.getComponent<Health>(ship)->current == 3);
  CHECK(world.registry.getComponent<Equipment>(ship)->shieldSeconds == 0.0F);

  world.registry.getComponent<Health>(ship)->current = 0;
  game::handlePlayerDeaths(world.registry);
  CHECK(world.registry.getComponent<Equipment>(ship)->invulnerableSeconds > 0.0F);
  game::fireEnemyProjectile(world.registry, bydos, ship, 200.0F);
  const auto second = world.registry.view<Projectile>().front();
  *world.registry.getComponent<Position>(second) = *world.registry.getComponent<Position>(ship);
  CHECK(game::resolveProjectileCollisions(world.registry) == 0);
  CHECK(world.registry.containsEntity(second)); // flies through
  CHECK(world.registry.getComponent<Health>(ship)->current == 3);
  game::updateEquipment(world.registry, game::RESPAWN_INVULNERABLE_SECONDS + 0.1F);
  CHECK(game::resolveProjectileCollisions(world.registry) == 1);
  CHECK(world.registry.getComponent<Health>(ship)->current == 2);
}

TEST_CASE("The final boss drops the Cup, and the campaign ends when it is caught", "[game][boss]") {
  World world;
  auto content = std::make_shared<game::GameContent>();
  game::BossType thief;
  thief.name = "thief";
  thief.health = 3;
  thief.trophy = true;
  content->bosses.push_back(thief);
  game::LevelDefinition level;
  level.number = 1;
  game::WaveDefinition wave;
  wave.boss = true;
  wave.enemyCount = 1;
  wave.spawnIntervalSeconds = 0.1F;
  wave.bossType = "thief";
  level.waves = {wave};
  content->story.define(level);
  content->story.setFinalLevel(1);
  game::LevelDirector director(1, BOUNDS, content->story, 1, content);
  const auto ship = world.player(100.0F, 100.0F);

  director.update(world.registry, 0.1F);
  REQUIRE(world.registry.view<BossBehavior>().size() == 1);
  CHECK(director.progress().bossMaxHealth == 3);
  const auto boss = world.registry.view<BossBehavior>().front();
  CHECK(world.registry.getComponent<LootDrop>(boss)->type == PickupType::Trophy);
  world.registry.getComponent<Health>(boss)->current = 0;
  game::removeDeadEntities(world.registry);
  REQUIRE(world.registry.view<Pickup>().size() == 1);
  const auto cup = world.registry.view<Pickup>().front();
  CHECK(world.registry.getComponent<Velocity>(cup)->vx == 0.0F); // waits for the players

  director.update(world.registry, 0.1F);
  CHECK(director.progress().phase == game::LevelPhase::Clearing);
  game::updateEquipment(world.registry, 60.0F);
  CHECK(world.registry.containsEntity(cup)); // never expires

  *world.registry.getComponent<Position>(ship) = *world.registry.getComponent<Position>(cup);
  CHECK(game::collectPickups(world.registry) == 1);
  CHECK(world.registry.getComponent<Player>(ship)->score == game::TROPHY_POINTS);
  director.update(world.registry, 0.1F);
  CHECK(director.progress().phase == game::LevelPhase::Complete);
}

TEST_CASE("Formations place a wave's enemies as designed", "[game][levels]") {
  World world;
  auto content = std::make_shared<game::GameContent>();
  content->enemies.push_back(type(MovementPattern::Straight));
  game::LevelCatalog catalog;
  game::LevelDefinition level;
  game::WaveDefinition wave;
  wave.enemy = "test";
  wave.enemyCount = 4;
  wave.spawnIntervalSeconds = 0.1F;
  wave.formation = game::Formation::Line;
  level.waves = {wave};
  level.introSeconds = 1.0F;
  catalog.define(level);
  game::LevelDirector director(7, BOUNDS, catalog, 1, content);
  CHECK(director.progress().phase == game::LevelPhase::Intro);
  director.update(world.registry, 0.5F);
  CHECK(world.registry.view<Enemy>().empty()); // title card first
  director.update(world.registry, 0.6F);
  for (int step = 0; step < 4; ++step) {
    director.update(world.registry, 0.1F);
  }
  const auto enemies = world.registry.view<Enemy>();
  REQUIRE(enemies.size() == 4);
  const float y = world.registry.getComponent<Position>(enemies.front())->y;
  for (const auto enemy : enemies) {
    CHECK(world.registry.getComponent<Position>(enemy)->y == y);
    CHECK(world.registry.getComponent<Appearance>(enemy) != nullptr);
  }
}
