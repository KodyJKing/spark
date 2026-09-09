#include "DissectTag.hpp"
#include "RenderContext.hpp"

#include "engine/tags/schema/schema.hpp"
#include "engine/halo1.hpp"

#include "imgui.h"

#include <map>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>

#define DEBUG_DISSECT_TAG 1

#ifdef DEBUG_DISSECT_TAG
#include <iostream>
#define DEBUG_LOG(msg) std::cout << "[DissectTag] " << msg << std::endl;
#else
#define DEBUG_LOG(msg)
#endif

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

    void renderTypeInput(RenderContext& context) {
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
        if (ImGui::BeginCombo("##Type", PrimitiveTypeRefNames[static_cast<int>(context.field->type.primitive)])) {
            for (int i = 0; i <= static_cast<int>(PrimitiveTypeRef::Enumeration); ++i) {
                bool isSelected = (static_cast<int>(context.field->type.primitive) == i);
                if (ImGui::Selectable(PrimitiveTypeRefNames[i], isSelected)) {
                    context.field->type.primitive = static_cast<PrimitiveTypeRef>(i);
                }
                if (isSelected)
                    ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }
    }

    void renderStructure(RenderContext& context);

    void renderSizeInput(Structure& structure) {
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
        ImGui::InputScalar("Size", ImGuiDataType_U64, &structure.size, nullptr, nullptr, "%08X");
        ImGui::SameLine();
        if (ImGui::Button("+")) {
            structure.size += 4;
        }
        ImGui::SameLine();
        if (ImGui::Button("-")) {
            structure.size -= 4;
        }
    }

    void renderSubstructure(RenderContext& context) {        
        ImGui::Indent();

        char* structureName = context.field->type.name;
        
        // Structure name editor
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
        ImGui::InputText("Structure", structureName, sizeof(context.field->type.name));
        ImGui::SameLine();

        // Find structure with this name.
        if (!context.schema->structures.count(structureName)) {
            ImGui::Text("Substructure not found: %s", structureName);

            if (ImGui::Button("Create Substructure")) {
                Structure newStructure{};
                newStructure.name = structureName;
                newStructure.size = 256;
                context.schema->structures[structureName] = newStructure;
                *context.structureModified = true;
            }
        } else {
            Engine::BlockPointer* blockPointer = reinterpret_cast<Engine::BlockPointer*>((uintptr_t) context.structureBase + context.field->offset);
            
            if (blockPointer->count == 0) {
                ImGui::Text("Block is empty for this tag.");
                ImGui::Unindent();
                return;
            }

            Structure& substructure = context.schema->structures.at(structureName);
            
            ImGuiStorage* storage = ImGui::GetStateStorage();
            int index = storage->GetInt(ImGui::GetID(substructure.name.c_str()), 0);
        
            uintptr_t baseAddress = reinterpret_cast<uintptr_t>(blockPointer->get<void*>(0));
            void* blockBase = reinterpret_cast<void*>(baseAddress + index * substructure.size);

            if (!Memory::isAllocated(baseAddress)) {
                ImGui::Text("Invalid block base pointer: %p", blockBase);
                ImGui::Unindent();
                return;
            }
    
            RenderContext subcontext = context;
            subcontext.field = context.field;
            subcontext.structureBase = blockBase;
            subcontext.structureSize = substructure.size ? substructure.size : 256;
            subcontext.structure = &substructure;

            renderSizeInput(substructure);
            
            ImGui::SameLine();

            // Index editor
            ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
            ImGui::InputInt("Index", &index);

            if (index >= blockPointer->count) {
                index = 0;
            } else if (index < 0) {
                index = blockPointer->count - 1;
            }

            storage->SetInt(ImGui::GetID(substructure.name.c_str()), index);

            ImGui::Separator();

            char headerLabel[256];
            snprintf(headerLabel, sizeof(headerLabel), "##Content %s", structureName);
            if (ImGui::CollapsingHeader(headerLabel)) {
                ImGui::Indent();
                renderStructure(subcontext);
                ImGui::Unindent();
            }
        }

        ImGui::Unindent();
    }

    void renderField(RenderContext& context) {
        ImGui::PushID(context.field);

        // Offset editor
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
        ImGui::InputScalar("##Offset", ImGuiDataType_U64, &context.field->offset, nullptr, nullptr, "%08X");
        ImGui::SameLine();
        
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8);
        ImGui::InputText("##Name", context.field->name, sizeof(context.field->name));
        ImGui::SameLine();
        
        renderTypeInput(context);
        ImGui::SameLine();

        auto relocationOffset = Engine::mapRelocationOffset();
        auto value = context.field->readString({.relocationOffset = relocationOffset, .structureBase = reinterpret_cast<uintptr_t>(context.structureBase)});
        ImGui::Text("%s", value.c_str());
        
        ImGui::SameLine();
        if (ImGui::Button("x")) {
            context.structure->deleteField(context.field);
            *context.structureModified = true;
        }

        ImGui::SameLine();
        if (ImGui::Button("Copy Address")) {
            uintptr_t address = reinterpret_cast<uintptr_t>(context.structureBase) + context.field->offset;
            ImGui::SetClipboardText(std::to_string(address).c_str());
        }

        if (context.field->type.primitive == PrimitiveTypeRef::StructureReference) {
            renderSubstructure(context);
        }

        ImGui::PopID();
    }

    // Render a single byte of unknown data on the same line.
    void renderUnknown(const RenderContext& context, size_t offset) {
        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 2);
        uint8_t* bytePointer = reinterpret_cast<uint8_t*>(context.structureBase) + offset;
        ImGui::Text("%02X", *bytePointer);

        // If the user right clicks this byte, create a field here. Populate as a uint8_t field.
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
            // Create a new field at this offset
            Field newField;
            snprintf(newField.name, sizeof(newField.name), "field_%X", offset);
            newField.type.primitive = PrimitiveTypeRef::Uint8;
            newField.offset = offset;
            newField.bitOffset = 0;
            snprintf(newField.type.name, sizeof(newField.type.name), "Struct_%X", offset);
            // Add the new field to the structure
            // This requires access to the structure, which is now available as an argument.
            context.structure->fields.push_back(newField);
            *context.structureModified = true;
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

    void renderStructure(RenderContext& context) {
        #define UNKNOWN_ROW_LENGTH 32
        size_t head = 0;
        size_t column = UNKNOWN_ROW_LENGTH;
        
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
            renderUnknown(context, head);
            head++;
            column++;

            if (group & 1) {
                ImGui::PopStyleColor();
            }
        };

        std::sort(context.structure->fields.begin(), context.structure->fields.end(), [](const Field& a, const Field& b) {
            return a.offset < b.offset;
        });

        auto fieldCount = context.structure->fields.size();
        for (size_t i = 0; i < fieldCount; i++) {
            auto& field = context.structure->fields[i];
            
            while (head < field.offset) {
                renderUnknownCell();
                if (*context.structureModified) break;
            }

            if (*context.structureModified) break;
            RenderContext subcontext = context.withField(&field);
            renderField(subcontext);

            if (*context.structureModified) break;

            head += Engine::TagSchema::PrimitiveTypeRefSizes[static_cast<size_t>(field.type.primitive)];
            column = UNKNOWN_ROW_LENGTH;
        }

        while (head < context.structureSize) {
            renderUnknownCell();
            if (*context.structureModified) break;
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

        bool structureModified = false;

        context.schema = schema;
        context.structureBase = tag->getData();
        context.structure = &mainStructure;
        context.structureModified = &structureModified;
        if (mainStructure.size) {
            context.structureSize = mainStructure.size;
        } else if (size) {
            context.structureSize = size;
        } else {
            context.structureSize = 256;
        }

        renderSizeInput(mainStructure);

        renderStructure(context);
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