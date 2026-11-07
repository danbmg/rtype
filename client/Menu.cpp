#include "client/Menu.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace client {

namespace {

using engine::input::Key;
using engine::render::Color;
namespace protocol = rtype::protocol;

constexpr std::size_t ITEM_COUNT = static_cast<std::size_t>(Menu::Item::Count);
constexpr std::array<const char*, ITEM_COUNT> LABELS{
    "GAME MODE",    "DIFFICULTY", "CONTROLS",      "AUTO-FIRE",  "MUSIC VOLUME",
    "SOUND VOLUME", "TEXT SIZE",  "SOUND CAPTIONS", "SCREEN SHAKE", "JOIN THE GAME"};
constexpr std::array<const char*, ITEM_COUNT> HELP{
    "Story: 10 levels to save the Cup. Endless: waves forever. The first player chooses.",
    "Easy: 5 lives, weaker and slower enemies. Hard: 2 lives, tougher enemies.",
    "LEFT/RIGHT: arrows, WASD or ZQSD. ENTER: bind each action to any key.",
    "Your ship fires on its own: no key to hold.",
    "Volume of the music, 0 to 100.",
    "Volume of the sound effects, 0 to 100.",
    "Larger text in the HUD and the menus.",
    "Important sounds are also written on screen.",
    "Turn off the screen shake of explosions (motion sensitivity).",
    "ENTER to connect. Players who join later play the game in progress."};

bool pressedAny(const engine::input::IInput& input, std::initializer_list<Key> keys) {
  return std::ranges::any_of(keys, [&input](Key key) { return input.pressed(key); });
}

int step(int value, int direction) { return std::clamp(value + direction * 10, 0, 100); }

} // namespace

std::string valueOf(Menu::Item item, const Settings& settings) {
  switch (item) {
  case Menu::Item::Mode: return settings.mode == protocol::GameMode::Story ? "STORY" : "ENDLESS";
  case Menu::Item::Difficulty: {
    constexpr std::array NAMES{"EASY", "NORMAL", "HARD"};
    return NAMES.at(static_cast<std::size_t>(settings.difficulty));
  }
  case Menu::Item::Controls: {
    std::string text = layoutName(settings.keys);
    if (text == "CUSTOM") {
      text += ":";
      for (const auto key : settings.keys) {
        text += " " + std::string(engine::input::keyName(key));
      }
    }
    return text;
  }
  case Menu::Item::AutoFire: return settings.autoFire ? "ON" : "OFF";
  case Menu::Item::Music: return std::to_string(settings.musicVolume);
  case Menu::Item::Effects: return std::to_string(settings.effectsVolume);
  case Menu::Item::TextSize: return settings.largeText ? "LARGE" : "NORMAL";
  case Menu::Item::Captions: return settings.captions ? "ON" : "OFF";
  case Menu::Item::Shake: return settings.screenShake ? "ON" : "OFF";
  case Menu::Item::Join:
  case Menu::Item::Count: break;
  }
  return "";
}

void Menu::change(Settings& settings, int direction) {
  switch (selected()) {
  case Item::Mode:
    settings.mode = settings.mode == protocol::GameMode::Story ? protocol::GameMode::Endless
                                                               : protocol::GameMode::Story;
    break;
  case Item::Difficulty: {
    const int next = std::clamp(static_cast<int>(settings.difficulty) + direction, 0, 2);
    settings.difficulty = static_cast<protocol::Difficulty>(next);
    break;
  }
  case Item::Controls: {
    // Cycle through the ready-made layouts; a custom one goes back to the first.
    std::size_t current = LAYOUTS.size();
    for (std::size_t index = 0; index < LAYOUTS.size(); ++index) {
      if (LAYOUTS[index].keys == settings.keys) {
        current = index;
      }
    }
    const auto count = static_cast<int>(LAYOUTS.size());
    const int next = current == LAYOUTS.size()
                         ? 0
                         : (static_cast<int>(current) + direction + count) % count;
    settings.keys = LAYOUTS[static_cast<std::size_t>(next)].keys;
    break;
  }
  case Item::AutoFire: settings.autoFire = !settings.autoFire; break;
  case Item::Music: settings.musicVolume = step(settings.musicVolume, direction); break;
  case Item::Effects: settings.effectsVolume = step(settings.effectsVolume, direction); break;
  case Item::TextSize: settings.largeText = !settings.largeText; break;
  case Item::Captions: settings.captions = !settings.captions; break;
  case Item::Shake: settings.screenShake = !settings.screenShake; break;
  case Item::Join:
  case Item::Count: break;
  }
}

