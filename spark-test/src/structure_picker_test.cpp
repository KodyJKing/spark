#include <catch_amalgamated.hpp>

#include "ui/dissect/Functions.hpp"

using namespace Engine::Map;

namespace {
    struct PickerFixture {
        ImGuiContext* imgui = ImGui::CreateContext();
        Schema schema;
        Context context{nullptr, &schema};
        StructureNode parent;
        FieldNode& node = schema.fields["field"];
        uint8_t memory[32]{};
        FieldRef field{&context, &node, memory, {&context, &parent, memory}};
        ImVec2 comboCenter;
        std::string text;
        bool hasPrimaryText = false;
        bool hasSecondaryText = false;

        PickerFixture() {
            auto& io = ImGui::GetIO();
            io.IniFilename = nullptr;
            io.DisplaySize = ImVec2(1200, 900);
            io.DeltaTime = 1.0f / 60.0f;
            unsigned char* pixels;
            int width, height;
            io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
            parent.id = "parent";
            node.id = "field";
            node.type.type = Type::Structure;
        }

        ~PickerFixture() { ImGui::DestroyContext(imgui); }

        void addStructure(const char* id, const char* name) {
            auto& structure = schema.structures[id];
            structure.id = id;
            snprintf(structure.name, sizeof(structure.name), "%s", name);
            structure.size = 16;
        }

        bool frame() {
            ImGui::NewFrame();
            ImGui::SetNextWindowPos(ImVec2(0, 0));
            ImGui::SetNextWindowSize(ImVec2(1100, 800));
            ImGui::Begin("Picker test", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoSavedSettings);
            auto start = ImGui::GetCursorScreenPos();
            comboCenter = ImVec2(start.x + 100, start.y + ImGui::GetFrameHeight() * 0.5f);
            ImGui::LogToBuffer();
            bool changed = SparkLib::UI::DissectTag::renderStructureTypePicker(field);
            text = imgui->LogBuffer.c_str();
            ImGui::LogFinish();
            hasPrimaryText = hasSecondaryText = false;
            for (const auto& vertex : ImGui::GetWindowDrawList()->VtxBuffer) {
                hasPrimaryText |= vertex.col == ImGui::GetColorU32(ImGuiCol_Text);
                hasSecondaryText |= vertex.col == ImGui::GetColorU32(ImGuiCol_TextDisabled);
            }
            ImGui::End();
            ImGui::Render();
            return changed;
        }

        bool click(ImVec2 position) {
            auto& io = ImGui::GetIO();
            io.AddMousePosEvent(position.x, position.y);
            io.AddMouseButtonEvent(0, true);
            bool changed = frame();
            io.AddMouseButtonEvent(0, false);
            return frame() || changed;
        }

        ImGuiWindow* open() {
            frame();
            frame();
            click(comboCenter);
            frame();
            REQUIRE_FALSE(imgui->OpenPopupStack.empty());
            auto* popup = imgui->OpenPopupStack.back().Window;
            REQUIRE(popup != nullptr);
            return popup;
        }

        bool chooseRow(ImGuiWindow* popup, int row) {
            return click(ImVec2(popup->DC.CursorStartPos.x + 20,
                popup->DC.CursorStartPos.y + row * ImGui::GetTextLineHeightWithSpacing()
                    + ImGui::GetTextLineHeight() * 0.5f));
        }

        bool chooseCreate(ImGuiWindow* popup) {
            return click(ImVec2(popup->DC.CursorStartPos.x + 20,
                popup->DC.CursorPosPrevLine.y + ImGui::GetTextLineHeight() * 0.5f));
        }
    };
}

TEST_CASE("Dissector structure picker assigns existing types by ID and persists the reference", "[structure_picker]") {
    PickerFixture ui;
    ui.node.type.type = GENERATE(Type::Structure, Type::BlockPointer);
    ui.addStructure("aaaaaaaaaaaaaaaa", "Shared");
    ui.addStructure("bbbbbbbbbbbbbbbb", "Shared");
    auto* popup = ui.open();
    REQUIRE_THAT(ui.text, Catch::Matchers::ContainsSubstring("aaaaaaaaaaaaaaaa"));
    REQUIRE_THAT(ui.text, Catch::Matchers::ContainsSubstring("bbbbbbbbbbbbbbbb"));
    REQUIRE(ui.chooseRow(popup, 1));
    REQUIRE(ui.node.type.ref == "bbbbbbbbbbbbbbbb");
    REQUIRE(ui.schema.structures.size() == 2);
    REQUIRE(ui.field.getReferencedType() == &ui.schema.structures.at("bbbbbbbbbbbbbbbb"));
    auto restored = Schema::fromJsonString(ui.schema.toJsonString());
    REQUIRE(restored.fields.at("field").type.ref == "bbbbbbbbbbbbbbbb");

    ui.frame();
    REQUIRE_THAT(ui.text, Catch::Matchers::ContainsSubstring("Shared"));
    REQUIRE_THAT(ui.text, Catch::Matchers::ContainsSubstring("bbbbbbbbbbbbbbbb"));
    REQUIRE(ui.hasPrimaryText);
    REQUIRE(ui.hasSecondaryText);

    popup = ui.open();
    REQUIRE(ui.chooseRow(popup, 0));
    REQUIRE(ui.node.type.ref == "aaaaaaaaaaaaaaaa");
    REQUIRE(ui.schema.structures.size() == 2);
}

TEST_CASE("Dissector structure picker creates and assigns a fresh type", "[structure_picker]") {
    PickerFixture ui;
    ui.node.type.type = GENERATE(Type::Structure, Type::BlockPointer);
    SECTION("no existing structures") {}
    SECTION("replace an assigned type") {
        ui.addStructure("existing", "Existing");
        ui.node.type.ref = "existing";
    }
    auto before = ui.schema.structures.size();
    auto* popup = ui.open();
    REQUIRE_THAT(ui.text, Catch::Matchers::ContainsSubstring("Create new structure..."));
    REQUIRE(ui.chooseCreate(popup));
    REQUIRE(ui.schema.structures.size() == before + 1);
    REQUIRE(ui.node.type.ref != NullId);
    REQUIRE(ui.node.type.ref != "existing");
    auto* created = ui.field.getReferencedType();
    REQUIRE(created != nullptr);
    REQUIRE(created->id == ui.node.type.ref);
    REQUIRE(created->size == (ui.node.type.type == Type::Structure ? 4 : 256));
    REQUIRE_FALSE(created->complete);
}

TEST_CASE("Dissector structure picker sorts by name and recovers a missing reference", "[structure_picker]") {
    PickerFixture ui;
    ui.node.type.ref = "missing";
    ui.addStructure("a", "Zulu");
    ui.addStructure("z", "Alpha");
    ui.frame();
    REQUIRE_THAT(ui.text, Catch::Matchers::ContainsSubstring("missing"));
    auto* popup = ui.open();
    REQUIRE(ui.chooseRow(popup, 0));
    REQUIRE(ui.node.type.ref == "z");
}
