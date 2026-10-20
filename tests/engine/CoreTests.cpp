#include <chrono>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "engine/core/Clock.hpp"
#include "engine/core/DeveloperConsole.hpp"
#include "engine/core/EventBus.hpp"
#include "engine/core/Logger.hpp"
#include "engine/core/Metrics.hpp"
#include "engine/core/ResourceManager.hpp"
#include "engine/core/SystemScheduler.hpp"

namespace {

struct FirstEvent {
  int value;
};

struct OtherEvent {
  int value;
};

struct DummyRegistry {};

struct TestResource {
  std::string value;
};

} // namespace

TEST_CASE("Fixed timestep consumes only complete ticks", "[core][clock]") {
  engine::core::FixedTimestep timestep(std::chrono::milliseconds(10));

  timestep.advance(std::chrono::milliseconds(9));
  CHECK_FALSE(timestep.ready());
  CHECK_FALSE(timestep.consumeStep());

  timestep.advance(std::chrono::milliseconds(26));
  CHECK(timestep.consumeStep());
  CHECK(timestep.consumeStep());
  CHECK(timestep.consumeStep());
  CHECK_FALSE(timestep.consumeStep());
}

TEST_CASE("Fixed timestep rejects invalid durations and deltas", "[core][clock][invalid-input]") {
  CHECK_THROWS_AS(engine::core::FixedTimestep(std::chrono::nanoseconds::zero()),
                  std::invalid_argument);
  CHECK_THROWS_AS(engine::core::FixedTimestep(std::chrono::nanoseconds(-1)), std::invalid_argument);

  engine::core::FixedTimestep timestep;
  CHECK_THROWS_AS(timestep.advance(std::chrono::nanoseconds(-1)), std::invalid_argument);
}

TEST_CASE("Event bus dispatches to matching event subscribers", "[core][event-bus]") {
  engine::core::EventBus bus;
  int firstTotal = 0;
  int otherTotal = 0;
  int secondSubscriber = 0;

  bus.subscribe<FirstEvent>([&firstTotal](const FirstEvent& event) { firstTotal += event.value; });
  bus.subscribe<FirstEvent>(
      [&secondSubscriber](const FirstEvent& event) { secondSubscriber = event.value; });
  bus.subscribe<OtherEvent>([&otherTotal](const OtherEvent& event) { otherTotal += event.value; });

  bus.emit(FirstEvent{7});
  CHECK(firstTotal == 7);
  CHECK(secondSubscriber == 7);
  CHECK(otherTotal == 0);

  bus.emit(OtherEvent{4});
  CHECK(otherTotal == 4);
  CHECK(firstTotal == 7);
}

TEST_CASE("System scheduler runs systems in registration order", "[core][scheduler]") {
  engine::core::SystemScheduler<DummyRegistry> scheduler;
  std::vector<int> order;
  scheduler.addSystem("first", [&order](DummyRegistry&) { order.push_back(1); });
  scheduler.addSystem("second", [&order](DummyRegistry&) { order.push_back(2); });

  DummyRegistry registry;
  scheduler.run(registry);

  REQUIRE(order.size() == 2);
  CHECK(order[0] == 1);
  CHECK(order[1] == 2);
}

TEST_CASE("Resource manager caches, retrieves, and unloads typed resources", "[core][resources]") {
  engine::core::ResourceManager resources;
  int loadCount = 0;
  resources.registerLoader<TestResource>([&loadCount](const std::string& id) {
    ++loadCount;
    return std::make_shared<TestResource>(TestResource{id});
  });

  const auto first = resources.load<TestResource>("ship");
  const auto cached = resources.load<TestResource>("ship");
  REQUIRE(first != nullptr);
  CHECK(first == cached);
  CHECK(first->value == "ship");
  CHECK(loadCount == 1);
  const auto preloaded = resources.preload<TestResource>({"bullet", "enemy"});
  REQUIRE(preloaded.size() == 2);
  CHECK(preloaded[0]->value == "bullet");
  CHECK(loadCount == 3);
  CHECK(resources.get<TestResource>("missing") == nullptr);
  CHECK(resources.unload<TestResource>("ship"));
  CHECK_FALSE(resources.unload<TestResource>("ship"));
  CHECK(resources.load<TestResource>("ship") != first);
  CHECK(loadCount == 4);
}

