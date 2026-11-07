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
using rtype::protocol::GamePhase;
namespace protocol = rtype::protocol;

constexpr float FIELD_TOP = 56.0F;
constexpr float FIELD_WIDTH = 800.0F;
constexpr float FIELD_HEIGHT = 600.0F;
constexpr float CANVAS_HEIGHT = 680.0F;
constexpr float EXPLOSION_SECONDS = 0.5F;
constexpr float VICTORY_JINGLE_SECONDS = 3.3F;

// Player colours, in player id order: red, blue, green, yellow (the ships of ships.png).
constexpr std::array<Color, 4> PLAYER_COLORS{Color{235, 45, 60}, Color{60, 140, 255},
                                             Color{60, 220, 100}, Color{255, 215, 40}};
constexpr std::array<const char*, 4> WEAPON_NAMES{"SHOT", "DOUBLE", "SPREAD", "HEAVY"};

// One palette per level theme: background, stars, nebula, two planet tints.
struct Theme {
  Color background;
  Color star;
  Color nebula;
  Color planetA;
  Color planetB;
};
constexpr std::array<Theme, protocol::THEME_COUNT> THEMES{{
    {{4, 8, 18}, {220, 230, 255}, {60, 90, 200}, {120, 170, 255}, {255, 180, 120}},
    {{14, 9, 6}, {255, 225, 200}, {170, 90, 40}, {210, 150, 90}, {150, 120, 100}},
    {{3, 14, 12}, {200, 255, 230}, {40, 170, 120}, {120, 255, 180}, {90, 160, 255}},
    {{16, 4, 8}, {255, 210, 210}, {190, 40, 60}, {255, 110, 110}, {200, 200, 210}},
    {{10, 4, 20}, {240, 220, 255}, {130, 60, 200}, {200, 140, 255}, {255, 215, 90}},
}};

constexpr std::array<int, 6> SIX_FRAMES{0, 33, 66, 100, 133, 166};
constexpr std::array<engine::render::Rect, 3> PLANET_FRAMES{
    {{0, 0, 96, 96}, {98, 0, 64, 64}, {164, 0, 40, 40}}};

struct Size {
  float width;
  float height;
};

// Drawn size of each kind, in world units (the server uses the same hitboxes).
Size sizeOf(EntityKind kind) {
  switch (kind) {
  case EntityKind::Player: return {33, 17};
  case EntityKind::Bydos: return {32, 24};
  case EntityKind::PlayerMissile:
  case EntityKind::EnemyMissile: return {8, 4};
  case EntityKind::Boss: return {120, 96};
  case EntityKind::FinalBoss: return {150, 120};
  case EntityKind::Trophy: return {40, 40};
  case EntityKind::Waver: return {30, 30};
  case EntityKind::Charger: return {28, 28};
  case EntityKind::Turret: return {32, 32};
  case EntityKind::BossMissile: return {12, 12};
  case EntityKind::ExtraLife:
  case EntityKind::Heal:
  case EntityKind::RapidFire:
  case EntityKind::DoubleShot:
  case EntityKind::SpreadShot:
  case EntityKind::HeavyShot:
  case EntityKind::Shield: return {20, 20};
  }
  return {16, 16};
}

bool isBoss(EntityKind kind) { return kind == EntityKind::Boss || kind == EntityKind::FinalBoss; }

bool isEnemy(EntityKind kind) {
  return kind == EntityKind::Bydos || kind == EntityKind::Waver || kind == EntityKind::Charger ||
         kind == EntityKind::Turret || isBoss(kind);
}

bool isPickup(EntityKind kind) {
  return kind >= EntityKind::ExtraLife && kind <= EntityKind::Trophy;
}

// Draw order: pickups, enemies, enemy shots, player shots, ships on top.
int layerOf(EntityKind kind) {
  if (isPickup(kind)) {
    return 0;
  }
  if (isEnemy(kind)) {
    return 1;
  }
  if (kind == EntityKind::EnemyMissile || kind == EntityKind::BossMissile) {
    return 2;
  }
  return kind == EntityKind::PlayerMissile ? 3 : 4;
}

Color withAlpha(Color color, float alpha) {
  color.a = static_cast<std::uint8_t>(std::clamp(alpha, 0.0F, 1.0F) * 255.0F);
  return color;
}

// A cheap repeatable pseudo-random number in [0, 1), for effects only.
float noise(float seed) {
  const float value = std::sin(seed * 12.9898F) * 43758.5453F;
  return value - std::floor(value);
}

