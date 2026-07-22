#pragma once

namespace Mod::Mario {
    // Renders the "Sound Board" button. Call from inside a tab item.
    void renderSoundBoardButton();
    // Renders the sound board window. Call once per frame (no-op when closed).
    void renderSoundBoard();
}
