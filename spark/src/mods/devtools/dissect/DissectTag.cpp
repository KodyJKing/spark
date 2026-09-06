#include "DissectTag.hpp"

#include "engine/tags/schema/schema.hpp"
#include "engine/halo1.hpp"

#include "imgui.h"

#include <map>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace Mod::DevTools::DissectTag {

    using namespace Engine::TagSchema;

    static const char* SCHEMA_FILE_PATH = "./tag_schema.json";

    TagSchemaCollection tagSchemas;

    void saveSchemas() {
        std::ofstream file(SCHEMA_FILE_PATH);
        if (!file.is_open())
            return;
        file << tagSchemas.toJsonString();
    }

    // Returns false if the file doesn't exist or couldn't be parsed.
    bool loadSchemas() {
        std::ifstream file(SCHEMA_FILE_PATH);
        if (!file.is_open())
            return false;
        std::stringstream buffer;
        buffer << file.rdbuf();
        tagSchemas = TagSchemaCollection::fromJsonString(buffer.str());
        return true;
    }

    bool initialized = false;
    void initialize() {
        if (initialized)
            return;
        initialized = true;
        loadSchemas();
    }

    struct RenderContext {
        Engine::Tag* tag;
        TagSchema* schema;
        void* structureBase;
        Structure* structure;
        Field* field;
        size_t structureSize;
        
        bool* structureModified;
    };

    struct WindowState {
        uint32_t currentTagId;
        bool open;
    };

    std::map<uint32_t, WindowState> windowStates;
    
    void openWindow(uint32_t tagId) {
        if (windowStates.find(tagId) == windowStates.end()) {
            windowStates[tagId] = WindowState{tagId, true};
        }
    }

    void clearClosedWindows() {
        for (auto it = windowStates.begin(); it != windowStates.end(); ) {
            if (!it->second.open) {
                it = windowStates.erase(it);
            } else {
                ++it;
            }
        }
    }

    std::string toHex(uint32_t value) {
        char buffer[9];
        snprintf(buffer, sizeof(buffer), "%08X", value);
        return std::string(buffer);
    }

    size_t guessDataSize(Engine::Tag* tag) {
        auto nextTag = tag + 1;
        if (!Engine::tagExists(nextTag))
            return 0;
        void* dataPointer = tag->getData();
        void* nextDataPointer = nextTag->getData();
        return reinterpret_cast<uint8_t*>(nextDataPointer) - reinterpret_cast<uint8_t*>(dataPointer);
    }

    void deleteFieldFromStructure(Structure& structure, Field* field) {
        auto it = std::find_if(structure.fields.begin(), structure.fields.end(),
                               [&](const Field& f) { return strcmp(f.name, field->name) == 0 && f.offset == field->offset; });
        if (it != structure.fields.end()) {
            structure.fields.erase(it);
        }
    }

    void renderTypeInput(Field& field) {
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
        if (ImGui::BeginCombo("##Type", PrimitiveTypeRefNames[static_cast<int>(field.type.primitive)])) {
            for (int i = 0; i <= static_cast<int>(PrimitiveTypeRef::Enumeration); ++i) {
                bool isSelected = (static_cast<int>(field.type.primitive) == i);
                if (ImGui::Selectable(PrimitiveTypeRefNames[i], isSelected)) {
                    field.type.primitive = static_cast<PrimitiveTypeRef>(i);
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    void renderField(TagSchema& schema, void* structureBase, Structure& structure, Field& field, bool& structureModified);

    void renderStructure(TagSchema& schema, void* structureBase, Structure& structure, size_t size);

    // void renderSubstructure(TagSchema& schema, void* structureBase, Field& field, bool& structureModified) {
    //     // Find structure with this name.
    //     if (!schema.structures.count(field.name)) {
    //         ImGui::Text("Substructure not found: %s", field.name);
    //         return;
    //     }
    //     Structure& substructure = schema.structures.at(field.name);

    //     ImGui::Indent();
    //     renderStructure(schema, structureBase, substructure, structureModified);
    //     ImGui::Unindent();
    // }

    void renderField(TagSchema& schema, void* structureBase, Structure& structure, Field& field, bool& structureModified) {
        ImGui::PushID(&field);

        // Offset editor
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
        ImGui::InputScalar("##Offset", ImGuiDataType_U64, &field.offset, nullptr, nullptr, "%08X");
        ImGui::SameLine();
        
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
        ImGui::InputText("##Name", field.name, sizeof(field.name));
        ImGui::SameLine();
        
        renderTypeInput(field);
        ImGui::SameLine();

        auto relocationOffset = Engine::mapRelocationOffset();
        auto value = field.readString({.relocationOffset = relocationOffset, .structureBase = reinterpret_cast<uintptr_t>(structureBase)});
        ImGui::Text("%s", value.c_str());
        
        ImGui::SameLine();
        if (ImGui::Button("x")) {
            deleteFieldFromStructure(structure, &field);
            structureModified = true;
        }

        ImGui::PopID();
    }

    // Render a single byte of unknown data on the same line.
    void renderUnknown(void* structureBase, size_t offset, Structure& structure, bool& structureModified) {
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 2);
        uint8_t* bytePointer = reinterpret_cast<uint8_t*>(structureBase) + offset;
        ImGui::Text("%02X", *bytePointer);

        // If the user right clicks this byte, create a field here. Populate as a uint8_t field.
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            // Create a new field at this offset
            Field newField;
            snprintf(newField.name, sizeof(newField.name), "field_%X", offset);
            newField.type.primitive = PrimitiveTypeRef::Uint8;
            newField.offset = offset;
            newField.bitOffset = 0;
            // Add the new field to the structure
            // This requires access to the structure, which is now available as an argument.
            structure.fields.push_back(newField);
            structureModified = true;
        }
    }

    TagSchema* getTagSchema(Engine::Tag* tag, bool createIfMissing = true) {
        auto groupId = tag->classIdStr();
        if (tagSchemas.schemas.find(groupId) == tagSchemas.schemas.end()) {
            if (createIfMissing) {
                tagSchemas.schemas[groupId] = TagSchema{.name = groupId, .groupId = groupId};
            } else {
                return nullptr;
            }
        }
        return &tagSchemas.schemas.at(groupId);
    }

    void renderStructure(TagSchema& schema, void* structureBase, Structure& structure, size_t size) {
        #define UNKNOWN_ROW_LENGTH 16
        size_t head = 0;
        size_t column = UNKNOWN_ROW_LENGTH;
        bool structureModified = false;
        
        auto renderUnknownCell = [&]() {
            // Stripe columns in groups of 4
            auto group = column / 4;
            if (group & 1) {
                ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(128, 128, 128, 255));
            }
            
            if (column >= UNKNOWN_ROW_LENGTH) {
                column = 0;
            } else {
                ImGui::SameLine();
            }
            renderUnknown(structureBase, head, structure, structureModified);
            head++;
            column++;

            if (group & 1) {
                ImGui::PopStyleColor();
            }
        };

        std::sort(structure.fields.begin(), structure.fields.end(), [](const Field& a, const Field& b) {
            return a.offset < b.offset;
        });

        auto fieldCount = structure.fields.size();
        for (size_t i = 0; i < fieldCount; i++) {
            auto& field = structure.fields[i];
            
            while (head < field.offset) {
                renderUnknownCell();
                if (structureModified) break;
            }

            if (structureModified) break;
            renderField(schema, structureBase, structure, field, structureModified);

            if (structureModified) break;

            head += Engine::TagSchema::PrimitiveTypeRefSizes[static_cast<size_t>(field.type.primitive)];
            column = UNKNOWN_ROW_LENGTH;
        }

        while (head < size) {
            renderUnknownCell();
            if (structureModified) break;
        }
    }

    void renderTagDetails(Engine::Tag* tag) {
        RenderContext context;
        context.tag = tag;
        
        auto groupId = tag->classIdStr();
        ImGui::Text("Group ID: \"%s\"", groupId.c_str());
        
        auto size = guessDataSize(tag);
        ImGui::Text("Estimated Data Size: %zu bytes", size);
        
        auto schema = getTagSchema(tag);
        if (!schema) {
            ImGui::Text("Tag schema not found.");
            return;
        }
        
        auto& mainStructure = schema->mainStructure;

        renderStructure(*schema, tag->getData(), mainStructure, size);
    }

    void render() {
        initialize();

        if (ImGui::Begin("Dissect Tag Schemas")) {
            if (ImGui::Button("Save"))
                saveSchemas();
        }
        ImGui::End();
        
        for (auto& [tagId, windowState] : windowStates) {
            auto tag = Engine::getTag(tagId);
            bool tagExists = Engine::tagExists(tag);
            
            std::string windowTitle;
            if (tagExists)
                windowTitle = std::string("Dissect Tag ") + tag->getResourcePath();
            else
                windowTitle = "Dissect Tag " + toHex(tagId) + " (Not Found)";
            
            ImGui::Begin(windowTitle.c_str(), &windowState.open);

            
            ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);

            if (!tagExists) {
                ImGui::Text("Tag not found.");
            } else {
                renderTagDetails(tag);
            }
            
            ImGui::End();
        }

        clearClosedWindows();
    }

}