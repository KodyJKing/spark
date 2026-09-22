#pragma once
#include <cstdint>

// Abandoning this. Keeping briefly for reference.
namespace Mod::DevTools::InjectBitmap {
    
    // Initializes data and hooks needed for bitmap injection.
    void init();

    // Injects a bitmap into the game's texture cache from a file.
    // Returns the handle to the injected tag.
    uint32_t injectBitmapFromFile(const char* filePath, const char* tagName);

    // Renders the user interface for the bitmap injection tool.
    void renderUI();

};
