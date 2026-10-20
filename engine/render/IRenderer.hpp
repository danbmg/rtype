#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

namespace engine::render {

struct Color {
  std::uint8_t r = 255;
  std::uint8_t g = 255;
  std::uint8_t b = 255;
  std::uint8_t a = 255;
};

struct Rect {
  int x;
  int y;
  int width;
  int height;
};

// All coordinates are in the logical canvas, independent of the window size.
class IRenderer {
public:
  virtual ~IRenderer() = default;
  virtual bool isOpen() const = 0;
  virtual bool hasFocus() const = 0;
  virtual void pollEvents() = 0;
  virtual void close() = 0;
  virtual void loadTexture(const std::string& id, const std::filesystem::path& path,
                           bool maskBackground) = 0;
  virtual void beginFrame(Color background) = 0;
  virtual void drawSprite(const std::string& id, Rect frame, float x, float y, float width,
                          float height, Color tint = {}) = 0;
  virtual void drawRectangle(float x, float y, float width, float height, Color color) = 0;
  virtual void drawText(const std::string& text, float x, float y, unsigned int size,
                        Color color = {}) = 0;
  virtual void endFrame() = 0;
};

} // namespace engine::render
