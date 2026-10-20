#include <array>
#include <stdexcept>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "client/Options.hpp"
#include "client/Scene.hpp"

namespace {

// Test time-based presentation without creating an OS window or an audio device.
class RecordingRenderer final : public engine::render::IRenderer {
public:
  bool isOpen() const override { return true; }
  bool hasFocus() const override { return true; }
  void pollEvents() override {}
  void close() override {}
  void loadTexture(const std::string&, const std::filesystem::path&, bool) override {}
  void beginFrame(engine::render::Color) override {}
  void drawSprite(const std::string&, engine::render::Rect, float, float, float, float,
                  engine::render::Color) override {}
  void drawRectangle(float x, float y, float, float, engine::render::Color) override {
    positions.push_back({x, y});
  }
  void drawText(const std::string&, float, float, unsigned int, engine::render::Color) override {}
  void endFrame() override {}
  std::vector<std::array<float, 2>> positions;
};

} // namespace

TEST_CASE("Client rejects invalid ports and incomplete command lines", "[client][options]") {
  for (const auto port : {"0", "-1", "65536", "42oops", "999999999999999999999"}) {
    const std::array<std::string_view, 2> args{"-p", port};
    REQUIRE_THROWS_AS(client::parseOptions(args, "r-type_client"), std::invalid_argument);
  }
  const std::array<std::string_view, 1> missing{"-h"};
  REQUIRE_THROWS_AS(client::parseOptions(missing, "r-type_client"), std::invalid_argument);
  const std::array<std::string_view, 1> unknown{"--unknown"};
  REQUIRE_THROWS_AS(client::parseOptions(unknown, "r-type_client"), std::invalid_argument);
}

TEST_CASE("Client accepts server address and explicit asset directory", "[client][options]") {
  const std::array<std::string_view, 7> args{"-h",       "game.example",    "-p",    "65535",
                                             "--assets", "portable assets", "--mute"};
  const auto options = client::parseOptions(args, "r-type_client");
  REQUIRE(options.host == "game.example");
  REQUIRE(options.port == 65535);
  REQUIRE(options.assets == "portable assets");
  REQUIRE(options.mute);
}

TEST_CASE("Starfield speed is independent of render frame rate", "[client][time]") {
  client::Scene slow;
  client::Scene fast;
  for (int frame = 0; frame < 30; ++frame) {
    slow.update(1.0F / 30.0F);
  }
  for (int frame = 0; frame < 144; ++frame) {
    fast.update(1.0F / 144.0F);
  }
  RecordingRenderer slowFrame;
  RecordingRenderer fastFrame;
  slow.drawBackground(slowFrame);
  fast.drawBackground(fastFrame);
  REQUIRE_FALSE(slowFrame.positions.empty());
  REQUIRE(slowFrame.positions.size() == fastFrame.positions.size());
  for (std::size_t index = 0; index < slowFrame.positions.size(); ++index) {
    // Account for floating point accumulation over 144 frames.
    REQUIRE(slowFrame.positions[index][0] ==
            Catch::Approx(fastFrame.positions[index][0]).margin(0.025));
    REQUIRE(slowFrame.positions[index][1] == fastFrame.positions[index][1]);
  }
}
