#pragma once

#include <array>
#include <string>
#include <vector>

#include "client/net/NetworkClient.hpp"
#include "engine/audio/IAudio.hpp"
#include "engine/render/IRenderer.hpp"

namespace client {

// Presentation only. This class never runs gameplay systems or changes server state.
class Scene {
public:
  Scene();
  void load(engine::render::IRenderer& renderer, engine::audio::IAudio& audio,
            const std::filesystem::path& assets);
  void update(float seconds);
  void handleEvents(net::NetworkClient& network, engine::audio::IAudio& audio);
  void drawBackground(engine::render::IRenderer& renderer) const;
  void drawGame(engine::render::IRenderer& renderer, const net::NetworkClient& network,
                bool largeText) const;
  void reset();

private:
  struct Star {
    float x;
    float y;
    float speed;
  };
  struct Explosion {
    float x;
    float y;
    float age;
  };

  std::array<Star, 100> _stars{};
  std::vector<Explosion> _explosions;
  std::string _notice;
  float _noticeRemaining = 0.0F;
  float _animationTime = 0.0F;
};

} // namespace client
