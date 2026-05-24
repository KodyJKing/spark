#include "HookLogMod.hpp"
#include "HookLogState.hpp"
#include "HookLogWindow.hpp"
#include "spark/hook/Hooks.hpp"
#include "spark/RenderBuses.hpp"
#include "engine/halo1.hpp"
#include <iostream>

void HookLogMod::init() {

    Spark::onRenderPauseMenuTabs.addHandler(modId_, +[](void*, auto next) {
        Mod::HookLog::renderPauseMenuTabs();
        next();
    }, nullptr);

    Spark::UpdateCamera::addHandler(modId_, +[](void*, auto next, float dt) {
        if (Mod::HookLog::toggles.UpdateCamera && Mod::HookLog::shouldLog(Mod::HookLog::lastLogTimes.UpdateCamera))
            std::cout << "[HookLog] UpdateCamera\n"
                      << "  dt=" << dt << "\n";
        next(dt);
    }, nullptr);

    Spark::UpdatePlayerControls::addHandler(modId_, +[](void*, auto next, float* param_1, float* param_2) {
        if (Mod::HookLog::toggles.UpdatePlayerControls && Mod::HookLog::shouldLog(Mod::HookLog::lastLogTimes.UpdatePlayerControls))
            std::cout << "[HookLog] UpdatePlayerControls\n"
                      << "  *param_1=" << *param_1 << "\n"  // TODO: identify param semantics
                      << "  *param_2=" << *param_2 << "\n";
        next(param_1, param_2);
    }, nullptr);

    Spark::RenderFPVModel::addHandler(modId_, +[](void*, auto next) {
        if (Mod::HookLog::toggles.RenderFPVModel && Mod::HookLog::shouldLog(Mod::HookLog::lastLogTimes.RenderFPVModel))
            std::cout << "[HookLog] RenderFPVModel\n";
        next();
    }, nullptr);

    Spark::UpdateAllEntities::addHandler(modId_, +[](void*, auto next) {
        if (Mod::HookLog::toggles.UpdateAllEntities && Mod::HookLog::shouldLog(Mod::HookLog::lastLogTimes.UpdateAllEntities))
            std::cout << "[HookLog] UpdateAllEntities\n";
        next();
    }, nullptr);

    Spark::UpdateEntity::addHandler(modId_, +[](void*, auto next, uint32_t entityHandle) -> uint64_t {
        if (Mod::HookLog::toggles.UpdateEntity && Mod::HookLog::shouldLog(Mod::HookLog::lastLogTimes.UpdateEntity))
            std::cout << "[HookLog] UpdateEntity\n"
                      << "  entityHandle=" << (void*)(uintptr_t)entityHandle << "\n";
        return next(entityHandle);
    }, nullptr);

    Spark::UpdateWorldBones::addHandler(modId_, +[](void*, auto next, uint32_t entityHandle) {
        if (Mod::HookLog::toggles.UpdateWorldBones && Mod::HookLog::shouldLog(Mod::HookLog::lastLogTimes.UpdateWorldBones))
            std::cout << "[HookLog] UpdateWorldBones\n"
                      << "  entityHandle=" << (void*)(uintptr_t)entityHandle << "\n";
        next(entityHandle);
    }, nullptr);

    Spark::RenderEntity::addHandler(modId_, +[](void*, auto next, Engine::RenderEntityRequest* req) {
        if (Mod::HookLog::toggles.RenderEntity && Mod::HookLog::shouldLog(Mod::HookLog::lastLogTimes.RenderEntity))
            std::cout << "[HookLog] RenderEntity\n"
                      << "  entityHandle=" << (void*)(uintptr_t)req->entityHandle << "\n";
        next(req);
    }, nullptr);

    Spark::TryPickInteractable::addHandler(modId_, +[](void*, auto next, uint16_t param_1, int16_t param_2, uint32_t entityHandle, int16_t param_4) {
        if (Mod::HookLog::toggles.TryPickInteractable && Mod::HookLog::shouldLog(Mod::HookLog::lastLogTimes.TryPickInteractable))
            std::cout << "[HookLog] TryPickInteractable\n"
                      << "  entityHandle=" << (void*)(uintptr_t)entityHandle << "\n"
                      << "  param_1=" << param_1 << "\n"
                      << "  param_2=" << param_2 << "\n"
                      << "  param_4=" << param_4 << "\n";
        next(param_1, param_2, entityHandle, param_4);
    }, nullptr);

    Spark::UpdateFlareTransform::addHandler(modId_, +[](void*, auto next, uint32_t flareHandle) {
        if (Mod::HookLog::toggles.UpdateFlareTransform && Mod::HookLog::shouldLog(Mod::HookLog::lastLogTimes.UpdateFlareTransform))
            std::cout << "[HookLog] UpdateFlareTransform\n"
                      << "  flareHandle=" << (void*)(uintptr_t)flareHandle << "\n";
        next(flareHandle);
    }, nullptr);

    Spark::SpawnProjectile::addHandler(modId_, +[](void*, auto next, Engine::ProjectileSpawnArgs* args, uint32_t flags) -> uint32_t {
        if (Mod::HookLog::toggles.SpawnProjectile && Mod::HookLog::shouldLog(Mod::HookLog::lastLogTimes.SpawnProjectile))
            std::cout << "[HookLog] SpawnProjectile\n"
                      << "  projectileTagId=" << (void*)(uintptr_t)args->projectileTagId << "\n"
                      << "  unknown1=" << args->unknown1 << "\n"
                      << "  unknown2=" << args->unknown2 << "\n"
                      << "  unknown3=" << args->unknown3 << "\n"
                      << "  ownerEntityHandle=" << (void*)(uintptr_t)args->ownerEntityHandle << "\n"
                      << "  unknown4=" << args->unknown4 << "\n"
                      << "  unknown5=" << args->unknown5 << "\n"
                      << "  spawnPosition=(" << args->spawnPosition.x << ", " << args->spawnPosition.y << ", " << args->spawnPosition.z << ")\n"
                      << "  flags=" << flags << "\n";
        return next(args, flags);
    }, nullptr);

    Spark::DamageEntity::addHandler(modId_, +[](void*, auto next, Engine::DamageEvent* event, uint32_t entityHandle, uint16_t param_3, uint16_t param_4, int16_t hitBoneIndex, uint64_t param_6) {
        if (Mod::HookLog::toggles.DamageEntity && Mod::HookLog::shouldLog(Mod::HookLog::lastLogTimes.DamageEntity))
            std::cout << "[HookLog] DamageEntity\n"
                      << "  entityHandle=" << (void*)(uintptr_t)entityHandle << "\n"
                      << "  param_3=" << param_3 << "\n"
                      << "  param_4=" << param_4 << "\n"
                      << "  hitBoneIndex=" << hitBoneIndex << "\n"
                      << "  param_6=" << (void*)(uintptr_t)param_6 << "\n"
                      << "  event.damageTypeTagHandle=" << (void*)(uintptr_t)event->damageTypeTagHandle << "\n"
                      << "  event.sourceType=" << event->sourceType << "\n"
                      << "  event.flags=" << event->flags << "\n"
                      << "  event.interactorHandle=" << (void*)(uintptr_t)event->interactorHandle << "\n"
                      << "  event.attackerHandle=" << (void*)(uintptr_t)event->attackerHandle << "\n"
                      << "  event.sourceTypeIndex=" << event->sourceTypeIndex << "\n"
                      << "  event.hitPosition=(" << event->hitPosition.x << ", " << event->hitPosition.y << ", " << event->hitPosition.z << ")\n"
                      << "  event.hitDirection=(" << event->hitDirection.x << ", " << event->hitDirection.y << ", " << event->hitDirection.z << ")\n"
                      << "  event.baseDamage=" << event->baseDamage << "\n"
                      << "  event.damageMultiplier=" << event->damageMultiplier << "\n"
                      << "  event.resultHealth=" << event->resultHealth << "\n"
                      << "  event.materialType=" << event->materialType << "\n";
        next(event, entityHandle, param_3, param_4, hitBoneIndex, param_6);
    }, nullptr);
}

void HookLogMod::free() {
}
