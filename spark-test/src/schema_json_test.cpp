#include <catch_amalgamated.hpp>
#include <json.hpp>

#include "engine/map/schema/schema.hpp"

using namespace Engine::Map;

TEST_CASE("Schema JSON preserves structure certification", "[schema_json]") {
    Schema schema;
    auto& root = schema.structures["root"];
    root.id = "root";
    root.groupId = Engine::GroupId_Weapon;
    auto& child = schema.structures["child"];
    child.id = "child";
    child.size = 4;

    SECTION("certified root") { root.complete = true; }
    SECTION("uncertified root") { root.complete = false; }
    SECTION("certified nested structure") { child.complete = true; }

    auto json = nlohmann::json::parse(schema.toJsonString());
    REQUIRE(json["structures"]["root"]["complete"].is_boolean());
    REQUIRE(json["structures"]["root"]["complete"].get<bool>() == root.complete);
    auto restored = Schema::fromJsonString(json.dump());
    REQUIRE(restored.structures.at("root").complete == root.complete);
    REQUIRE(restored.structures.at("child").complete == child.complete);
    REQUIRE(restored.structures.at("root").size == 0);
}

TEST_CASE("Schema JSON defaults legacy certification to false and rejects invalid values", "[schema_json]") {
    Schema schema;
    auto& root = schema.structures["root"];
    root.id = "root";
    root.complete = true;
    auto json = nlohmann::json::parse(schema.toJsonString());

    SECTION("missing property") {
        json["structures"]["root"].erase("complete");
        auto restored = Schema::fromJsonString(json.dump());
        REQUIRE_FALSE(restored.structures.at("root").complete);
    }
    SECTION("string is not a boolean") {
        json["structures"]["root"]["complete"] = "true";
        REQUIRE_THROWS_AS(Schema::fromJsonString(json.dump()), nlohmann::json::type_error);
    }
    SECTION("null is not a boolean") {
        json["structures"]["root"]["complete"] = nullptr;
        REQUIRE_THROWS_AS(Schema::fromJsonString(json.dump()), nlohmann::json::type_error);
    }
}
