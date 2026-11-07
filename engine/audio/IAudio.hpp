#pragma once

#include <filesystem>
#include <string>

namespace engine::audio {

// Sound effects are loaded in memory and played by id; music tracks are streamed from their
// file and loop. Volumes go from 0 (silent) to 100, separately for effects and music.
class IAudio {
public:
  virtual ~IAudio() = default;
  virtual void load(const std::string& id, const std::filesystem::path& path) = 0;
  virtual void play(const std::string& id) = 0;
  virtual void setVolume(float volume) = 0;

  // Throws std::runtime_error if the file does not exist.
  virtual void loadMusic(const std::string& id, const std::filesystem::path& path) = 0;
  // Starts that track in a loop; does nothing if it is already playing.
  virtual void playMusic(const std::string& id) = 0;
  virtual void stopMusic() = 0;
  virtual void setMusicVolume(float volume) = 0;
};

} // namespace engine::audio