TEST_CASE("Resource manager reports invalid ids and missing loaders",
          "[core][resources][invalid-input]") {
  engine::core::ResourceManager resources;
  CHECK_THROWS_AS(resources.load<TestResource>("missing"), std::logic_error);
  CHECK_THROWS_AS(resources.load<TestResource>(""), std::invalid_argument);
  CHECK_THROWS_AS(resources.registerLoader<TestResource>({}), std::invalid_argument);
  resources.registerLoader<TestResource>(
      [](const std::string&) { return std::shared_ptr<TestResource>{}; });
  CHECK_THROWS_AS(resources.load<TestResource>("empty"), std::runtime_error);
}

TEST_CASE("Developer console executes commands and reads and writes typed CVars",
          "[core][console]") {
  engine::core::DeveloperConsole console;
  console.defineVariable("god_mode", false);
  console.defineVariable("lives", std::int64_t{3});
  console.defineVariable("gravity", 9.8);
  console.defineVariable("player_name", std::string{"pilot"});
  console.registerCommand("echo", [](const std::vector<std::string>& args) {
    return args.size() > 1 ? args[1] : std::string{};
  });

  CHECK(console.execute("get god_mode") == "false");
  CHECK(console.execute("set god_mode true") == "true");
  CHECK(console.execute("set lives 5") == "5");
  CHECK(console.execute("set gravity 4.25") == "4.25");
  CHECK(console.execute("set player_name ace") == "ace");
  CHECK(console.execute("echo hello") == "hello");
  CHECK(console.execute("   ").empty());

  const auto* lives = console.getVariable("lives");
  REQUIRE(lives != nullptr);
  CHECK(std::get<std::int64_t>(*lives) == 5);
}

TEST_CASE("Developer console rejects unknown names, invalid values, and duplicate entries",
          "[core][console][invalid-input]") {
  engine::core::DeveloperConsole console;
  console.defineVariable("lives", std::int64_t{3});
  console.registerCommand("echo", [](const std::vector<std::string>&) { return std::string{}; });

  CHECK_THROWS_AS(console.execute("missing"), std::out_of_range);
  CHECK_THROWS_AS(console.execute("get"), std::invalid_argument);
  CHECK_THROWS_AS(console.execute("set unknown 2"), std::out_of_range);
  CHECK_THROWS_AS(console.execute("set lives three"), std::invalid_argument);
  CHECK_THROWS_AS(console.setVariable("lives", "3.5"), std::invalid_argument);
  console.defineVariable("gravity", 9.8);
  CHECK_THROWS_AS(console.setVariable("gravity", "nan"), std::invalid_argument);
  CHECK_THROWS_AS(console.defineVariable("lives", std::int64_t{4}), std::invalid_argument);
  CHECK_THROWS_AS(console.registerCommand(
                      "echo", [](const std::vector<std::string>&) { return std::string{}; }),
                  std::invalid_argument);
}

TEST_CASE("Metrics reports frame, entity, CPU, and memory measurements", "[core][metrics]") {
  engine::core::Metrics metrics;
  metrics.recordFrame(20.0, 17);
  metrics.setCpuUsagePercent(62.5);
  metrics.setMemoryBytes(4096);

  const auto& snapshot = metrics.snapshot();
  CHECK(snapshot.frameCount == 1);
  CHECK(snapshot.framesPerSecond == Catch::Approx(50.0));
  CHECK(snapshot.frameTimeMilliseconds == Catch::Approx(20.0));
  CHECK(snapshot.entityCount == 17);
  CHECK(snapshot.cpuUsagePercent == Catch::Approx(62.5));
  CHECK(snapshot.memoryBytes == 4096);

  CHECK_THROWS_AS(metrics.recordFrame(-1.0, 0), std::invalid_argument);
  CHECK_THROWS_AS(metrics.setCpuUsagePercent(101.0), std::invalid_argument);
}

TEST_CASE("Logger filters by level and keeps lines whole across threads", "[core][logger]") {
  std::ostringstream output;
  auto* const previous = std::clog.rdbuf(output.rdbuf());
  engine::core::Logger::setLevel(engine::core::LogLevel::Info);

  engine::core::Logger::debug("hidden");
  constexpr int THREADS = 4;
  constexpr int LINES_PER_THREAD = 200;
  {
    std::vector<std::jthread> writers;
    for (int index = 0; index < THREADS; ++index) {
      writers.emplace_back([] {
        for (int line = 0; line < LINES_PER_THREAD; ++line) {
          engine::core::Logger::info("network-and-game-loop");
        }
      });
    }
  }
  std::clog.rdbuf(previous);

  std::istringstream lines(output.str());
  int count = 0;
  for (std::string line; std::getline(lines, line); ++count) {
    CHECK(line.ends_with("] INFO network-and-game-loop"));
  }
  CHECK(count == THREADS * LINES_PER_THREAD);
  CHECK(output.str().find("hidden") == std::string::npos);
}
