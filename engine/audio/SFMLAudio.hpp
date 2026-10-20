#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include <SFML/Audio.hpp>

#include "engine/audio/IAudio.hpp"

namespace engine::audio {

class SFMLAudio final : public IAudio {
public:
  void load(const std::string& id, const std::filesystem::path& path) override;
  void play(const std::string& id) override;
  void setVolume(float volume) override;

private:
  // Buffers outlive sounds, which hold references to them.
  std::unordered_map<std::string, sf::SoundBuffer> _buffers;
  std::vector<std::unique_ptr<sf::Sound>> _sounds;
  float _volume = 50.0F;
};

} // namespace engine::audio
