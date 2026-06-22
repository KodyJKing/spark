// X-Macro for declaring hooks.

#ifndef HOOK
#define HOOK(Name, Ret, Offset, ...)
#endif

HOOK( LoadCheckpoint,       void,     0xBE8A64U                                                         )
HOOK( UpdateCamera,         void,     0xB14380U, float /*dt*/                                           )
HOOK( UpdatePlayerControls,         void,     0xA9A8A4U, float*, float*                                         )
HOOK( UpdatePlayerControlsAndLook, void,     0xA997B8U, uint32_t, uint32_t                                   )
HOOK( RenderFPVModel,       void,     0xB275B8U                                                         )
HOOK( UpdateAllEntities,    void,     0xB35654U                                                         )
HOOK( UpdateEntity,         uint64_t, 0xB3A06CU, uint32_t /*entityHandle*/                              )
HOOK( UpdateWorldBones,     void,     0xB3A614U, uint32_t /*entityHandle*/                              )
HOOK( RenderEntityModels,   void,     0xB48C90U, uint32_t* /*entityHandleScratch*/                      )
HOOK( RenderEntity,         void,     0xB48CD0U, Engine::RenderEntityRequest*                           )
HOOK( RenderBSPAlbedo,      void,     0xC76CD0U                                                         )
HOOK( TryPickInteractable,  void,     0xAD559CU, uint16_t, int16_t, uint32_t /*entityHandle*/, int16_t  )
HOOK( UpdateFlareTransform, void,     0xBE7D70U, uint32_t /*flareHandle*/                               )
// This is a misnoner. It spawns many types of objects. Todo: Rename this everywhere.
HOOK( SpawnProjectile,      uint32_t, 0xB35FD4U, Engine::ProjectileSpawnArgs*, uint32_t /*flags*/       )
HOOK( DamageEntity,         void,     0xB9EA28U, Engine::DamageEvent* /*event*/, uint32_t /*entityHandle*/, uint16_t, uint16_t, int16_t /*hitBoneIndex*/, uint64_t )
HOOK( ConsoleReportError,   void,     0xB20CDCU, const char* /*source*/, const char* /*category*/, const char* /*message*/, const char* /*location*/ )
HOOK( SoundImpulseStart,    void,     0xB32F00U, uint32_t /*soundTagHandle*/, uint32_t /*sourceEntityHandle*/, float /*scale*/ )
