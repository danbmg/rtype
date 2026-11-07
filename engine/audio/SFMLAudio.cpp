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

namespace engine::audio {

void SFMLAudio::loadMusic(const std::string& id, const std::filesystem::path& path) {
  if (!std::filesystem::is_regular_file(path)) {
    throw std::runtime_error("Could not find music: " + path.string());
  }
  _tracks[id] = path;
}

void SFMLAudio::playMusic(const std::string& id) {
  if (id == _currentTrack && _music.getStatus() == sf::SoundSource::Status::Playing) {
    return;
  }
  const auto track = _tracks.find(id);
  if (track == _tracks.end() || !_music.openFromFile(track->second)) {
    return; // a missing track leaves the game silent rather than stopping it
  }
  _currentTrack = id;
  _music.setLooping(true);
  _music.setVolume(_musicVolume);
  _music.play();
}

void SFMLAudio::stopMusic() {
  _music.stop();
  _currentTrack.clear();
}

void SFMLAudio::setMusicVolume(float volume) {
  _musicVolume = std::clamp(volume, 0.0F, 100.0F);
  _music.setVolume(_musicVolume);
}

} // namespace engine::audio
