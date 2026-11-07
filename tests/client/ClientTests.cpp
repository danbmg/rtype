#include <array>
#include <optional>
#include <stdexcept>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "client/Menu.hpp"
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

// Keys pressed for one menu update, without a keyboard.
class ScriptedInput final : public engine::input::IInput {
public:
  void update(bool) override {}
  bool down(engine::input::Key key) const override { return key == _key; }
  bool pressed(engine::input::Key key) const override { return key == _key; }
  std::optional<engine::input::Key> anyPressed() const override { return _key; }
  void press(std::optional<engine::input::Key> key) { _key = key; }

private:
  std::optional<engine::input::Key> _key;
};

class SilentAudio final : public engine::audio::IAudio {
public:
  void load(const std::string&, const std::filesystem::path&) override {}
  void play(const std::string&) override { ++played; }
  void setVolume(float) override {}
  void loadMusic(const std::string&, const std::filesystem::path&) override {}
  void playMusic(const std::string&) override {}
  void stopMusic() override {}
  void setMusicVolume(float) override {}
  int played = 0;
};

client::Menu::Result press(client::Menu& menu, client::Settings& settings, engine::input::Key key) {
  ScriptedInput input;
  SilentAudio audio;
  input.press(key);
  return menu.update(input, settings, audio);
}

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

TEST_CASE("Client options choose the game mode and difficulty", "[client][options]") {
  const std::array<std::string_view, 5> args{"--mode", "endless", "--difficulty", "hard", "--join"};
  const auto options = client::parseOptions(args, "r-type_client");
  CHECK(options.mode == rtype::protocol::GameMode::Endless);
  CHECK(options.difficulty == rtype::protocol::Difficulty::Hard);
  CHECK(options.join);
  for (const auto& wrong : {std::array<std::string_view, 2>{"--mode", "versus"},
                            std::array<std::string_view, 2>{"--difficulty", "insane"}}) {
    CHECK_THROWS_AS(client::parseOptions(wrong, "r-type_client"), std::invalid_argument);
  }
  const std::array<std::string_view, 1> missing{"--mode"};
  CHECK_THROWS_AS(client::parseOptions(missing, "r-type_client"), std::invalid_argument);
}

TEST_CASE("The menu changes the mode and difficulty sent to the server", "[client][menu]") {
  client::Menu menu;
  client::Settings settings;
  REQUIRE(menu.selected() == client::Menu::Item::Join);
  while (menu.selected() != client::Menu::Item::Mode) {
    press(menu, settings, engine::input::Key::Up);
  }
  press(menu, settings, engine::input::Key::Right);
  CHECK(settings.mode == rtype::protocol::GameMode::Endless);
  press(menu, settings, engine::input::Key::Down);
  press(menu, settings, engine::input::Key::Left);
  CHECK(settings.difficulty == rtype::protocol::Difficulty::Easy);
  press(menu, settings, engine::input::Key::Left); // already the easiest
  CHECK(settings.difficulty == rtype::protocol::Difficulty::Easy);
  CHECK(client::valueOf(client::Menu::Item::Difficulty, settings) == "EASY");
  while (menu.selected() != client::Menu::Item::Join) {
    press(menu, settings, engine::input::Key::Down);
  }
  CHECK(press(menu, settings, engine::input::Key::Enter) == client::Menu::Result::Join);
  CHECK(press(menu, settings, engine::input::Key::Escape) == client::Menu::Result::Quit);
}

TEST_CASE("The tutorial difficulty is offered in story mode only", "[client][menu][tutorial]") {
  using engine::input::Key;
  client::Menu menu;
  client::Settings settings;
  REQUIRE(settings.mode == rtype::protocol::GameMode::Story);
  while (menu.selected() != client::Menu::Item::Difficulty) {
    press(menu, settings, Key::Up);
  }
  for (int step = 0; step < 4; ++step) {
    press(menu, settings, Key::Right);
  }
  CHECK(settings.difficulty == rtype::protocol::Difficulty::Tutorial); // the last one
  CHECK(client::valueOf(client::Menu::Item::Difficulty, settings) == "TUTORIAL");

  // Switching to endless leaves the tutorial, which endless cannot reach.
  press(menu, settings, Key::Up);
  press(menu, settings, Key::Right);
  REQUIRE(settings.mode == rtype::protocol::GameMode::Endless);
  CHECK(settings.difficulty == rtype::protocol::Difficulty::Normal);
  press(menu, settings, Key::Down);
  for (int step = 0; step < 4; ++step) {
    press(menu, settings, Key::Right);
  }
  CHECK(settings.difficulty == rtype::protocol::Difficulty::Hard);

  const std::array<std::string_view, 2> args{"--difficulty", "tutorial"};
  CHECK(client::parseOptions(args, "r-type_client").difficulty ==
        rtype::protocol::Difficulty::Tutorial);
}

TEST_CASE("Every action can be bound to any key", "[client][menu][accessibility]") {
  using engine::input::Key;
  client::Menu menu;
  client::Settings settings;
  while (menu.selected() != client::Menu::Item::Controls) {
    press(menu, settings, Key::Up);
  }
  press(menu, settings, Key::Right);
  CHECK(std::string(client::layoutName(settings.keys)) == "WASD");
  press(menu, settings, Key::Enter);
  REQUIRE(menu.rebinding());
  for (const auto key : {Key::I, Key::K, Key::J, Key::L, Key::LShift}) {
    press(menu, settings, key);
  }
  CHECK_FALSE(menu.rebinding());
  CHECK(settings.keys == client::KeyBindings{Key::I, Key::K, Key::J, Key::L, Key::LShift});
  CHECK(std::string(client::layoutName(settings.keys)) == "CUSTOM");
  CHECK(client::valueOf(client::Menu::Item::Controls, settings) == "CUSTOM: I K J L LSHIFT");

  // Escape stops in the middle and keeps what was already bound.
  press(menu, settings, Key::Enter);
  press(menu, settings, Key::Up);
  press(menu, settings, Key::Escape);
  CHECK_FALSE(menu.rebinding());
  CHECK(settings.keys[0] == Key::Up);
  CHECK(settings.keys[4] == Key::LShift);
  CHECK(engine::input::keyName(Key::Space) == "SPACE");
}

TEST_CASE("Accessibility options toggle from the menu", "[client][menu][accessibility]") {
  client::Menu menu;
  client::Settings settings;
  const auto select = [&](client::Menu::Item item) {
    while (menu.selected() != item) {
      press(menu, settings, engine::input::Key::Down);
    }
  };
  select(client::Menu::Item::AutoFire);
  press(menu, settings, engine::input::Key::Enter);
  CHECK(settings.autoFire);
  select(client::Menu::Item::Music);
  for (int step = 0; step < 12; ++step) {
    press(menu, settings, engine::input::Key::Left);
  }
  CHECK(settings.musicVolume == 0);
  select(client::Menu::Item::Shake);
  press(menu, settings, engine::input::Key::Right);
  CHECK_FALSE(settings.screenShake);
  select(client::Menu::Item::Captions);
  press(menu, settings, engine::input::Key::Enter);
  CHECK_FALSE(settings.captions);
}