// Approximate centring: the renderer cannot measure text, DejaVu Sans averages 0.58 em.
void centred(IRenderer& renderer, const std::string& text, float y, unsigned int size,
             Color color = {}) {
  const float width = static_cast<float>(text.size()) * static_cast<float>(size) * 0.58F;
  renderer.drawText(text, FIELD_WIDTH / 2.0F - width / 2.0F, y, size, color);
}

const protocol::PlayerState* findPlayer(const net::NetworkClient& network, std::uint8_t id) {
  const auto& players = network.players();
  const auto found = std::ranges::find(players, id, &protocol::PlayerState::playerId);
  return found == players.end() ? nullptr : &*found;
}

std::vector<std::string> lines(const std::string& text) {
  std::vector<std::string> result(1);
  for (const char character : text) {
    if (character == '\n') {
      result.emplace_back();
    } else {
      result.back().push_back(character);
    }
  }
  return result;
}

} // namespace

Scene::Scene() {
  std::mt19937 random(42);
  std::uniform_real_distribution<float> x(0.0F, FIELD_WIDTH);
  std::uniform_real_distribution<float> y(0.0F, FIELD_HEIGHT);
  for (std::size_t index = 0; index < _stars.size(); ++index) {
    // Three parallax layers: far and slow, middle, near and fast.
    const auto layer = index % 3;
    std::uniform_real_distribution<float> speed(layer == 0 ? 8.0F : layer == 1 ? 22.0F : 55.0F,
                                                layer == 0 ? 16.0F : layer == 1 ? 40.0F : 90.0F);
    _stars[index] = {x(random), y(random), speed(random)};
  }
  _planets = {{{520.0F, 90.0F, 4.0F, 0}, {120.0F, 380.0F, 7.0F, 1}, {700.0F, 470.0F, 11.0F, 2}}};
  _nebula = {0.0F, 330.0F, 660.0F, 990.0F};
}

void Scene::load(IRenderer& renderer, engine::audio::IAudio& audio,
                 const std::filesystem::path& assets) {
  // Supplied R-Type sheets: their top-left colour is the background to remove.
  renderer.loadTexture("sheet1", assets / "r-typesheet1.gif", true);
  renderer.loadTexture("bydos", assets / "r-typesheet5.gif", true);
  renderer.loadTexture("missiles", assets / "r-typesheet3.gif", true);
  renderer.loadTexture("waver", assets / "r-typesheet7.gif", true);
  renderer.loadTexture("charger", assets / "r-typesheet8.gif", true);
  renderer.loadTexture("turret", assets / "r-typesheet11.gif", true);
  // Generated art with transparency (assets/tools/generate_sprites.py).
  for (const auto* name : {"ships", "t1", "trophy", "trophy_big", "pickups", "boss_gatekeeper",
                           "boss_final", "boss_shot", "shield", "planets"}) {
    renderer.loadTexture(name, assets / (std::string(name) + ".png"), false);
  }
  for (const auto* name : {"shot", "explosion", "big_explosion", "pickup", "hit", "shield",
                           "warning", "stage_clear", "victory", "select"}) {
    audio.load(name, assets / (std::string(name) + ".wav"));
  }
  audio.loadMusic("menu", assets / "music_menu.wav");
  audio.loadMusic("stage", assets / "music_stage.wav");
  audio.loadMusic("boss", assets / "music_boss.wav");
}

void Scene::update(float seconds) {
  _time = std::fmod(_time + seconds, 3600.0F);
  _noticeRemaining = std::max(0.0F, _noticeRemaining - seconds);
  for (auto& star : _stars) {
    star.x = std::fmod(star.x - star.speed * seconds + FIELD_WIDTH, FIELD_WIDTH);
  }
  for (auto& planet : _planets) {
    planet.x -= planet.speed * seconds;
    if (planet.x < -100.0F) {
      planet.x += FIELD_WIDTH + 200.0F;
    }
  }
  for (std::size_t index = 0; index < _nebula.size(); ++index) {
    const float speed = 3.0F + static_cast<float>(index);
    _nebula[index] = std::fmod(_nebula[index] - speed * seconds + 1320.0F, 1320.0F);
  }
  for (auto& explosion : _explosions) {
    explosion.age += seconds;
  }
  std::erase_if(_explosions,
                [](const auto& explosion) { return explosion.age >= EXPLOSION_SECONDS; });
  for (auto& piece : _confetti) {
    piece.y += piece.speed * seconds;
    piece.x += std::sin(_time * 3.0F + piece.sway) * 30.0F * seconds;
    if (piece.y > CANVAS_HEIGHT) {
      piece.y -= CANVAS_HEIGHT + 40.0F;
    }
  }
  for (auto& caption : _captions) {
    caption.remaining -= seconds;
  }
  while (!_captions.empty() && _captions.front().remaining <= 0.0F) {
    _captions.pop_front();
  }
  _shake = std::max(0.0F, _shake - seconds);
  _warning = std::max(0.0F, _warning - seconds);
  _bossFlash = std::max(0.0F, _bossFlash - seconds);
  _victoryTime += seconds;
  for (std::size_t id = 0; id < _tilt.size(); ++id) {
    _hitFlash[id] = std::max(0.0F, _hitFlash[id] - seconds);
    _still[id] += seconds;
    if (_still[id] > 0.1F) {
      _tilt[id] *= std::exp(-seconds * 8.0F); // level out once the ship stops climbing
    }
  }
}

