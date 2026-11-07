#include "game/Content.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <tuple>
#include <utility>

#include <nlohmann/json.hpp>

namespace game {

namespace {

using Json = nlohmann::json;
using rtype::common::components::MovementPattern;
using rtype::common::components::Visual;

// A designer edits these files by hand: a 1 MB limit is far above any real level.
constexpr std::uintmax_t MAX_FILE_SIZE = 1024 * 1024;
constexpr std::size_t MAX_NAME_LENGTH = 32;

bool isPrintable(const std::string& text) {
  return std::ranges::all_of(text, [](char character) {
    const auto byte = static_cast<unsigned char>(character);
    return byte == '\n' || (byte >= 0x20 && byte <= 0x7E);
  });
}

// Reads the fields of one JSON object and reports every mistake with its location, for example
// `enemies.json: enemies[2] "waver": "speed" must be a number between 10 and 600`.
class Fields {
public:
  Fields(const Json& object, std::string where, std::initializer_list<std::string_view> allowed)
      : _object(object), _where(std::move(where)) {
    if (!_object.is_object()) {
      throw ContentError(_where + ": expected an object { ... }");
    }
    for (const auto& [key, value] : _object.items()) {
      (void)value;
      if (std::ranges::find(allowed, std::string_view(key)) == allowed.end()) {
        std::string known;
        for (const auto name : allowed) {
          known += (known.empty() ? "" : ", ") + std::string(name);
        }
        throw ContentError(_where + ": unknown field \"" + key + "\" (allowed: " + known + ")");
      }
    }
  }

  bool has(std::string_view key) const { return _object.contains(key); }

  [[noreturn]] void fail(std::string_view key, const std::string& message) const {
    throw ContentError(_where + ": \"" + std::string(key) + "\" " + message);
  }

  double number(std::string_view key, double fallback, double minimum, double maximum) const {
    if (!has(key)) {
      return fallback;
    }
    const auto& value = _object.at(key);
    if (!value.is_number() || !std::isfinite(value.get<double>()) ||
        value.get<double>() < minimum || value.get<double>() > maximum) {
      std::ostringstream range;
      range << "must be a number between " << minimum << " and " << maximum;
      fail(key, range.str());
    }
    return value.get<double>();
  }

  float decimal(std::string_view key, float fallback, double minimum, double maximum) const {
    return static_cast<float>(number(key, fallback, minimum, maximum));
  }

  int integer(std::string_view key, int fallback, int minimum, int maximum) const {
    if (!has(key)) {
      return fallback;
    }
    const auto& value = _object.at(key);
    if (!value.is_number_integer() || value.get<long long>() < minimum ||
        value.get<long long>() > maximum) {
      fail(key, "must be a whole number between " + std::to_string(minimum) + " and " +
                    std::to_string(maximum));
    }
    return static_cast<int>(value.get<long long>());
  }

  bool boolean(std::string_view key, bool fallback) const {
    if (!has(key)) {
      return fallback;
    }
    if (!_object.at(key).is_boolean()) {
      fail(key, "must be true or false");
    }
    return _object.at(key).get<bool>();
  }

  std::string text(std::string_view key, std::size_t maximum, bool required) const {
    if (!has(key)) {
      if (required) {
        fail(key, "is required");
      }
      return {};
    }
    const auto& value = _object.at(key);
    if (!value.is_string()) {
      fail(key, "must be a text in quotes");
    }
    auto result = value.get<std::string>();
    if (result.size() > maximum) {
      fail(key, "is too long (" + std::to_string(result.size()) + " characters, at most " +
                    std::to_string(maximum) + ")");
    }
    if (!isPrintable(result)) {
      fail(key, "may only contain plain ASCII characters (no accents or emoji)");
    }
    return result;
  }

  std::string name(std::string_view key, bool required) const {
    auto result = text(key, MAX_NAME_LENGTH, required);
    const bool valid = std::ranges::all_of(result, [](char character) {
      return (character >= 'a' && character <= 'z') || (character >= '0' && character <= '9') ||
             character == '-';
    });
    if (!valid || (required && result.empty())) {
      fail(key, "must use lowercase letters, digits and '-' only, like \"heavy-drone\"");
    }
    return result;
  }

