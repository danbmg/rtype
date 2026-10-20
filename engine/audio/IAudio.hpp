#pragma once

#include <filesystem>
#include <string>

namespace engine::audio {

class IAudio {
public:
  virtual ~IAudio() = default;
  virtual void load(const std::string& id, const std::filesystem::path& path) = 0;
  virtual void play(const std::string& id) = 0;
  virtual void setVolume(float volume) = 0;
};

} // namespace engine::audio
