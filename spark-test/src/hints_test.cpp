#include <catch_amalgamated.hpp>

#include "engine/map/schema/hints.hpp"

using namespace Engine::Map;

TEST_CASE("Engine hints preserve scalar string and vector scores", "[hints]") {
    StructureRef structure{};
    Hints::HintContext ctx{nullptr, nullptr, structure};
    float scalar = 1.0f;
    REQUIRE(Hints::floatScore(ctx, &scalar) == 1.0f);
    scalar = 0.0f;
    REQUIRE(Hints::floatScore(ctx, &scalar) == 0.0f);
    char text[32] = "test1234";
    REQUIRE(Hints::tagStringScore(ctx, text) == 1.0f);
    float vector[] = {0.6f, 0.8f, 0.001f};
    REQUIRE(Hints::vecNScore(ctx, vector, 3) == 10.0f);
    REQUIRE(Hints::vecNScore(ctx, vector, 0) == 0.0f);
}

TEST_CASE("Engine hint cache reuses distributions and respects suggestion thresholds", "[hints]") {
    StructureRef structure{};
    Hints::HintContext ctx{nullptr, nullptr, structure};
    Hints::HintCache cache;
    float scores[TypeCount];
    Hints::computeTypeScoreDistribution(ctx, nullptr, scores);
    for (float score : scores) REQUIRE(score == 0.0f);

    auto* cached = cache.getScores(ctx, nullptr);
    REQUIRE(cache.cache.size() == 1);
    REQUIRE(cache.scores.size() == TypeCount);
    REQUIRE(cache.getScores(ctx, nullptr) == cached);
    REQUIRE(cache.suggestedType(ctx, nullptr, 0.25f) == Type::U32);

    cached[static_cast<int>(Type::F32)] = 0.4f;
    cached[static_cast<int>(Type::Vec3)] = 0.6f;
    REQUIRE(cache.suggestedType(ctx, nullptr, 0.5f) == Type::Vec3);
    REQUIRE(cache.suggestedType(ctx, nullptr, 0.7f) == Type::U32);
    REQUIRE(cache.getScores(ctx, nullptr)[static_cast<int>(Type::Vec3)] == 0.6f);
}