void Scene::reset() {
  _explosions.clear();
  _confetti.clear();
  _captions.clear();
  _notice.clear();
  _noticeRemaining = 0.0F;
  _shake = 0.0F;
  _warning = 0.0F;
  _bossFlash = 0.0F;
  _previousPlayers.clear();
  _previousStatus = {};
  _hitFlash.fill(0.0F);
  _tilt.fill(0.0F);
  _theme = 0;
}

void Scene::explode(float x, float y, float size, float delay) {
  // Cap transient effects even if many events arrive in a single frame.
  if (_explosions.size() < 96) {
    _explosions.push_back({x, y, -delay, size});
  }
}

void Scene::caption(std::string text) {
  if (_captions.size() >= 4) {
    _captions.pop_front();
  }
  _captions.push_back({std::move(text), 2.5F});
}

void Scene::handleEvents(net::NetworkClient& network, engine::audio::IAudio& audio) {
  for (const auto& event : network.pollEvents()) {
    std::visit(
        [this, &audio](const auto& value) {
          using T = std::decay_t<decltype(value)>;
          if constexpr (std::is_same_v<T, net::EntitySpawned>) {
            if (value.entity.kind == EntityKind::PlayerMissile) {
              audio.play("shot");
            }
          } else if constexpr (std::is_same_v<T, net::EntityDestroyed>) {
            const auto kind = value.entity.kind;
            if (value.reason != protocol::DestroyReason::Killed ||
                !(isEnemy(kind) || kind == EntityKind::Player)) {
              return;
            }
            const auto size = sizeOf(kind);
            const float centreX = value.entity.x + size.width / 2.0F;
            const float centreY = value.entity.y + size.height / 2.0F;
            if (isBoss(kind)) {
              // A chain of blasts over the whole wreck, and the screen shakes.
              for (int index = 0; index < 14; ++index) {
                const float seed = static_cast<float>(index) + 1.0F;
                explode(value.entity.x + noise(seed) * size.width,
                        value.entity.y + noise(seed * 7.0F) * size.height, 48.0F,
                        static_cast<float>(index) * 0.08F);
              }
              explode(centreX, centreY, 110.0F, 1.1F);
              _shake = 1.4F;
              _shakeStrength = 9.0F;
              audio.play("big_explosion");
              caption("[Huge explosion: the boss is down]");
            } else {
              explode(centreX, centreY, std::max(size.width, size.height) * 1.1F);
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
  reactToStatus(network, audio);
  reactToPlayers(network, audio);
  trackShips(network);
}

void Scene::reactToStatus(const net::NetworkClient& network, engine::audio::IAudio& audio) {
  const auto& status = network.gameStatus();
  if (network.levelInfo().level != 0) {
    _theme = static_cast<std::uint8_t>(network.levelInfo().theme % THEMES.size());
  }
  if (status.phase != _previousStatus.phase) {
    switch (status.phase) {
    case GamePhase::BossFight:
      _warning = 3.0F;
      audio.play("warning");
      caption("[Alarm: a boss is coming]");
      break;
    case GamePhase::StageClear:
      audio.play("stage_clear");
      caption("[Fanfare: stage clear]");
      break;
    case GamePhase::Victory: {
      audio.play("victory");
      caption("[Victory fanfare]");
      _victoryTime = 0.0F;
      _confetti.clear();
      for (int index = 0; index < 90; ++index) {
        const float seed = static_cast<float>(index) + 0.5F;
        const Color color = index % 5 == 4 ? Color{255, 215, 90}
                                           : PLAYER_COLORS[static_cast<std::size_t>(index % 4)];
        _confetti.push_back({noise(seed) * FIELD_WIDTH, -noise(seed * 3.0F) * CANVAS_HEIGHT,
                             60.0F + noise(seed * 5.0F) * 90.0F, noise(seed * 9.0F) * 6.0F,
                             color});
      }
      break;
    }
    case GamePhase::GameOver: caption("[Game over]"); break;
    case GamePhase::Lobby:
    case GamePhase::Intro:
    case GamePhase::Playing: break;
    }
    if (status.phase != GamePhase::Victory) {
      _confetti.clear();
    }
  }
  if (status.bossMaxHealth > 0 && status.bossMaxHealth == _previousStatus.bossMaxHealth &&
      status.bossHealth < _previousStatus.bossHealth) {
    _bossFlash = 0.08F;
  }
  _previousStatus = status;
}

void Scene::reactToPlayers(const net::NetworkClient& network, engine::audio::IAudio& audio) {
  const auto phase = network.gameStatus().phase;
  // A restart refills every ship: only changes during play are hits and pickups.
  const bool inPlay = phase == GamePhase::Playing || phase == GamePhase::BossFight ||
                      phase == GamePhase::StageClear;
  bool hit = false;
  bool shieldDown = false;
  bool pickup = false;
  for (const auto& player : network.players()) {
    const auto previous = std::ranges::find(_previousPlayers, player.playerId,
                                            &protocol::PlayerState::playerId);
    if (previous == _previousPlayers.end() || player.playerId >= _hitFlash.size()) {
      continue;
    }
    const bool hadShield = (previous->flags & protocol::player::SHIELD) != 0;
    const bool hasShield = (player.flags & protocol::player::SHIELD) != 0;
    if (player.lives < previous->lives || player.health < previous->health) {
      _hitFlash[player.playerId] = 0.35F;
      hit = true;
      if (player.playerId == network.playerId()) {
        _shake = std::max(_shake, 0.25F);
        _shakeStrength = std::max(_shakeStrength, 4.0F);
      }
    } else if (hadShield && !hasShield) {
      shieldDown = true;
    }
    if (inPlay && (player.lives > previous->lives ||
                   (player.weapon != previous->weapon && player.weapon != 0) ||
                   player.rapidFireMilliseconds > previous->rapidFireMilliseconds + 1000 ||
                   (hasShield && !hadShield) ||
                   (player.health > previous->health && player.lives == previous->lives))) {
      pickup = true;
    }
  }
  if (hit) {
    audio.play("hit");
    caption("[Ship hit]");
  }
  if (shieldDown) {
    audio.play("shield");
    caption("[Shield down]");
  }
  if (pickup) {
    audio.play("pickup");
    caption("[Power-up collected]");
  }
  _previousPlayers = network.players();
}

void Scene::trackShips(const net::NetworkClient& network) {
  for (const auto& [id, entity] : network.entities()) {
    (void)id;
    if (entity.kind != EntityKind::Player || entity.playerId >= _tilt.size()) {
      continue;
    }
    const auto player = entity.playerId;
    const float delta = entity.y - _lastY[player];
    if (std::abs(delta) > 0.01F) {
      if (std::abs(delta) < 40.0F) { // not a respawn or a teleport
        _tilt[player] = std::clamp(_tilt[player] + (delta > 0.0F ? 0.35F : -0.35F), -1.0F, 1.0F);
      }
      _still[player] = 0.0F;
      _lastY[player] = entity.y;
    }
  }
}

void Scene::updateMusic(const net::NetworkClient& network, engine::audio::IAudio& audio) const {
  if (!network.isConnected()) {
    audio.playMusic("menu");
    return;
  }
  switch (network.gameStatus().phase) {
  case GamePhase::BossFight: audio.playMusic("boss"); break;
  case GamePhase::Victory:
    if (_victoryTime < VICTORY_JINGLE_SECONDS) {
      audio.stopMusic(); // let the fanfare play alone
    } else {
      audio.playMusic("menu");
    }
    break;
  case GamePhase::Lobby:
  case GamePhase::GameOver: audio.playMusic("menu"); break;
  case GamePhase::Intro:
  case GamePhase::Playing:
  case GamePhase::StageClear: audio.playMusic("stage"); break;
  }
}

Color Scene::backgroundColor() const { return THEMES[_theme].background; }

void Scene::drawBackground(IRenderer& renderer) const {
  const auto& theme = THEMES[_theme];
  // Slow nebula bands, then planets and three layers of stars: everything scrolls with time.
  for (std::size_t index = 0; index < _nebula.size(); ++index) {
    // Nested translucent layers: the band is denser in its middle and fades at the edges.
    const float y = FIELD_TOP + 40.0F + static_cast<float>(index) * 140.0F;
    for (int layer = 0; layer < 5; ++layer) {
      const float inset = static_cast<float>(layer) * 22.0F;
      renderer.drawRectangle(_nebula[index] - 520.0F + inset * 2.0F, y + inset * 0.6F,
                             520.0F - inset * 4.0F, 120.0F - inset * 1.2F,
                             withAlpha(theme.nebula, 0.025F));
    }
  }
  for (const auto& planet : _planets) {
    const auto frame = PLANET_FRAMES.at(static_cast<std::size_t>(planet.index));
    const auto tint = planet.index == 1 ? theme.planetB : theme.planetA;
    renderer.drawSprite("planets", frame, planet.x, FIELD_TOP + planet.y,
                        static_cast<float>(frame.width), static_cast<float>(frame.height),
                        withAlpha(tint, 0.85F));
  }
  for (const auto& star : _stars) {
    const float brightness = std::min(1.0F, 0.35F + star.speed / 100.0F);
    const float size = star.speed > 50.0F ? 2.0F : 1.0F;
    const auto shade = [brightness](std::uint8_t channel) {
      return static_cast<std::uint8_t>(static_cast<float>(channel) * brightness);
    };
    renderer.drawRectangle(star.x, star.y + FIELD_TOP, size, size,
                           {shade(theme.star.r), shade(theme.star.g), shade(theme.star.b)});
  }
}

void Scene::drawGame(IRenderer& renderer, const net::NetworkClient& network,
                     const Settings& settings) const {
  if (!network.isConnected() || network.worldWidth() == 0 || network.worldHeight() == 0) {
    return;
  }
  float offsetX = 0.0F;
  float offsetY = 0.0F;
  if (settings.screenShake && _shake > 0.0F) {
    const float strength = _shakeStrength * std::min(1.0F, _shake / 0.6F);
    offsetX = std::sin(_time * 83.0F) * strength;
    offsetY = std::cos(_time * 67.0F) * strength;
  }
  drawEntities(renderer, network, offsetX, offsetY);
  drawHud(renderer, network, settings);
  drawOverlays(renderer, network, settings);
}

void Scene::drawEntities(IRenderer& renderer, const net::NetworkClient& network, float offsetX,
                         float offsetY) const {
  const float scaleX = FIELD_WIDTH / static_cast<float>(network.worldWidth());
  const float scaleY = FIELD_HEIGHT / static_cast<float>(network.worldHeight());
  const auto frameAt = [this](float rate, int count) {
    return static_cast<int>(_time * rate) % count;
  };
  for (int layer = 0; layer <= 4; ++layer) {
    for (const auto& [id, entity] : network.entities()) {
      if (layerOf(entity.kind) != layer) {
        continue;
      }
      const auto size = sizeOf(entity.kind);
      const float x = offsetX + entity.x * scaleX;
      const float y = FIELD_TOP + offsetY + entity.y * scaleY;
      const float width = size.width * scaleX;
      const float height = size.height * scaleY;
      const float seed = static_cast<float>(id % 97);
      switch (entity.kind) {
      case EntityKind::Player: {
        const auto player = static_cast<std::size_t>(entity.playerId % 4);
        const auto* state = findPlayer(network, entity.playerId);
        const auto flags = state != nullptr ? state->flags : 0;
        // Five frames: 0-1 diving, 2 level, 3-4 climbing.
        const float tilt = _tilt[player];
        const int column = tilt < -0.6F   ? 4
                           : tilt < -0.2F ? 3
                           : tilt > 0.6F  ? 0
                           : tilt > 0.2F  ? 1
                                          : 2;
        const bool blink = (flags & protocol::player::INVULNERABLE) != 0 &&
                           std::fmod(_time, 0.2F) < 0.1F;
        if (!blink) {
          const float flame = 3.0F + 2.0F * noise(_time * 30.0F + seed);
          renderer.drawRectangle(x - flame * scaleX, y + 7.0F * scaleY, flame * scaleX,
                                 3.0F * scaleY, {255, 170, 60, 220});
          const Color tint = _hitFlash[player] > 0.0F ? Color{255, 110, 110} : Color{};
          renderer.drawSprite("ships", {column * 33, static_cast<int>(player) * 17, 33, 17}, x, y,
                              width, height, tint);
        }
        if ((flags & protocol::player::SHIELD) != 0) {
          const float pulse = 0.6F + 0.3F * std::sin(_time * 6.0F);
          renderer.drawSprite("shield", {0, 0, 52, 34}, x - 9.5F * scaleX, y - 8.5F * scaleY,
                              52.0F * scaleX, 34.0F * scaleY, withAlpha({255, 255, 255}, pulse));
        }
        // Labels tell players apart without relying on colour perception.
        const std::string label = "P" + std::to_string(entity.playerId + 1) +
                                  (entity.playerId == network.playerId() ? " YOU" : "");
        renderer.drawText(label, x, y - 18.0F, 13, PLAYER_COLORS[player]);
        break;
      }
      case EntityKind::Bydos:
        renderer.drawSprite("bydos", {frameAt(8.0F, 8) * 33, 0, 33, 36}, x, y, width, height);
        break;
      case EntityKind::Waver:
        renderer.drawSprite("waver",
                            {SIX_FRAMES.at(static_cast<std::size_t>(frameAt(10.0F, 6))), 0, 33, 33},
                            x, y, width, height);
        break;
      case EntityKind::Charger:
        renderer.drawSprite("charger", {3 + frameAt(12.0F, 8) * 33, 3, 30, 30}, x, y, width,
                            height);
        break;
      case EntityKind::Turret:
        renderer.drawSprite("turret",
                            {SIX_FRAMES.at(static_cast<std::size_t>(frameAt(6.0F, 6))), 0, 33, 33},
                            x, y, width, height);
        break;
      case EntityKind::Boss:
      case EntityKind::FinalBoss: {
        const bool final = entity.kind == EntityKind::FinalBoss;
        const int frameWidth = final ? 150 : 120;
        const int frameHeight = final ? 120 : 96;
        const Color tint = _bossFlash > 0.0F ? Color{255, 120, 120} : Color{};
        renderer.drawSprite(final ? "boss_final" : "boss_gatekeeper",
                            {frameAt(3.0F, 2) * frameWidth, 0, frameWidth, frameHeight}, x, y,
                            width, height, tint);
        break;
      }
      case EntityKind::PlayerMissile:
        renderer.drawSprite("missiles", {0, 0, 17, 18}, x, y, width, height,
                            PLAYER_COLORS[entity.playerId % 4]);
        break;
      case EntityKind::EnemyMissile:
        renderer.drawSprite("missiles", {34, 0, 17, 18}, x, y, width, height);
        break;
      case EntityKind::BossMissile:
        renderer.drawSprite("boss_shot", {frameAt(10.0F, 2) * 12, 0, 12, 12}, x, y, width, height);
        break;
      case EntityKind::Trophy: {
        const float bob = std::sin(_time * 3.0F) * 4.0F;
        const float glow = 0.35F + 0.25F * std::sin(_time * 5.0F);
        renderer.drawSprite("trophy", {0, 0, 48, 48}, x - 8.0F, y - 8.0F + bob, width + 16.0F,
                            height + 16.0F, withAlpha({255, 230, 120}, glow));
        renderer.drawSprite("trophy", {0, 0, 48, 48}, x, y + bob, width, height);
        if (std::fmod(_time, 0.8F) < 0.55F) {
          renderer.drawText("CATCH THE CUP!", x - 34.0F, y - 26.0F + bob, 15, {255, 215, 90});
        }
        break;
      }
      case EntityKind::ExtraLife:
      case EntityKind::Heal:
      case EntityKind::RapidFire:
      case EntityKind::DoubleShot:
      case EntityKind::SpreadShot:
      case EntityKind::HeavyShot:
      case EntityKind::Shield: {
        const int index = static_cast<int>(entity.kind) - static_cast<int>(EntityKind::ExtraLife);
        const float bob = std::sin(_time * 4.0F + seed) * 2.0F;
        renderer.drawSprite("pickups", {index * 24, 0, 24, 24}, x - 1.0F, y - 1.0F + bob,
                            width + 2.0F, height + 2.0F);
        break;
      }
      }
    }
  }
  for (const auto& explosion : _explosions) {
    if (explosion.age < 0.0F) {
      continue; // part of a chain, not started yet
    }
    const int frame = std::min(7, static_cast<int>(explosion.age / EXPLOSION_SECONDS * 8.0F));
    const float width = explosion.size * scaleX;
    const float height = explosion.size * scaleY;
    renderer.drawSprite("sheet1", {70 + frame * 33, 294, 33, 34},
                        offsetX + explosion.x * scaleX - width / 2.0F,
                        FIELD_TOP + offsetY + explosion.y * scaleY - height / 2.0F, width, height);
  }
}

void Scene::drawHud(IRenderer& renderer, const net::NetworkClient& network,
                    const Settings& settings) const {
  const unsigned int big = settings.largeText ? 17U : 15U;
  const unsigned int small = settings.largeText ? 15U : 13U;
  renderer.drawRectangle(0, 0, FIELD_WIDTH, FIELD_TOP, {10, 14, 30});
  renderer.drawRectangle(0, FIELD_TOP - 2.0F, FIELD_WIDTH, 2, {40, 50, 90});
  for (std::uint8_t id = 0; id < protocol::MAX_PLAYERS; ++id) {
    const float x = 8.0F + static_cast<float>(id) * 200.0F;
    const auto color = PLAYER_COLORS[id];
    const auto* player = findPlayer(network, id);
    if (player == nullptr) {
      renderer.drawSprite("t1", {0, 0, 120, 48}, x, 6, 30, 12, withAlpha(color, 0.25F));
      renderer.drawText("P" + std::to_string(id + 1) + "  free", x + 36.0F, 2, small,
                        {90, 100, 130});
      continue;
    }
    if (id == network.playerId()) {
      renderer.drawRectangle(x - 6.0F, 0, 196, FIELD_TOP - 2.0F, withAlpha(color, 0.12F));
    }
    renderer.drawSprite("t1", {0, 0, 120, 48}, x, 6, 30, 12, color);
    renderer.drawText("P" + std::to_string(id + 1) + (id == network.playerId() ? " YOU" : ""),
                      x + 36.0F, 1, big, color);
    renderer.drawText(std::to_string(player->score), x + 112.0F, 1, big);
    if (player->lives == 0) {
      renderer.drawText("OUT", x, 24, big, {255, 120, 120});
      continue;
    }
    // Lives as small ships, health as pips.
    renderer.drawSprite("ships", {66, static_cast<int>(id) * 17, 33, 17}, x, 24, 22, 11);
    renderer.drawText("x" + std::to_string(player->lives), x + 24.0F, 20, small);
    for (int pip = 0; pip < 3; ++pip) {
      const bool full = pip < player->health;
      renderer.drawRectangle(x + 50.0F + static_cast<float>(pip) * 11.0F, 25, 9, 9,
                             full ? Color{90, 230, 120} : Color{50, 60, 80});
    }
    renderer.drawText(WEAPON_NAMES.at(std::min<std::size_t>(player->weapon, 3)), x + 88.0F, 21,
                      small, {200, 210, 240});
    if ((player->flags & protocol::player::SHIELD) != 0) {
      renderer.drawText("SHIELD", x + 136.0F, 21, small, {120, 200, 255});
    }
    if (player->rapidFireMilliseconds > 0) {
      const float ratio =
          std::min(1.0F, static_cast<float>(player->rapidFireMilliseconds) / 8000.0F);
      renderer.drawText("RAPID", x, 37, small - 2, {255, 170, 60});
      renderer.drawRectangle(x + 40.0F, 42, 120.0F * ratio, 5, {255, 170, 60});
    }
  }

  // Footer: what is being played, where, and the controls.
  const auto& status = network.gameStatus();
  const auto& info = network.levelInfo();
  renderer.drawRectangle(0, FIELD_TOP + FIELD_HEIGHT, FIELD_WIDTH, 24, {10, 14, 30});
  std::string where = status.mode == protocol::GameMode::Story ? "STORY" : "ENDLESS";
  constexpr std::array DIFFICULTIES{"EASY", "NORMAL", "HARD"};
  where += std::string("  ") + DIFFICULTIES.at(static_cast<std::size_t>(status.difficulty));
  if (status.level > 0) {
    where += "  LEVEL " + std::to_string(status.level) +
             (status.levelCount > 0 ? "/" + std::to_string(status.levelCount) : "");
    where += "  WAVE " + std::to_string(status.wave) + "/" + std::to_string(status.waveCount);
    if (!info.title.empty()) {
      where += "  " + info.title;
    }
  }
  renderer.drawText(where.substr(0, 70), 8, FIELD_TOP + FIELD_HEIGHT + 3.0F, small,
                    {200, 210, 240});
  const auto keyOf = [&settings](Action action) {
    return std::string(engine::input::keyName(settings.keys[static_cast<std::size_t>(action)]));
  };
  const std::string controls = (settings.autoFire ? "AUTO-FIRE" : "FIRE " + keyOf(Action::Fire)) +
                               "  F2 AUTO  M MUTE  ESC LEAVE";
  renderer.drawText(controls, 520, FIELD_TOP + FIELD_HEIGHT + 3.0F, small, {150, 160, 190});

  // Boss health bar.
  if (status.bossMaxHealth > 0) {
    const float ratio =
        static_cast<float>(status.bossHealth) / static_cast<float>(status.bossMaxHealth);
    const float y = FIELD_TOP + FIELD_HEIGHT - 26.0F;
    renderer.drawRectangle(96, y - 2.0F, 608, 18, {10, 10, 20, 200});
    renderer.drawRectangle(100, y + 2.0F, 600.0F * ratio, 10,
                           _bossFlash > 0.0F ? Color{255, 255, 255} : Color{230, 50, 70});
    renderer.drawText("BOSS", 40, y - 3.0F, big, {255, 120, 120});
    renderer.drawText(std::to_string(status.bossHealth), 712, y - 3.0F, small);
  }
}

void Scene::drawOverlays(IRenderer& renderer, const net::NetworkClient& network,
                         const Settings& settings) const {
  const auto& status = network.gameStatus();
  const auto& info = network.levelInfo();
  const unsigned int body = settings.largeText ? 20U : 17U;
  const auto countdown = std::to_string(status.countdown);

  switch (status.phase) {
  case GamePhase::Intro: {
    renderer.drawRectangle(70, 170, 660, 300, {8, 12, 28, 225});
    renderer.drawRectangle(70, 170, 660, 3, PLAYER_COLORS[0]);
    const std::string level = status.mode == protocol::GameMode::Story
                                  ? "LEVEL " + std::to_string(status.level) + " / " +
                                        std::to_string(status.levelCount)
                                  : "ROUND " + std::to_string(status.level);
    centred(renderer, level, 184, 20, {255, 215, 90});
    centred(renderer, info.title, 214, 30, {255, 255, 255});
    float y = 268.0F;
    for (const auto& line : lines(info.story)) {
      centred(renderer, line, y, body, {200, 210, 235});
      y += static_cast<float>(body) + 8.0F;
    }
    centred(renderer, "GET READY   " + countdown, 430, 22, {120, 255, 140});
    break;
  }
  case GamePhase::StageClear:
    centred(renderer, "STAGE CLEAR!", 260, 40, {255, 215, 90});
    centred(renderer, "Next level in " + countdown, 316, body);
    break;
  case GamePhase::GameOver: {
    renderer.drawRectangle(120, 230, 560, 190, {20, 8, 12, 230});
    centred(renderer, "GAME OVER", 250, 40, {255, 90, 90});
    centred(renderer, "The Bydos keep the Cup... for now.", 310, body);
    centred(renderer, "Try again in " + countdown, 350, body, {255, 215, 90});
    break;
  }
  case GamePhase::Victory: {
    renderer.drawRectangle(0, FIELD_TOP, FIELD_WIDTH, FIELD_HEIGHT, {6, 6, 18, 200});
    for (const auto& piece : _confetti) {
      renderer.drawRectangle(piece.x, piece.y, 4, 7, piece.color);
    }
    const float bob = std::sin(_time * 2.0F) * 6.0F;
    renderer.drawSprite("trophy_big", {0, 0, 192, 192}, 304, 80 + bob, 192, 192);
    centred(renderer, "THE CUP IS SAVED!", 286, 40, {255, 215, 90});
    centred(renderer, "T1 brings the Summoner's Cup home. Champions of the world!", 342, body);
    auto players = network.players();
    std::ranges::sort(players, std::greater{}, &protocol::PlayerState::score);
    float y = 384.0F;
    for (const auto& player : players) {
      centred(renderer,
              "P" + std::to_string(player.playerId + 1) + "   " + std::to_string(player.score),
              y, body + 2, PLAYER_COLORS[player.playerId % 4]);
      y += 28.0F;
    }
    centred(renderer, "New story in " + countdown, 600, body, {150, 160, 190});
    break;
  }
  case GamePhase::Lobby: centred(renderer, "Waiting for the game to start...", 330, 22); break;
  case GamePhase::Playing:
  case GamePhase::BossFight: break;
  }

  if (_warning > 0.0F && std::fmod(_time, 0.5F) < 0.32F) {
    renderer.drawRectangle(0, 250, FIELD_WIDTH, 6, {230, 40, 50});
    renderer.drawRectangle(0, 330, FIELD_WIDTH, 6, {230, 40, 50});
    centred(renderer, "WARNING", 262, 52, {255, 60, 70});
  }
  const auto* self = findPlayer(network, network.playerId());
  if (self != nullptr && self->lives == 0 && status.phase != GamePhase::GameOver &&
      status.phase != GamePhase::Victory) {
    renderer.drawRectangle(145, 270, 510, 115, {12, 18, 32, 235});
    centred(renderer, "YOU ARE OUT", 280, 32, {255, 120, 120});
    centred(renderer, "Your team fights on. ENTER: continue   ESCAPE: leave", 335, body - 2);
  }
  if (_noticeRemaining > 0.0F) {
    renderer.drawRectangle(0, 600, FIELD_WIDTH, 26, {12, 18, 32, 230});
    renderer.drawText(_notice, 16, 603, body - 2);
  }
}

void Scene::drawCaptions(IRenderer& renderer, const Settings& settings) const {
  if (!settings.captions) {
    return;
  }
  float y = FIELD_TOP + 6.0F;
  for (const auto& caption : _captions) {
    const float width = static_cast<float>(caption.text.size()) * 7.6F + 12.0F;
    renderer.drawRectangle(FIELD_WIDTH - width - 6.0F, y, width, 20, {0, 0, 0, 170});
    renderer.drawText(caption.text, FIELD_WIDTH - width, y + 1.0F, 13, {255, 240, 180});
    y += 22.0F;
  }
}

} // namespace client
