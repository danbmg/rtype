#pragma once

#include <unordered_map>

#include <SFML/Graphics.hpp>

#include "engine/render/IRenderer.hpp"

namespace engine::render {

class SFMLRenderer final : public IRenderer {
public:
  SFMLRenderer(const std::string& title, unsigned int width, unsigned int height, float canvasWidth,
               float canvasHeight, const std::filesystem::path& font);
  bool isOpen() const override;
  bool hasFocus() const override;
  void pollEvents() override;
  void close() override;
  void loadTexture(const std::string& id, const std::filesystem::path& path,
                   bool maskBackground) override;
  void beginFrame(Color background) override;
  void drawSprite(const std::string& id, Rect frame, float x, float y, float width, float height,
                  Color tint = {}) override;
  void drawRectangle(float x, float y, float width, float height, Color color) override;
  void drawText(const std::string& text, float x, float y, unsigned int size,
                Color color = {}) override;
  void endFrame() override;

private:
  void resize(unsigned int width, unsigned int height);

  sf::RenderWindow _window;
  sf::Font _font;
  sf::View _view;
  std::unordered_map<std::string, sf::Texture> _textures;
};

} // namespace engine::render
