#include "MarioSoundBoard.hpp"

#include "imgui.h"
#include "decomp/sm64.h"
#include "libsm64.h"

#include <cstring>

namespace Mod::Mario {

    static bool s_open = false;

    struct SoundEntry {
        const char* name;
        int32_t     bits;
    };

    static const SoundEntry s_sounds[] = {
        #define ENTRY(x) { #x, (int32_t)(x) },
        #include "MarioSounds.def"
        #undef ENTRY
    };

    static constexpr int s_soundCount = (int)(sizeof(s_sounds) / sizeof(s_sounds[0]));

    void renderSoundBoardButton() {
        if (ImGui::Button("Sound Board"))
            s_open = true;
    }

    void renderSoundBoard() {
        if (!s_open) return;

        ImGui::SetNextWindowSize(ImVec2(420, 520), ImGuiCond_FirstUseEver);
        if (!ImGui::Begin("Mario Sound Board", &s_open)) {
            ImGui::End();
            return;
        }

        static char filter[128] = "";
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText("##search", filter, sizeof(filter));
        ImGui::Separator();

        ImGui::BeginChild("##sounds");
        for (int i = 0; i < s_soundCount; i++) {
            if (filter[0] && !strstr(s_sounds[i].name, filter))
                continue;
            if (ImGui::Button(s_sounds[i].name, ImVec2(-FLT_MIN, 0)))
                sm64_play_sound_global(s_sounds[i].bits);
        }
        ImGui::EndChild();

        ImGui::End();
    }

}
