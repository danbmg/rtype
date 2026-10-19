#include <algorithm>
#include <cmath>
#include <iostream>

#include "engine/core/Metrics.hpp"
#include "engine/ecs/Registry.hpp"

namespace {

struct Position {
  float x;
  float y;
};

struct Velocity {
  float x;
  float y;
};

struct Paddle {
  float height;
  bool leftSide;
};

struct Ball {
  float size;
};

struct Score {
  int points = 0;
};

constexpr float FIELD_WIDTH = 800.0F;
constexpr float FIELD_HEIGHT = 600.0F;
constexpr float PADDLE_WIDTH = 14.0F;
constexpr float PADDLE_HEIGHT = 90.0F;
constexpr float BALL_SIZE = 12.0F;
constexpr float PADDLE_SPEED = 110.0F;
constexpr float STEP_SECONDS = 1.0F / 60.0F;
constexpr int WINNING_SCORE = 5;

void resetBall(engine::ecs::Registry& registry, engine::ecs::Entity ball, float direction) {
  auto* position = registry.getComponent<Position>(ball);
  auto* velocity = registry.getComponent<Velocity>(ball);
  if (position == nullptr || velocity == nullptr) {
    return;
  }
  position->x = FIELD_WIDTH / 2.0F;
  position->y = FIELD_HEIGHT / 2.0F;
  velocity->x = 300.0F * direction;
  velocity->y = direction * 180.0F;
}

void movePaddle(engine::ecs::Registry& registry, engine::ecs::Entity paddleEntity,
                float targetY) {
  auto* paddlePosition = registry.getComponent<Position>(paddleEntity);
  const auto* paddle = registry.getComponent<Paddle>(paddleEntity);
  if (paddlePosition == nullptr || paddle == nullptr) {
    return;
  }
  const float center = paddlePosition->y + paddle->height / 2.0F;
  const float difference = targetY - center;
  const float maximumStep = PADDLE_SPEED * STEP_SECONDS;
  paddlePosition->y += std::clamp(difference, -maximumStep, maximumStep);
  paddlePosition->y = std::clamp(paddlePosition->y, 0.0F, FIELD_HEIGHT - paddle->height);
}

bool overlaps(const Position& ball, const Ball& ballShape, const Position& paddle,
              const Paddle& paddleShape) {
  return ball.x < paddle.x + PADDLE_WIDTH && ball.x + ballShape.size > paddle.x &&
         ball.y < paddle.y + paddleShape.height && ball.y + ballShape.size > paddle.y;
}

} // namespace

int main() {
  engine::ecs::Registry world;
  const auto leftPaddle = world.spawnEntity();
  world.addComponent(leftPaddle, Position{30.0F, FIELD_HEIGHT / 2.0F - PADDLE_HEIGHT / 2.0F});
  world.addComponent(leftPaddle, Paddle{PADDLE_HEIGHT, true});
  world.addComponent(leftPaddle, Score{});

  const auto rightPaddle = world.spawnEntity();
  world.addComponent(rightPaddle,
                     Position{FIELD_WIDTH - 30.0F - PADDLE_WIDTH,
                              FIELD_HEIGHT / 2.0F - PADDLE_HEIGHT / 2.0F});
  world.addComponent(rightPaddle, Paddle{PADDLE_HEIGHT, false});
  world.addComponent(rightPaddle, Score{});

  const auto ball = world.spawnEntity();
  world.addComponent(ball, Position{});
  world.addComponent(ball, Velocity{});
  world.addComponent(ball, Ball{BALL_SIZE});
  resetBall(world, ball, 1.0F);

  engine::core::Metrics metrics;
  for (int tick = 0; tick < 36000; ++tick) {
    auto* ballPosition = world.getComponent<Position>(ball);
    auto* ballVelocity = world.getComponent<Velocity>(ball);
    const auto* ballShape = world.getComponent<Ball>(ball);
    if (ballPosition == nullptr || ballVelocity == nullptr || ballShape == nullptr) {
      return 1;
    }

    movePaddle(world, leftPaddle, ballPosition->y + ballShape->size / 2.0F);
    movePaddle(world, rightPaddle, ballPosition->y + ballShape->size / 2.0F);
    ballPosition->x += ballVelocity->x * STEP_SECONDS;
    ballPosition->y += ballVelocity->y * STEP_SECONDS;

    if (ballPosition->y <= 0.0F || ballPosition->y + ballShape->size >= FIELD_HEIGHT) {
      ballPosition->y = std::clamp(ballPosition->y, 0.0F, FIELD_HEIGHT - ballShape->size);
      ballVelocity->y = -ballVelocity->y;
    }

    for (const auto paddleEntity : {leftPaddle, rightPaddle}) {
      const auto* paddlePosition = world.getComponent<Position>(paddleEntity);
      const auto* paddle = world.getComponent<Paddle>(paddleEntity);
      if (paddlePosition == nullptr || paddle == nullptr ||
          !overlaps(*ballPosition, *ballShape, *paddlePosition, *paddle)) {
        continue;
      }
      ballVelocity->x = std::abs(ballVelocity->x) * (paddle->leftSide ? 1.0F : -1.0F);
      const float offset =
          (ballPosition->y + ballShape->size / 2.0F) - (paddlePosition->y + paddle->height / 2.0F);
      ballVelocity->y = offset * 5.0F;
      ballPosition->x = paddle->leftSide ? paddlePosition->x + PADDLE_WIDTH
                                         : paddlePosition->x - ballShape->size;
    }

    if (ballPosition->x < 0.0F) {
      ++world.getComponent<Score>(rightPaddle)->points;
      resetBall(world, ball, 1.0F);
    } else if (ballPosition->x > FIELD_WIDTH) {
      ++world.getComponent<Score>(leftPaddle)->points;
      resetBall(world, ball, -1.0F);
    }

    metrics.recordFrame(STEP_SECONDS * 1000.0, world.view<Position>().size());
    if (world.getComponent<Score>(leftPaddle)->points >= WINNING_SCORE ||
        world.getComponent<Score>(rightPaddle)->points >= WINNING_SCORE) {
      break;
    }
  }

  std::cout << "Pong simulation result: " << world.getComponent<Score>(leftPaddle)->points
            << " - " << world.getComponent<Score>(rightPaddle)->points << '\n'
            << "Simulated FPS: " << metrics.snapshot().framesPerSecond << '\n';
}
