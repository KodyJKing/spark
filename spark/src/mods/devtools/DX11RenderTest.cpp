#include "engine/halo1.hpp"
#include "spark/overlay/ESP.hpp"
#include "spark/hook/Hooks.hpp"

namespace Mod::DevTools::DX11RenderTest {
    void initHandlers(Spark::ModId modId) {
        // Spark::RenderBSPAlbedo::addHandler(modId, +[](void*, Spark::RenderBSPAlbedo::Cursor next) {
        //     next();

        //     using namespace Spark::Overlay;

        //     ESP::DX11::begin();

        //     // Draw a red line from origin to every entity for testing.
        //     Engine::foreachEntityRecord([&](auto rec) {
        //         auto entity = rec->entity();
        //         if (!entity) return;
        //         Vec3 pos = entity->pos;
        //         ESP::DX11::drawLine({0, 0, 100}, pos, IM_COL32(255, 0, 0, 255));
        //     });
            
        //     ESP::DX11::end();
        // }, nullptr);
    }
}