Menu::Result Menu::update(const engine::input::IInput& input, Settings& settings,
                          engine::audio::IAudio& audio) {
  if (_rebinding) {
    // Waiting for a key: Escape cancels, any other key is bound to the action.
    if (input.pressed(Key::Escape)) {
      _rebinding.reset();
    } else if (const auto key = input.anyPressed()) {
      settings.keys[*_rebinding] = *key;
      audio.play("select");
      _rebinding = *_rebinding + 1 < ACTION_COUNT ? std::optional(*_rebinding + 1) : std::nullopt;
    }
    return Result::None;
  }
  if (input.pressed(Key::Escape)) {
    return Result::Quit;
  }
  const auto& keys = settings.keys;
  const auto bound = [&keys](Action action) { return keys[static_cast<std::size_t>(action)]; };
  if (pressedAny(input, {Key::Up, bound(Action::Up)})) {
    _selected = (_selected + ITEM_COUNT - 1) % ITEM_COUNT;
    audio.play("select");
  } else if (pressedAny(input, {Key::Down, bound(Action::Down)})) {
    _selected = (_selected + 1) % ITEM_COUNT;
    audio.play("select");
  } else if (pressedAny(input, {Key::Left, bound(Action::Left)})) {
    change(settings, -1);
    audio.play("select");
  } else if (pressedAny(input, {Key::Right, bound(Action::Right)})) {
    change(settings, 1);
    audio.play("select");
  } else if (input.pressed(Key::Enter)) {
    if (selected() == Item::Join) {
      return Result::Join;
    }
    if (selected() == Item::Controls) {
      _rebinding = 0;
    } else {
      change(settings, 1);
    }
    audio.play("select");
  }
  return Result::None;
}

void Menu::draw(engine::render::IRenderer& renderer, const Settings& settings,
                const std::string& address, const std::string& status, float time) const {
  const unsigned int text = settings.largeText ? 22U : 18U;
  renderer.drawRectangle(60, 70, 680, 580, {10, 14, 30, 235});
  renderer.drawRectangle(60, 70, 680, 4, {235, 45, 60});
  // Title: the four team colours under the emblem.
  renderer.drawSprite("t1", {0, 0, 120, 48}, 90, 92, 90, 36, {235, 45, 60});
  renderer.drawText("R-TYPE", 200, 82, 44, {100, 220, 255});
  renderer.drawText("SAVE THE SUMMONER'S CUP", 202, 132, 20, {255, 215, 90});
  const float bob = std::sin(time * 2.0F) * 3.0F;
  renderer.drawSprite("trophy", {0, 0, 48, 48}, 640, 86 + bob, 64, 64);

  for (std::size_t index = 0; index < ITEM_COUNT; ++index) {
    const float y = 178.0F + static_cast<float>(index) * 36.0F;
    const bool current = index == _selected;
    if (current) {
      renderer.drawRectangle(80, y - 4, 640, 32, {40, 60, 110, 230});
      renderer.drawRectangle(80, y - 4, 4, 32, {255, 215, 90});
    }
    const Color colour = current ? Color{255, 255, 255} : Color{170, 185, 210};
    const auto item = static_cast<Item>(index);
    if (item == Item::Join) {
      renderer.drawText(current ? "> JOIN THE GAME <" : "JOIN THE GAME", 300, y, text + 2,
                        current ? Color{120, 255, 140} : colour);
      continue;
    }
    renderer.drawText(LABELS.at(index), 100, y, text, colour);
    const auto value = valueOf(item, settings);
    renderer.drawText(current ? "< " + value + " >" : value, 380, y, text, colour);
  }

  const float helpY = 548.0F;
  if (_rebinding) {
    renderer.drawRectangle(80, helpY - 6, 640, 34, {120, 40, 40, 230});
    renderer.drawText(std::string("Press the key for ") + ACTION_NAMES.at(*_rebinding) +
                          "   (ESCAPE: stop)",
                      100, helpY, text, {255, 230, 120});
  } else {
    renderer.drawText(HELP.at(_selected), 90, helpY, settings.largeText ? 16U : 14U,
                      {200, 210, 230});
  }
  renderer.drawText("Server " + address, 90, 586, 16, {150, 160, 190});
  if (!status.empty()) {
    renderer.drawText(status.substr(0, 60), 90, 610, 16, {255, 200, 90});
  }
}

} // namespace client
