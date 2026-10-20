#include "client/Scene.hpp"

#include <algorithm>
#include <cmath>
#include <random>
#include <type_traits>

namespace client {

namespace {

using engine::render::Color;
using engine::render::IRenderer;
using rtype::protocol::EntityKind;
constexpr std::array<Color, 4> PLAYER_COLORS{Color{100, 220, 255}, Color{255, 200, 90},
                                             Color{255, 130, 200}, Color{160, 255, 150}};
constexpr float FIELD_TOP = 56.0F;
constexpr float FIELD_WIDTH = 800.0F;
constexpr float FIELD_HEIGHT = 600.0F;

Color playerColor(std::uint8_t id) { return PLAYER_COLORS[id % PLAYER_COLORS.size()]; }

} // namespace

Scene::Scene() {
  std::mt19937 random(42);
  std::uniform_real_distribution<float> x(0.0F, FIELD_WIDTH);
  std::uniform_real_distribution<float> y(0.0F, FIELD_HEIGHT);
  std::uniform_real_distribution<float> speed(12.0F, 45.0F);
  for (auto& star : _stars) {
    star = {x(random), y(random), speed(random)};
  }
}

void Scene::load(IRenderer& renderer, engine::audio::IAudio& audio,
                 const std::filesystem::path& assets) {
  renderer.loadTexture("ships", assets / "r-typesheet1.gif", true);
  renderer.loadTexture("bydos", assets / "r-typesheet5.gif", true);
  renderer.loadTexture("missiles", assets / "r-typesheet3.gif", true);
  audio.load("shot", assets / "shot.wav");
  audio.load("explosion", assets / "explosion.wav");
}

void Scene::update(float seconds) {
  _animationTime = std::fmod(_animationTime + seconds, 60.0F);
  _noticeRemaining = std::max(0.0F, _noticeRemaining - seconds);
  for (auto& star : _stars) {
    star.x = std::fmod(star.x - star.speed * seconds + FIELD_WIDTH, FIELD_WIDTH);
  }
  for (auto& explosion : _explosions) {
    explosion.age += seconds;
  }
  std::erase_if(_explosions, [](const auto& explosion) { return explosion.age >= 0.4F; });
}

void Scene::reset() {
  _explosions.clear();
  _notice.clear();
  _noticeRemaining = 0.0F;
}

void Scene::handleEvents(net::NetworkClient& network, engine::audio::IAudio& audio) {
  for (const auto& event : network.pollEvents()) {
    std::visit(
        [this, &audio](const auto& value) {
          using T = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<T, net::EntitySpawned>) {
            if (value.entity.kind == EntityKind::PlayerMissile ||
                value.entity.kind == EntityKind::EnemyMissile) {
              audio.play("shot");
            }
          } else if constexpr (std::is_same_v<T, net::EntityDestroyed>) {
            if (value.reason == rtype::protocol::DestroyReason::Killed) {
              // Cap transient effects even if many events arrive in a single frame.
              if (_explosions.size() < 64) {
                _explosions.push_back({value.entity.x, value.entity.y, 0.0F});
              }
              audio.play("explosion");
            }
          } else if constexpr (std::is_same_v<T, net::PlayerLeft>) {
            _notice =
                "Player " + std::to_string(value.playerId + 1) +
                (value.reason == rtype::protocol::LeaveReason::TimedOut ? " disconnected (timeout)"
                                                                        : " left the game");
            _noticeRemaining = 4.0F;
          } else if constexpr (std::is_same_v<T, net::ConnectionLost>) {
            _notice = "Connection lost";
            _noticeRemaining = 4.0F;
          }
        },
        event);
  }
}

void Scene::drawBackground(IRenderer& renderer) const {
  for (const auto& star : _stars) {
    const auto brightness = static_cast<std::uint8_t>(90.0F + star.speed * 3.0F);
    const float size = star.speed > 30.0F ? 2.0F : 1.0F;
    renderer.drawRectangle(star.x, star.y + FIELD_TOP, size, size,
                           {brightness, brightness, brightness});
  }
}

void Scene::drawGame(IRenderer& renderer, const net::NetworkClient& network, bool largeText) const {
  if (!network.isConnected() || network.worldWidth() == 0 || network.worldHeight() == 0) {
    return;
  }
  const float scaleX = FIELD_WIDTH / static_cast<float>(network.worldWidth());
  const float scaleY = FIELD_HEIGHT / static_cast<float>(network.worldHeight());
  const int frame = static_cast<int>(_animationTime * 8.0F) % 8;
  for (const auto& [id, entity] : network.entities()) {
    (void)id;
    const float x = entity.x * scaleX;
    const float y = FIELD_TOP + entity.y * scaleY;
    switch (entity.kind) {
    case EntityKind::Player:
      renderer.drawSprite("ships", {101, 0, 33, 17}, x, y, 33.0F * scaleX, 17.0F * scaleY);
      // Labels and bars distinguish players without relying on colour perception.
      renderer.drawRectangle(x, y + 19.0F * scaleY, 33.0F * scaleX, 2.0F,
                             playerColor(entity.playerId));
      renderer.drawText("P" + std::to_string(entity.playerId + 1), x, y - 19.0F, 14,
                        playerColor(entity.playerId));
      break;
    case EntityKind::Bydos:
      renderer.drawSprite("bydos", {frame * 33, 0, 33, 36}, x, y, 32.0F * scaleX, 24.0F * scaleY);
      break;
    case EntityKind::PlayerMissile:
      renderer.drawSprite("missiles", {0, 0, 17, 18}, x, y, 8.0F * scaleX, 4.0F * scaleY,
                          playerColor(entity.playerId));
      break;
    case EntityKind::EnemyMissile:
      renderer.drawSprite("missiles", {34, 0, 17, 18}, x, y, 8.0F * scaleX, 4.0F * scaleY);
      break;
    }
  }
  for (const auto& explosion : _explosions) {
    const int explosionFrame = std::min(7, static_cast<int>(explosion.age * 20.0F));
    renderer.drawSprite("ships", {70 + explosionFrame * 33, 294, 33, 34}, explosion.x * scaleX,
                        FIELD_TOP + explosion.y * scaleY, 33.0F * scaleX, 34.0F * scaleY);
  }

  renderer.drawRectangle(0, 0, 800, FIELD_TOP, {12, 18, 32});
  for (const auto& player : network.players()) {
    const float x = 12.0F + static_cast<float>(player.playerId) * 200.0F;
    const std::string label = "P" + std::to_string(player.playerId + 1) +
                              (player.playerId == network.playerId() ? " YOU" : "");
    renderer.drawText(label + "  Lives " + std::to_string(player.lives), x, 2,
                      largeText ? 20U : 16U, playerColor(player.playerId));
    renderer.drawText("HP " + std::to_string(player.health) + "   Score " +
                          std::to_string(player.score),
                      x, 28, largeText ? 18U : 14U);
    if (player.playerId == network.playerId() && player.lives == 0) {
      renderer.drawRectangle(145, 270, 510, 115, {12, 18, 32, 235});
      renderer.drawText("GAME OVER", 285, 280, 32);
      renderer.drawText("Escape: leave    Enter: rejoin", 215, 335, 22);
    }
  }
  if (_noticeRemaining > 0.0F) {
    renderer.drawRectangle(0, 610, 800, 42, {12, 18, 32, 230});
    renderer.drawText(_notice, 16, 617, largeText ? 23U : 18U);
  }
}

} // namespace client