  template<typename Enum>
  Enum choice(std::string_view key, Enum fallback,
              std::initializer_list<std::pair<std::string_view, Enum>> options) const {
    if (!has(key)) {
      return fallback;
    }
    const auto& value = _object.at(key);
    std::string known;
    for (const auto& [label, option] : options) {
      if (value.is_string() && value.get<std::string>() == label) {
        return option;
      }
      known += (known.empty() ? "\"" : ", \"") + std::string(label) + "\"";
    }
    fail(key, "must be one of " + known);
  }

  const Json& array(std::string_view key, std::size_t minimum, std::size_t maximum) const {
    if (!has(key)) {
      fail(key, "is required");
    }
    const auto& value = _object.at(key);
    if (!value.is_array() || value.size() < minimum || value.size() > maximum) {
      fail(key, "must be a list [ ... ] of " + std::to_string(minimum) + " to " +
                    std::to_string(maximum) + " items");
    }
    return value;
  }

  std::pair<float, float> size(std::string_view key, std::pair<float, float> fallback,
                               float minimum, float maximum) const {
    if (!has(key)) {
      return fallback;
    }
    const auto& value = _object.at(key);
    if (!value.is_array() || value.size() != 2 || !value[0].is_number() ||
        !value[1].is_number() || value[0].get<double>() < minimum ||
        value[0].get<double>() > maximum || value[1].get<double>() < minimum ||
        value[1].get<double>() > maximum) {
      std::ostringstream message;
      message << "must be [width, height], each between " << minimum << " and " << maximum;
      fail(key, message.str());
    }
    return {value[0].get<float>(), value[1].get<float>()};
  }

private:
  const Json& _object;
  std::string _where;
};

Json readJson(const std::filesystem::path& path) {
  std::error_code error;
  const auto size = std::filesystem::file_size(path, error);
  if (error) {
    throw ContentError(path.string() + ": cannot read the file (" + error.message() + ")");
  }
  if (size > MAX_FILE_SIZE) {
    throw ContentError(path.string() + ": file larger than 1 MB");
  }
  std::ifstream input(path);
  if (!input) {
    throw ContentError(path.string() + ": cannot open the file");
  }
  try {
    // Comments are allowed: designers document their levels in place.
    return Json::parse(input, nullptr, true, true);
  } catch (const Json::parse_error& parseError) {
    throw ContentError(path.string() + ": invalid JSON: " + parseError.what());
  }
}

EnemyType readEnemy(const Json& json, const std::string& where) {
  const Fields fields(json, where,
                      {"name", "visual", "health", "points", "size", "movement", "speed",
                       "amplitude", "frequency", "fireRate", "shotSpeed", "burst", "drop"});
  EnemyType enemy;
  enemy.name = fields.name("name", true);
  enemy.visual = fields.choice<Visual>("visual", Visual::Bydos,
                                       {{"bydos", Visual::Bydos},
                                        {"waver", Visual::Waver},
                                        {"charger", Visual::Charger},
                                        {"turret", Visual::Turret}});
  enemy.health = fields.integer("health", 1, 1, 1000);
  enemy.points = fields.integer("points", 100, 0, 100000);
  std::tie(enemy.width, enemy.height) = fields.size("size", {32.0F, 24.0F}, 4.0F, 200.0F);
  enemy.movement = fields.choice<MovementPattern>("movement", MovementPattern::Straight,
                                                  {{"straight", MovementPattern::Straight},
                                                   {"sine", MovementPattern::Sine},
                                                   {"zigzag", MovementPattern::Zigzag},
                                                   {"chase", MovementPattern::Chase},
                                                   {"turret", MovementPattern::Turret}});
  enemy.speed = fields.decimal("speed", 80.0F, 10.0, 600.0);
  enemy.amplitude = fields.decimal("amplitude", 60.0F, 0.0, 300.0);
  enemy.frequency = fields.decimal("frequency", 0.5F, 0.0, 5.0);
  enemy.fireRate = fields.decimal("fireRate", 0.0F, 0.0, 5.0);
  enemy.shotSpeed = fields.decimal("shotSpeed", 220.0F, 50.0, 800.0);
  enemy.burst = static_cast<std::uint8_t>(fields.integer("burst", 1, 1, 8));
  enemy.dropChance = fields.decimal("drop", 0.15F, 0.0, 1.0);
  return enemy;
}

BossPhase readPhase(const Json& json, const std::string& where) {
  const Fields fields(json, where,
                      {"below", "attacks", "interval", "bullets", "shotSpeed", "moveSpeed",
                       "summon"});
  BossPhase phase;
  phase.below = fields.decimal("below", 1.0F, 0.01, 1.0);
  phase.attacks.clear();
  const auto& attacks = fields.array("attacks", 1, 8);
  for (std::size_t index = 0; index < attacks.size(); ++index) {
    const Json wrapper{{"attack", attacks[index]}};
    const Fields attack(wrapper, where + " attacks[" + std::to_string(index) + "]", {"attack"});
    phase.attacks.push_back(attack.choice<BossAttack>("attack", BossAttack::Aimed,
                                                      {{"aimed", BossAttack::Aimed},
                                                       {"spread", BossAttack::Spread},
                                                       {"ring", BossAttack::Ring},
                                                       {"wall", BossAttack::Wall},
                                                       {"summon", BossAttack::Summon}}));
  }
  phase.interval = fields.decimal("interval", 1.5F, 0.2, 10.0);
  phase.bullets = static_cast<std::uint8_t>(fields.integer("bullets", 5, 1, 32));
  phase.shotSpeed = fields.decimal("shotSpeed", 240.0F, 50.0, 800.0);
  phase.moveSpeed = fields.decimal("moveSpeed", 60.0F, 0.0, 400.0);
  phase.summon = fields.name("summon", false);
  if (std::ranges::find(phase.attacks, BossAttack::Summon) != phase.attacks.end() &&
      phase.summon.empty()) {
    fields.fail("summon", "is required by the \"summon\" attack (name of an enemy)");
  }
  return phase;
}

BossType readBoss(const Json& json, const std::string& where) {
  const Fields fields(json, where,
                      {"name", "visual", "health", "points", "size", "trophy", "phases"});
  BossType boss;
  boss.name = fields.name("name", true);
  boss.visual = fields.choice<Visual>("visual", Visual::Boss,
                                      {{"boss", Visual::Boss}, {"final-boss", Visual::FinalBoss}});
  boss.health = fields.integer("health", 60, 1, 20000);
  boss.points = fields.integer("points", 5000, 0, 1000000);
  const auto drawn = boss.visual == Visual::FinalBoss ? std::pair{150.0F, 120.0F}
                                                     : std::pair{120.0F, 96.0F};
  std::tie(boss.width, boss.height) = fields.size("size", drawn, 16.0F, 300.0F);
  boss.trophy = fields.boolean("trophy", false);
  boss.phases.clear();
  const auto& phases = fields.array("phases", 1, 8);
  for (std::size_t index = 0; index < phases.size(); ++index) {
    boss.phases.push_back(
        readPhase(phases[index], where + " phases[" + std::to_string(index) + "]"));
    if (index == 0 && boss.phases.front().below != 1.0F) {
      fields.fail("phases", "must start with a phase whose \"below\" is 1.0 (full health)");
    }
    if (index > 0 && boss.phases[index].below >= boss.phases[index - 1].below) {
      fields.fail("phases", "must be sorted by decreasing \"below\" (1.0, then 0.6, then 0.3...)");
    }
  }
  return boss;
}

std::string describe(const std::string& file, const char* list, std::size_t index,
                     const Json& item) {
  std::string where = file + ": " + list + "[" + std::to_string(index) + "]";
  if (item.is_object() && item.contains("name") && item.at("name").is_string()) {
    where += " \"" + item.at("name").get<std::string>() + "\"";
  }
  return where;
}

void readEnemyFile(const std::filesystem::path& path, GameContent& content) {
  const auto json = readJson(path);
  const auto file = path.filename().string();
  const Fields root(json, file, {"enemies", "bosses"});
  const auto& enemies = root.array("enemies", 1, 64);
  for (std::size_t index = 0; index < enemies.size(); ++index) {
    auto enemy = readEnemy(enemies[index], describe(file, "enemies", index, enemies[index]));
    if (content.findEnemy(enemy.name) != nullptr) {
      throw ContentError(file + ": two enemies are called \"" + enemy.name + "\"");
    }
    content.enemies.push_back(std::move(enemy));
  }
  const auto& bosses = root.array("bosses", 0, 32);
  for (std::size_t index = 0; index < bosses.size(); ++index) {
    auto boss = readBoss(bosses[index], describe(file, "bosses", index, bosses[index]));
    if (content.findBoss(boss.name) || content.findEnemy(boss.name) != nullptr) {
      throw ContentError(file + ": the name \"" + boss.name + "\" is already used");
    }
    content.bosses.push_back(std::move(boss));
  }
  // Summoned enemies must exist: checked once every enemy is known.
  for (const auto& boss : content.bosses) {
    for (const auto& phase : boss.phases) {
      if (!phase.summon.empty() && content.findEnemy(phase.summon) == nullptr) {
        throw ContentError(file + ": boss \"" + boss.name + "\" summons \"" + phase.summon +
                           "\", which is not in \"enemies\"");
      }
    }
  }
}

WaveDefinition readWave(const Json& json, const std::string& where, const GameContent& content) {
  const Fields fields(json, where,
                      {"enemy", "boss", "count", "interval", "formation", "speed", "drop"});
  WaveDefinition wave;
  wave.enemy = fields.name("enemy", false);
  wave.bossType = fields.name("boss", false);
  if (wave.enemy.empty() == wave.bossType.empty()) {
    throw ContentError(where + ": a wave has either \"enemy\" or \"boss\", not both nor none");
  }
  if (!wave.bossType.empty()) {
    if (!content.findBoss(wave.bossType)) {
      fields.fail("boss", "names a boss that is not in enemies.json: \"" + wave.bossType + "\"");
    }
    for (const auto* key : {"count", "interval", "formation", "speed", "drop"}) {
      if (fields.has(key)) {
        fields.fail(key, "does not apply to a boss wave");
      }
    }
    wave.boss = true;
    wave.enemyCount = 1;
    wave.spawnIntervalSeconds = 0.5F;
    return wave;
  }
  const auto* enemy = content.findEnemy(wave.enemy);
  if (enemy == nullptr) {
    fields.fail("enemy", "names an enemy that is not in enemies.json: \"" + wave.enemy + "\"");
  }
  wave.enemyCount = static_cast<std::uint32_t>(fields.integer("count", 5, 1, 50));
  wave.spawnIntervalSeconds = fields.decimal("interval", 1.0F, 0.1, 10.0);
  wave.formation = fields.choice<Formation>("formation", Formation::Random,
                                            {{"random", Formation::Random},
                                             {"line", Formation::Line},
                                             {"v", Formation::V},
                                             {"stairs", Formation::Stairs}});
  wave.enemySpeedMultiplier = fields.decimal("speed", 1.0F, 0.2, 5.0);
  wave.pickupDropChance = fields.decimal("drop", enemy->dropChance, 0.0, 1.0);
  return wave;
}

LevelDefinition readLevel(const std::filesystem::path& path, std::uint32_t number,
                          const GameContent& content) {
  const auto json = readJson(path);
  const auto file = path.filename().string();
  const Fields fields(json, file,
                      {"title", "story", "theme", "intro", "waveDelay", "nextLevelDelay", "waves"});
  LevelDefinition level;
  level.number = number;
  level.title = fields.text("title", MAX_LEVEL_TITLE, true);
  level.story = fields.text("story", MAX_LEVEL_STORY, false);
  level.theme = static_cast<std::uint8_t>(fields.integer("theme", 0, 0, 4));
  level.introSeconds = fields.decimal("intro", 4.0F, 0.0, 15.0);
  level.interWaveDelaySeconds = fields.decimal("waveDelay", 2.5F, 0.0, 30.0);
  level.nextLevelDelaySeconds = fields.decimal("nextLevelDelay", 4.0F, 0.0, 30.0);
  const auto& waves = fields.array("waves", 1, 30);
  for (std::size_t index = 0; index < waves.size(); ++index) {
    level.waves.push_back(
        readWave(waves[index], file + ": waves[" + std::to_string(index) + "]", content));
  }
  return level;
}

std::filesystem::path insideDirectory(const std::filesystem::path& directory,
                                      const std::string& relative, const std::string& where) {
  const auto normal = std::filesystem::path(relative).lexically_normal();
  if (relative.empty() || normal.is_absolute() || normal.has_root_name() ||
      (!normal.empty() && *normal.begin() == "..")) {
    throw ContentError(where + ": \"" + relative +
                       "\" must be a path inside the content directory, like \"story/01.json\"");
  }
  return directory / normal;
}

void readStory(const std::filesystem::path& directory, GameContent& content) {
  const auto path = directory / "story.json";
  const auto json = readJson(path);
  const Fields fields(json, "story.json", {"name", "levels"});
  content.campaignName = fields.text("name", MAX_LEVEL_TITLE, true);
  const auto& levels = fields.array("levels", 1, 99);
  for (std::size_t index = 0; index < levels.size(); ++index) {
    const auto where = "story.json: levels[" + std::to_string(index) + "]";
    if (!levels[index].is_string()) {
      throw ContentError(where + " must be a file name in quotes");
    }
    const auto file = insideDirectory(directory, levels[index].get<std::string>(), where);
    content.story.define(readLevel(file, static_cast<std::uint32_t>(index + 1), content));
  }
  content.story.setFinalLevel(static_cast<std::uint32_t>(levels.size()));
}

void readEndless(const std::filesystem::path& directory, GameContent& content) {
  const auto json = readJson(directory / "endless.json");
  const Fields fields(json, "endless.json",
                      {"title", "enemies", "bosses", "bossEvery", "wavesPerLevel", "intro"});
  LevelGenerator generator;
  generator.titlePrefix = fields.text("title", 32, false);
  if (generator.titlePrefix.empty()) {
    generator.titlePrefix = "Endless - Round";
  }
  for (const auto& name : fields.array("enemies", 1, 64)) {
    if (!name.is_string() || content.findEnemy(name.get<std::string>()) == nullptr) {
      throw ContentError("endless.json: \"enemies\" lists " + name.dump() +
                         ", which is not an enemy of enemies.json");
    }
    generator.enemies.push_back(name.get<std::string>());
  }
  for (const auto& name : fields.array("bosses", 0, 32)) {
    if (!name.is_string() || !content.findBoss(name.get<std::string>())) {
      throw ContentError("endless.json: \"bosses\" lists " + name.dump() +
                         ", which is not a boss of enemies.json");
    }
    generator.bosses.push_back(name.get<std::string>());
  }
  generator.bossEvery = static_cast<std::uint32_t>(fields.integer("bossEvery", 2, 1, 20));
  generator.wavesPerLevel = static_cast<std::uint32_t>(fields.integer("wavesPerLevel", 5, 1, 10));
  generator.introSeconds = fields.decimal("intro", 3.0F, 0.0, 15.0);
  content.endless.setGenerator(std::move(generator));
}

} // namespace

DifficultySettings difficultySettings(Difficulty difficulty) {
  switch (difficulty) {
  case Difficulty::Easy: return {0.6F, 0.55F, 0.8F, 0.9F, 1.5F, 5};
  case Difficulty::Hard: return {1.5F, 1.4F, 1.2F, 1.1F, 0.8F, 2};
  case Difficulty::Normal: break;
  }
  return {};
}

const EnemyType* GameContent::findEnemy(std::string_view name) const {
  const auto found = std::ranges::find(enemies, name, &EnemyType::name);
  return found == enemies.end() ? nullptr : &*found;
}

std::optional<std::uint16_t> GameContent::findBoss(std::string_view name) const {
  const auto found = std::ranges::find(bosses, name, &BossType::name);
  if (found == bosses.end()) {
    return std::nullopt;
  }
  return static_cast<std::uint16_t>(found - bosses.begin());
}

const BossType& GameContent::boss(std::uint16_t index) const {
  if (index < bosses.size()) {
    return bosses[index];
  }
  // The boss of generated levels: health comes from the wave, it sweeps and fires aimed shots.
  static const BossType generic = [] {
    BossType boss;
    boss.name = "generic";
    BossPhase phase;
    phase.attacks = {BossAttack::Aimed, BossAttack::Spread};
    phase.interval = 1.6F;
    phase.bullets = 3;
    phase.moveSpeed = 65.0F;
    boss.phases = {phase};
    return boss;
  }();
  return generic;
}

GameContent loadContent(const std::filesystem::path& directory) {
  if (!std::filesystem::is_directory(directory)) {
    throw ContentError(directory.string() + ": content directory not found");
  }
  GameContent content;
  readEnemyFile(directory / "enemies.json", content);
  readStory(directory, content);
  readEndless(directory, content);
  return content;
}

} // namespace game
