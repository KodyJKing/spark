#include "LevelEditSave.hpp"

#ifdef ENABLE_LEVEL_EDITOR

#include <fstream>
#include <filesystem>

namespace Mod::Mario::LevelEdit {

static std::filesystem::path savePath(const std::string& levelName) {
    std::filesystem::path src = __FILE__; // .../level-edit/LevelEditSave.cpp
    return src.parent_path() / "level-edits" / (levelName + ".hpp");
}

void saveLevelEdits(EditorState& state) {
    if (!state.currentEdits) return;
    const std::string& name = state.currentContext.levelName;
    if (name.empty()) return;

    std::filesystem::path path = savePath(name);
    std::ofstream f(path);
    if (!f) {
        fprintf(stderr, "[LevelEdit] Failed to open %s for writing\n", path.string().c_str());
        return;
    }

    auto& obbs = state.currentEdits->orientedBoundingBoxes;
    std::string NS = name;
    if (!NS.empty()) NS[0] = (char)toupper((unsigned char)NS[0]);

    f << "#pragma once\n";
    f << "#include \"../MarioLevelEdit.hpp\"\n\n";
    f << "namespace Mod::Mario::LevelEdit::" << NS << "Edits {\n";
    f << "    inline LevelEdits s_edits = {\n";
    f << "        .orientedBoundingBoxes = {\n";
    for (const auto& obb : obbs) {
        f << "            { "
          << "{ " << obb.center.x      << "f, " << obb.center.y      << "f, " << obb.center.z      << "f }, "
          << "{ " << obb.halfExtents.x << "f, " << obb.halfExtents.y << "f, " << obb.halfExtents.z << "f }, "
          << "{ " << obb.orientation.x << "f, " << obb.orientation.y << "f, " << obb.orientation.z << "f } },\n";
    }
    f << "        }\n";
    f << "    };\n";
    f << "}\n";

    printf("[LevelEdit] Saved %zu OBB(s) to %s\n", obbs.size(), path.string().c_str());
}

} // namespace Mod::Mario::LevelEdit

#endif // ENABLE_LEVEL_EDITOR
