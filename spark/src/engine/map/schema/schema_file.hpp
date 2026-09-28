#pragma once
#include "schema.hpp"
#include <filesystem>
namespace Engine::Map {
    Schema loadSchemaFromFile(const std::filesystem::path& filePath);
    void saveSchemaToFile(const Schema& schema, const std::filesystem::path& filePath);
}
