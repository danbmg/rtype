#include "engine/render/SFMLRenderer.hpp"

#include <stdexcept>

namespace engine::render {

namespace {
sf::Color nativeColor(Color color) { return {color.r, color.g, color.b, color.a}; }
} // namespace

SFMLRenderer::SFMLRenderer(const std::string& title, unsigned int width, unsigned int height,
                           float canvasWidth, float canvasHeight, const std::filesystem::path& font)
    : _window(sf::VideoMode({width, height}), title), _font(font),
      _view(sf::FloatRect({0.0F, 0.0F}, {canvasWidth, canvasHeight})) {
  _window.setFramerateLimit(60);
  resize(width, height);
}

bool SFMLRenderer::isOpen() const { return _window.isOpen(); }
bool SFMLRenderer::hasFocus() const { return _window.hasFocus(); }
void SFMLRenderer::close() { _window.close(); }

void SFMLRenderer::resize(unsigned int width, unsigned int height) {
  if (width == 0 || height == 0) {
    return;
  }
  const float ratio = static_cast<float>(width) / static_cast<float>(height);
  const float canvasRatio = _view.getSize().x / _view.getSize().y;
  sf::FloatRect viewport({0.0F, 0.0F}, {1.0F, 1.0F});
  if (ratio > canvasRatio) {
    viewport.size.x = canvasRatio / ratio;
    viewport.position.x = (1.0F - viewport.size.x) / 2.0F;
  } else {
    viewport.size.y = ratio / canvasRatio;
    viewport.position.y = (1.0F - viewport.size.y) / 2.0F;
  }
  _view.setViewport(viewport);
  _window.setView(_view);
}

void SFMLRenderer::pollEvents() {
  while (const auto event = _window.pollEvent()) {
    if (event->is<sf::Event::Closed>()) {
      close();
    } else if (const auto* resized = event->getIf<sf::Event::Resized>()) {
      resize(resized->size.x, resized->size.y);
    }
  }
}

void SFMLRenderer::loadTexture(const std::string& id, const std::filesystem::path& path,
                               bool maskBackground) {
  sf::Image image(path);
  if (maskBackground) {
    image.createMaskFromColor(image.getPixel({0, 0}));
  }
  auto [entry, inserted] = _textures.try_emplace(id);
  (void)inserted;
  if (!entry->second.loadFromImage(image)) {
    throw std::runtime_error("Could not load texture: " + path.string());
  }
  entry->second.setSmooth(false);
}

void SFMLRenderer::beginFrame(Color background) { _window.clear(nativeColor(background)); }

void SFMLRenderer::drawSprite(const std::string& id, Rect frame, float x, float y, float width,
                              float height, Color tint) {
  sf::Sprite sprite(_textures.at(id), sf::IntRect({frame.x, frame.y}, {frame.width, frame.height}));
  sprite.setPosition({x, y});
  sprite.setScale(
      {width / static_cast<float>(frame.width), height / static_cast<float>(frame.height)});
  sprite.setColor(nativeColor(tint));
  _window.draw(sprite);
}

void SFMLRenderer::drawRectangle(float x, float y, float width, float height, Color color) {
  sf::RectangleShape rectangle({width, height});
  rectangle.setPosition({x, y});
  rectangle.setFillColor(nativeColor(color));
  _window.draw(rectangle);
}

void SFMLRenderer::drawText(const std::string& text, float x, float y, unsigned int size,
                            Color color) {
  sf::Text label(_font, text, size);
  label.setPosition({x, y});
  label.setFillColor(nativeColor(color));
  _window.draw(label);
}

void SFMLRenderer::endFrame() { _window.display(); }

} // namespace engine::render
