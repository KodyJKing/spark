#pragma once

#include <stdint.h>
#include <optional>
#include "camera.hpp"
#include "camera_controller.hpp"
#include "entity/index.hpp"
#include "spark/SparkAPI.h"

namespace Engine {
    namespace PlayerActionFlags {
        const uint32_t crouch     = 1 << 0;
        const uint32_t jump       = 1 << 1;
        const uint32_t flash      = 1 << 4;
        const uint32_t melee      = 1 << 7;
        const uint32_t reload     = 1 << 10;
        const uint32_t shoot      = 1 << 11;
        const uint32_t grenade1   = 1 << 12;
        const uint32_t grenade2   = 1 << 13;
    }
    struct PlayerController {
        uint8_t pad_0000[16];  // 0000
        uint32_t playerHandle; // 0010
        uint32_t actions;      // 0014
        uint8_t pad_0018[4];   // 0018
        float yaw;             // 001C
        float pitch;           // 0020
        float walkY;           // 0024
        float walkX;           // 0028
        float gunTrigger;      // 002C
        uint8_t pad_0030[8];   // 0030
        float targetIndicator; // 0038
        uint8_t pad_003C[360]; // 003C
        uint32_t targetHandle; // 01A4
    };

    SPARK_API Camera* getPlayerCameraPointer();
    SPARK_API CameraController *getPlayerCameraControllerPointer();
    SPARK_API bool isPlayerInputDisabled();
    SPARK_API void enterThirdPerson();
    SPARK_API uint32_t getPlayerHandle();
    SPARK_API uint32_t getPlayerHeldWeaponHandle();
    SPARK_API PlayerController * getPlayerControllerPointer();

    SPARK_API EntityRecord* getPlayerRecord();
    SPARK_API Entity *getPlayerEntity();
    SPARK_API Inventory *getPlayerInventory();
    SPARK_API std::optional<Vec3> getPlayerPosition();
    SPARK_API std::optional<Vec3> getPlayerVelocity();
    SPARK_API bool isPlayerHandle(uint32_t entityHandle);
    SPARK_API bool isPlayerControlled( EntityRecord* rec );
    SPARK_API bool isPlayerInVehicle();
}
