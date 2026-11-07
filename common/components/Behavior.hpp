#pragma once

#include <cstdint>

namespace rtype::common::components {

// What an enemy looks like on the clients. The server translates it into a protocol entity kind;
// gameplay code never needs to know how it is drawn.
enum class Visual : std::uint8_t {
  Bydos = 0,
  Waver = 1,
  Charger = 2,
  Turret = 3,
  Boss = 4,
  FinalBoss = 5,
  BossShot = 6, // a boss projectile: larger than a regular enemy missile
};

struct Appearance {
  Visual visual = Visual::Bydos;
};

// How a regular enemy flies:
// - Straight: right to left at a constant speed.
// - Sine: right to left, oscillating vertically (amplitude, frequency in cycles per second).
// - Zigzag: right to left, bouncing between two heights (amplitude) every 1 / frequency seconds.
// - Chase: right to left, steering vertically towards the closest player.
// - Turret: drifts slowly with the scenery and shoots volleys of aimed shots.
enum class MovementPattern : std::uint8_t {
  Straight = 0,
  Sine = 1,
  Zigzag = 2,
  Chase = 3,
  Turret = 4
};

struct EnemyBehavior {
  MovementPattern movement = MovementPattern::Straight;
  float speed = 80.0F;     // world units per second, leftwards
  float amplitude = 0.0F;  // vertical span for Sine and Zigzag, world units
  float frequency = 0.0F;  // Sine cycles or Zigzag turns per second
  float baseY = 0.0F;      // height the Sine and Zigzag patterns oscillate around
  float age = 0.0F;        // seconds since spawn
  float fireRate = 0.0F;   // volleys per second, 0: never fires
  float shotSpeed = 220.0F;
  std::uint8_t burst = 1;  // aimed shots per volley
  float fireTimer = 0.0F;  // seconds since the last volley
};

// A boss. Its attack phases live in the game content (GameContent::bosses[type]): components
// stay plain data and refer to them by index.
struct BossBehavior {
  std::uint16_t type = 0;
  std::uint8_t phase = 0;      // index of the active phase
  std::uint8_t nextAttack = 0; // index in the active phase's attack list
  float attackTimer = 0.0F;    // seconds until the next attack
  float age = 0.0F;
  float anchorX = 0.0F; // x where the boss stops after flying in
  float baseY = 0.0F;   // centre of its vertical sweep
};

} // namespace rtype::common::components
