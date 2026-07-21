#include "LevelEditSave.hpp"

#ifdef ENABLE_LEVEL_EDITOR

#include "imgui.h"
#include <sstream>

namespace Mod::Mario::LevelEdit {

void copyLevelEditsToClipboard(EditorState& state) {
    if (!state.currentEdits) return;

    std::ostringstream f;

    auto& obbs    = state.currentEdits->orientedBoundingBoxes;
    uint64_t sig  = state.currentContext.bspSignature;

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

    // Emit a single map entry ready to paste into level-edits/index.hpp.
    char sigBuf[32];
    snprintf(sigBuf, sizeof(sigBuf), "0x%016llXULL", (unsigned long long)sig);

    f << "// BSP 0x" << sigBuf << "\n";
    f << "{ " << sigBuf << ", LevelEdits {\n";
    f << "    .orientedBoundingBoxes = {\n";
    for (const auto& obb : obbs) {
        f << "        { "
          << "{ " << flt(obb.center.x)      << "f, " << flt(obb.center.y)      << "f, " << flt(obb.center.z)      << "f }, "
          << "{ " << flt(obb.halfExtents.x) << "f, " << flt(obb.halfExtents.y) << "f, " << flt(obb.halfExtents.z) << "f }, "
          << "{ " << flt(obb.orientation.x) << "f, " << flt(obb.orientation.y) << "f, " << flt(obb.orientation.z) << "f } },\n";
    }
    f << "    }\n";
    f << "}},\n";

    ImGui::SetClipboardText(f.str().c_str());
}

} // namespace Mod::Mario::LevelEdit

#endif // ENABLE_LEVEL_EDITOR
