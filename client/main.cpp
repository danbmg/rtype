#include <algorithm>
#include <cmath>
#include <exception>
#include <string_view>
#include <vector>

#include "client/Menu.hpp"
#include "client/Options.hpp"
#include "client/Scene.hpp"
#include "client/Settings.hpp"
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
    "Usage: r-type_client [-h host] [-p port] [--assets directory] [--mode story|endless]\n"
    "                     [--difficulty easy|normal|hard] [--join] [--mute] [--help]\n"
    "Menu: UP/DOWN choose, LEFT/RIGHT change, ENTER join or rebind keys, ESCAPE quit\n"
    "In game: bound keys move and fire | F2 auto-fire | M mute | +/- volume | F1 larger text |\n"
    "         ENTER continue when out | ESCAPE back to the menu";

bool outOfLives(const client::net::NetworkClient& network) {
  for (const auto& player : network.players()) {
    if (player.playerId == network.playerId()) {
      return player.lives == 0;
    }
  }
  return false;
}

void applyVolumes(engine::audio::IAudio& audio, const client::Settings& settings) {
  audio.setVolume(settings.muted ? 0.0F : static_cast<float>(settings.effectsVolume));
  audio.setMusicVolume(settings.muted ? 0.0F : static_cast<float>(settings.musicVolume));
}

std::string connectionStatus(const client::net::NetworkClient& network) {
  if (network.state() == ConnectionState::Connecting) {
    return "Connecting...";
  }
  return network.error();
}

void run(const client::Options& options, engine::render::IRenderer& renderer,
         engine::input::IInput& input, engine::audio::IAudio& audio) {
  client::net::NetworkClient network;
  client::Scene scene;
  client::Menu menu;
  client::Settings settings;
  settings.mode = options.mode;
  settings.difficulty = options.difficulty;
  settings.muted = options.mute;
  scene.load(renderer, audio, options.assets);
  engine::core::Clock clock;
  float elapsed = 0.0F; // menu animations
  const auto address = options.host.substr(0, 38) + ":" + std::to_string(options.port);
  const auto join = [&] {
    network.disconnect();
    (void)network.pollEvents();
    scene.reset();
    network.connect(options.host, options.port, settings.mode, settings.difficulty);
  };
  if (options.join) {
    join();
  }

  while (renderer.isOpen()) {
    // A long pause must not make visual effects jump several screens.
    const float seconds = static_cast<float>(std::min(clock.seconds(), 0.1));
    clock.reset();
    elapsed = std::fmod(elapsed + seconds, 3600.0F);
    renderer.pollEvents();
    if (!renderer.isOpen()) {
      break;
    }
    input.update(renderer.hasFocus());
    network.update();
    const bool inGame = network.state() == ConnectionState::Connected ||
                        network.state() == ConnectionState::Connecting;

    if (!inGame) {
      switch (menu.update(input, settings, audio)) {
      case client::Menu::Result::Quit: renderer.close(); continue;
      case client::Menu::Result::Join: join(); break;
      case client::Menu::Result::None: break;
      }
    } else {
      if (input.pressed(Key::Escape)) {
        network.disconnect();
        scene.reset();
      }
      if (input.pressed(Key::Enter) && outOfLives(network)) {
        join(); // continue: a fresh ship in the game in progress
      }
      if (input.pressed(Key::F2)) {
        settings.autoFire = !settings.autoFire;
      }
      if (input.pressed(Key::M)) {
        settings.muted = !settings.muted;
      }
      if (input.pressed(Key::F1)) {
        settings.largeText = !settings.largeText;
      }
      if (input.pressed(Key::Add) || input.pressed(Key::Subtract)) {
        const int step = input.pressed(Key::Add) ? 10 : -10;
        settings.effectsVolume = std::clamp(settings.effectsVolume + step, 0, 100);
        settings.musicVolume = std::clamp(settings.musicVolume + step, 0, 100);
        settings.muted = false;
      }
    }
    applyVolumes(audio, settings);

    if (network.isConnected()) {
      const auto held = [&](client::Action action) {
        return input.down(settings.keys[static_cast<std::size_t>(action)]);
      };
      network.sendInput({held(client::Action::Up), held(client::Action::Down),
                         held(client::Action::Left), held(client::Action::Right),
                         settings.autoFire || held(client::Action::Fire)});
    }
    scene.handleEvents(network, audio);
    scene.update(seconds);
    scene.updateMusic(network, audio);

    renderer.beginFrame(scene.backgroundColor());
    scene.drawBackground(renderer);
    if (network.isConnected()) {
      scene.drawGame(renderer, network, settings);
    } else {
      menu.draw(renderer, settings, address, connectionStatus(network), elapsed);
    }
    scene.drawCaptions(renderer, settings);
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
    engine::render::SFMLRenderer renderer("R-Type: Save the Summoner's Cup", 960, 816, 800, 680,
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
