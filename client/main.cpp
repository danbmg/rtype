#include <algorithm>
#include <array>
#include <exception>
#include <string_view>
#include <vector>

#include "client/Options.hpp"
#include "client/Scene.hpp"
#include "engine/audio/SFMLAudio.hpp"
#include "engine/core/Clock.hpp"
#include "engine/core/Logger.hpp"
#include "engine/input/SFMLInput.hpp"
#include "engine/render/SFMLRenderer.hpp"

namespace {

using client::net::ConnectionState;
using engine::core::Logger;
using engine::input::Key;

constexpr std::string_view USAGE =
    "Usage: r-type_client [-h host] [-p port] [--assets directory] [--mute] [--help]\n"
    "Enter: connect/retry | arrows: move | Space: fire | Escape: leave/quit\n"
    "Tab: arrows/WASD/ZQSD | M: mute | +/-: volume | F1: larger text";

bool gameOver(const client::net::NetworkClient& network) {
  for (const auto& player : network.players()) {
    if (player.playerId == network.playerId()) {
      return player.lives == 0;
    }
  }
  return false;
}

void run(const client::Options& options, engine::render::IRenderer& renderer,
         engine::input::IInput& input, engine::audio::IAudio& audio) {
  client::net::NetworkClient network;
  client::Scene scene;
  scene.load(renderer, audio, options.assets);
  engine::core::Clock clock;
  std::size_t layout = 0;
  bool largeText = false;
  bool muted = options.mute;
  float volume = 50.0F;
  audio.setVolume(muted ? 0.0F : volume);
  constexpr std::array<std::array<Key, 4>, 3> BINDINGS{{{Key::Up, Key::Down, Key::Left, Key::Right},
                                                        {Key::W, Key::S, Key::A, Key::D},
                                                        {Key::Z, Key::S, Key::Q, Key::D}}};
  constexpr std::array LAYOUT_NAMES{"ARROWS", "WASD", "ZQSD"};

  while (renderer.isOpen()) {
    // A long pause must not make visual effects jump several screens.
    const float seconds = static_cast<float>(std::min(clock.seconds(), 0.1));
    clock.reset();
    renderer.pollEvents();
    if (!renderer.isOpen()) {
      break;
    }
    input.update(renderer.hasFocus());
    network.update();

    if (input.pressed(Key::Escape)) {
      if (network.state() != ConnectionState::Connected &&
          network.state() != ConnectionState::Connecting) {
        renderer.close();
        break;
      }
      network.disconnect();
      scene.reset();
    }
    if (input.pressed(Key::Enter) &&
        (network.state() != ConnectionState::Connected || gameOver(network))) {
      network.disconnect();
      (void)network.pollEvents();
      scene.reset();
      network.connect(options.host, options.port);
    }
    if (input.pressed(Key::Tab)) {
      layout = (layout + 1) % BINDINGS.size();
    }
    if (input.pressed(Key::M)) {
      muted = !muted;
    }
    if (input.pressed(Key::F1)) {
      largeText = !largeText;
    }
    if (input.pressed(Key::Add) || input.pressed(Key::Subtract)) {
      volume = std::clamp(volume + (input.pressed(Key::Add) ? 10.0F : -10.0F), 0.0F, 100.0F);
      muted = false;
    }
    audio.setVolume(muted ? 0.0F : volume);

    if (network.isConnected()) {
      const auto& keys = BINDINGS[layout];
      network.sendInput({input.down(keys[0]), input.down(keys[1]), input.down(keys[2]),
                         input.down(keys[3]), input.down(Key::Space)});
    }
    scene.handleEvents(network, audio);
    scene.update(seconds);
    renderer.beginFrame({4, 8, 18});
    scene.drawBackground(renderer);
    if (network.isConnected()) {
      scene.drawGame(renderer, network, largeText);
    } else {
      renderer.drawRectangle(70, 115, 660, 420, {12, 18, 32, 240});
      renderer.drawText("R-TYPE", 275, 140, 52, {100, 220, 255});
      renderer.drawText("Cooperate. Dodge. Shoot.", 235, 210, 24);
      // Keep arbitrary host names inside the panel.
      const auto address = options.host.substr(0, 38) + ":" + std::to_string(options.port);
      renderer.drawText(address, 120, 265, largeText ? 24U : 20U);
      const bool connecting = network.state() == ConnectionState::Connecting;
      renderer.drawText(connecting ? "Connecting..." : "ENTER - join the game", 210, 325, 28);
      if (!network.error().empty()) {
        renderer.drawText(network.error().substr(0, 52), 110, 375, 20, {255, 200, 90});
      }
      renderer.drawText("Move: arrows / WASD / ZQSD    Shoot: SPACE", 115, 425, 20);
      renderer.drawText("TAB: controls    M: mute    F1: larger HUD", 125, 465, 20);
      renderer.drawText("ESC: quit", 335, 505, 18);
    }
    renderer.drawRectangle(0, 656, 800, 24, {12, 18, 32});
    const std::string footer =
        std::string(LAYOUT_NAMES[layout]) + " [TAB]   FIRE [SPACE]   VOLUME " +
        std::to_string(muted ? 0 : static_cast<int>(volume)) + " [+/- M]   TEXT [F1]   LEAVE [ESC]";
    renderer.drawText(footer, 10, 657, largeText ? 16U : 14U);
    renderer.endFrame();
  }
  network.disconnect();
}

} // namespace

int main(int argc, char** argv) {
  try {
    std::vector<std::string_view> arguments;
    for (int index = 1; index < argc; ++index) {
      arguments.emplace_back(argv[index]);
    }
    const auto options = client::parseOptions(arguments, argv[0]);
    if (options.help) {
      Logger::info(std::string(USAGE));
      return 0;
    }
    engine::render::SFMLRenderer renderer("R-Type", 960, 816, 800, 680,
                                          options.assets / "DejaVuSans.ttf");
    engine::input::SFMLInput input;
    engine::audio::SFMLAudio audio;
    run(options, renderer, input, audio);
    return 0;
  } catch (const std::exception& error) {
    Logger::error(error.what());
    Logger::info(std::string(USAGE));
    return 84;
  }
}
