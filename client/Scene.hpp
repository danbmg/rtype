#pragma once

#include <array>
#include <deque>
#include <filesystem>
#include <string>
#include <vector>

#include "client/Settings.hpp"
#include "client/net/NetworkClient.hpp"
#include "engine/audio/IAudio.hpp"
#include "engine/render/IRenderer.hpp"

namespace client {

// Presentation only: draws what the server reports and adds effects (explosions, screen shake,
// flashes, captions, music). It never runs gameplay systems or changes server state.
class Scene {
public:
  Scene();
  void load(engine::render::IRenderer& renderer, engine::audio::IAudio& audio,
            const std::filesystem::path& assets);
  // Animations and effects advance with the real time elapsed, never per frame.
  void update(float seconds);
  // Reacts to what changed on the server: sounds, explosions, flashes, captions.
  void handleEvents(net::NetworkClient& network, engine::audio::IAudio& audio);
  // Menu music when not connected, stage or boss music in game, the victory fanfare at the end.
  void updateMusic(const net::NetworkClient& network, engine::audio::IAudio& audio) const;
  engine::render::Color backgroundColor() const;
  void drawBackground(engine::render::IRenderer& renderer) const;
  void drawGame(engine::render::IRenderer& renderer, const net::NetworkClient& network,
                const Settings& settings) const;
  void drawCaptions(engine::render::IRenderer& renderer, const Settings& settings) const;
  void reset();

private:
  struct Star {
    float x;
    float y;
    float speed;
  };
  struct Planet {
    float x;
    float y;
    float speed;
    int index; // 0 to 2 in planets.png
  };
  struct Explosion {
    float x; // centre, world units
    float y;
    float age;
    float size;
  };
  struct Caption {
    std::string text;
    float remaining;
  };
  struct Confetti {
    float x;
    float y;
    float speed;
    float sway;
    engine::render::Color color;
  };

  void explode(float x, float y, float size, float delay = 0.0F);
  void caption(std::string text);
  void reactToStatus(const net::NetworkClient& network, engine::audio::IAudio& audio);
  void reactToPlayers(const net::NetworkClient& network, engine::audio::IAudio& audio);
  void trackShips(const net::NetworkClient& network);
  void drawEntities(engine::render::IRenderer& renderer, const net::NetworkClient& network,
                    float offsetX, float offsetY) const;
  void drawHud(engine::render::IRenderer& renderer, const net::NetworkClient& network,
               const Settings& settings) const;
  void drawOverlays(engine::render::IRenderer& renderer, const net::NetworkClient& network,
                    const Settings& settings) const;

  std::array<Star, 120> _stars{};
  std::array<Planet, 3> _planets{};
  std::array<float, 4> _nebula{};
  std::vector<Explosion> _explosions;
  std::vector<Confetti> _confetti;
  std::deque<Caption> _captions;
  std::string _notice;
  float _noticeRemaining = 0.0F;
  float _time = 0.0F;
  float _shake = 0.0F;
  float _shakeStrength = 0.0F;
  float _warning = 0.0F;   // boss warning banner
  float _bossFlash = 0.0F; // the boss was hit
  float _victoryTime = 0.0F;
  std::array<float, 4> _hitFlash{};
  std::array<float, 4> _tilt{};  // -1 climbing, +1 diving, per player
  std::array<float, 4> _lastY{};
  std::array<float, 4> _still{}; // seconds since the ship last moved vertically
  std::vector<rtype::protocol::PlayerState> _previousPlayers;
  rtype::protocol::GameStatus _previousStatus;
  std::uint8_t _theme = 0;
};

} // namespace client
