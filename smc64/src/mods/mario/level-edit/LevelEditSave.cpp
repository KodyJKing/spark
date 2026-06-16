#include "LevelEditSave.hpp"

#ifdef ENABLE_LEVEL_EDITOR

#include "imgui.h"
#include <sstream>

namespace Mod::Mario::LevelEdit {

void copyLevelEditsToClipboard(EditorState& state) {
    if (!state.currentEdits) return;
    const std::string& name = state.currentContext.levelName;
    if (name.empty()) return;

    std::ostringstream f;

    auto& obbs = state.currentEdits->orientedBoundingBoxes;
    std::string NS = name;
    if (!NS.empty()) NS[0] = (char)toupper((unsigned char)NS[0]);

    // Format a float literal that always has a decimal point so "90f" never appears.
    auto flt = [](float v) -> std::string {
        char buf[32];
        snprintf(buf, sizeof(buf), "%g", v);
        std::string s = buf;
        // If no '.', 'e', or 'E' is present, append '.' so the 'f' suffix is valid.
        if (s.find('.') == std::string::npos &&
            s.find('e') == std::string::npos &&
            s.find('E') == std::string::npos) {
            s += '.';
        }
        return s;
    };

    f << "#pragma once\n";
    f << "#include \"../MarioLevelEdit.hpp\"\n\n";
    f << "namespace Mod::Mario::LevelEdit::" << NS << "Edits {\n";
    f << "    inline LevelEdits s_edits = {\n";
    f << "        .orientedBoundingBoxes = {\n";
    for (const auto& obb : obbs) {
        f << "            { "
          << "{ " << flt(obb.center.x)      << "f, " << flt(obb.center.y)      << "f, " << flt(obb.center.z)      << "f }, "
          << "{ " << flt(obb.halfExtents.x) << "f, " << flt(obb.halfExtents.y) << "f, " << flt(obb.halfExtents.z) << "f }, "
          << "{ " << flt(obb.orientation.x) << "f, " << flt(obb.orientation.y) << "f, " << flt(obb.orientation.z) << "f } },\n";
    }
    f << "        }\n";
    f << "    };\n";
    f << "}\n";

    ImGui::SetClipboardText(f.str().c_str());
}

} // namespace Mod::Mario::LevelEdit

#endif // ENABLE_LEVEL_EDITOR
