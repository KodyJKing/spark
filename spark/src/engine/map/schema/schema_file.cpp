#include "schema.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include "schema_file.hpp"

#include "utils/Utils.hpp"

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

    static Schema s_mainSchema;
    static bool s_mainSchemaLoaded = false;
    Schema* getMainSchema() {
        if (s_mainSchemaLoaded) return &s_mainSchema;
        auto defaultSchemaPath = Utils::getTagSchemaPath();
        s_mainSchema = loadSchemaFromFile(defaultSchemaPath);
        s_mainSchemaLoaded = true;
        return &s_mainSchema;
    }

    void saveMainSchema() {
        auto defaultSchemaPath = Utils::getTagSchemaPath();
        saveSchemaToFile(s_mainSchema, defaultSchemaPath);
    }
}
