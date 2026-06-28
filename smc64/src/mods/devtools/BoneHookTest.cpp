#include "BoneHookTest.hpp"
#include "spark/hook/Hooks.hpp"
#include "engine/halo1.hpp"

namespace Mod::DevTools::BoneHookTest {

    void init(Spark::ModId modId) {
        Spark::UpdateWorldBones::addHandler(modId, +[](void*, auto next, uint32_t entityHandle) -> void {
            next(entityHandle);

            auto entity = Engine::getEntityPointer(entityHandle);
            if (!entity) return;
            if (entity->entityCategory != Engine::EntityCategory_Biped) return;
            
            auto boneCount = entity->worldBones.count();
            auto bones = entity->worldBones.get(entity, 0);
            if (boneCount == 0) return;

            float entityZ = entity->pos.z;

            
            float t = (float) GetTickCount64() / 1000.0f;
            float f = 2.0f;
            float scale = sinf(t * f) * 0.4 + 0.6;

            for (size_t i = 0; i < boneCount; i++) {
                auto& bone = bones[i];
                float dz = bone.pos.z - entityZ;
                bone.pos.z = entityZ + dz * scale;
                bone.x.z *= scale; 
                bone.y.z *= scale; 
                bone.z.z *= scale; 
            }

        }, nullptr);
    }

}
