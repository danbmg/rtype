#include "engine/audio/SFMLAudio.hpp"

#include <algorithm>
#include <stdexcept>

namespace engine::audio {

void SFMLAudio::load(const std::string& id, const std::filesystem::path& path) {
  if (_buffers.contains(id)) {
    return;
  }
  if (!_buffers[id].loadFromFile(path)) {
    _buffers.erase(id);
    throw std::runtime_error("Could not load sound: " + path.string());
  }
}

void SFMLAudio::play(const std::string& id) {
  std::erase_if(_sounds, [](const auto& sound) {
    return sound->getStatus() == sf::SoundSource::Status::Stopped;
  });
  if (_volume == 0.0F || _sounds.size() >= 16) {
    return;
  }
  auto sound = std::make_unique<sf::Sound>(_buffers.at(id));
  sound->setVolume(_volume);
  sound->play();
  _sounds.push_back(std::move(sound));
}

void SFMLAudio::setVolume(float volume) {
  _volume = std::clamp(volume, 0.0F, 100.0F);
  for (auto& sound : _sounds) {
    sound->setVolume(_volume);
  }
}

} // namespace engine::audio
