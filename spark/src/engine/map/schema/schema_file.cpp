#include "schema.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include "schema_file.hpp"

namespace Engine::Map {

    Schema loadSchemaFromFile(const std::filesystem::path& filePath) {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            auto str = filePath.string();
            std::cout << "Failed to open schema file: " << str << std::endl;
            return Schema{};
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        return Schema::fromJsonString(buffer.str());
    }

    void saveSchemaToFile(const Schema &schema, const std::filesystem::path &filePath) {
        std::ofstream file(filePath);
        if (!file.is_open()) {
            auto str = filePath.string();
            std::cout << "Failed to open schema file for writing: " << str << std::endl;    
            return;
        }
        file << schema.toJsonString();
    }
}
