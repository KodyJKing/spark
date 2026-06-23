#include "entity_functions.hpp"
#include "entity_list.hpp"

#include "memory/Memory.hpp"

namespace Engine {

    Tag* Entity::tag() { return Engine::getTag( tagID ); }
    char* Entity::getTagResourcePath() {
        auto pTag = tag();
        if ( !pTag ) return nullptr;
        return pTag->getResourcePath();
    };
    bool Entity::fromResourcePath( const char* str ) {
        auto resourcePath = getTagResourcePath();
        return resourcePath && strncmp( resourcePath, str, 1024 ) == 0;
    }

    std::vector<QuatTransform> Entity::copyBoneTransforms() {
        std::vector<QuatTransform> result;
        auto boneCount = this->bones.count();
        if ( !boneCount ) return result;
        auto bones = this->getBoneTransforms();
        for ( uint16_t i = 0; i < boneCount; i++ )
            result.push_back( bones[i] );
        return result;
    }

    bool isReloading( Entity* entity ) { return entity->weaponAnim == 0x05; }
    bool isDoingMelee( Entity* entity ) { return entity->weaponAnim == 0x07; }

    bool isTransport( Entity* entity ) {
        return
            entity->fromResourcePath( "vehicles\\pelican\\pelican" ) ||
            entity->fromResourcePath( "vehicles\\c_dropship\\c_dropship" );
    }

    bool isRidingTransport( Entity* entity ) {
        if ( !entity )
            return false;
        auto vehicleRec = getEntityRecord( entity->parentHandle );
        if ( !vehicleRec )
            return false;
        auto vehicle = getEntityPointer( vehicleRec );
        return isTransport( vehicle );
    }

    uint16_t boneCount(void* anim) {
        // Todo: Create a type for anim.
        return Memory::safeRead<uint16_t>( (uintptr_t) anim + 0x2c ).value_or( 0 );
    }

    // uint16_t Entity::boneCount() {
    //     return bones.count();

    //     // This is a good example of how to traverse animation tag data, so I'm keeping it around.
    //     //
    //     // auto animSetTag = Engine::getTag(animSetTagID);
    //     // if ( !animSetTag ) return 0;
    //     // void* animSetData = animSetTag->getData();
    //     // if ( !animSetData ) return 0;
    //     // uint32_t animArrayAddress = Memory::safeRead<uint32_t>( (uintptr_t) animSetData + 0x78 ).value_or( 0 );
    //     // uintptr_t animArray = Engine::translateMapAddress( animArrayAddress );
    //     // if ( !animArray ) return 0;
    //     // int animIndex = 0;
    //     // size_t sizeOfAnimation = 0xb4;
    //     // uintptr_t anim = animArray + animIndex * sizeOfAnimation;
    //     // return Engine::boneCount( (void*) anim );
    // }

    bool entityValid( Entity* entity ) {
        return entity && entity->tagID != NULL_HANDLE;
    }

    bool entityValid( uint32_t entityHandle ) {
        auto entity = getEntityPointer( entityHandle );
        if (!entity) return false;
        return entityValid( entity );
    }

}
