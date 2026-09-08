#include "tag.hpp"
#include "common.hpp"
#include "utils/Strings.hpp"
#include "memory/Memory.hpp"
#include "map.hpp"
#include <unordered_map>

#define DEBUG_TAG 1

#ifdef DEBUG_TAG
#include <iostream>
#define LOG(x) std::cout << "[Engine::Tags] " << x << std::endl;
#else
#define LOG(x)
#endif
namespace Engine {

    #define TAG_ARRAY_OFFSET 0x1C34FB0U
    #define TAG_ARRAY_SIZE_OFFSET 0x1C

    // Correct but unused.
    // struct TagArrayHeader {
    //     char pad1[0xC];
    //     uint32_t tagArraySize;
    //     char pad2[0x18];
    // };
    
    char* Tag::getResourcePath() { return (char*) translateMapAddress( resourcePathAddress ); }
    void* Tag::getData() { return (void*) translateMapAddress( dataAddress ); }
    std::string Tag::groupIDStr() {
        auto fourccA = Strings::fourccToString( groupID );
        auto fourccB = Strings::fourccToString( parentGroupID );
        auto fourccC = Strings::fourccToString( grandparentGroupID );
        return "[" + fourccC + " > " + fourccB + " > " + fourccA + "]";
    }
    std::string Tag::classIdStr() {
        auto fourccA = Strings::fourccToString( groupID );
        return fourccA;
    }

    Tag* getTagArray() {
        return *(Tag**) ( dllBase() + TAG_ARRAY_OFFSET );
    }

    void setTagArray(Tag* newArray) {
        *(Tag**) ( dllBase() + TAG_ARRAY_OFFSET ) = newArray;
    }

    uint32_t getTagArraySize() {
        void* tagArray = *(void**) ( dllBase() + TAG_ARRAY_OFFSET );
        uint32_t tagArraySize = *(uint32_t*) ( (uintptr_t)tagArray - TAG_ARRAY_SIZE_OFFSET );
        return tagArraySize;
    }

    Tag* getTag( uint32_t tagID ) {
        if (tagID == NULL_HANDLE)
            return nullptr;
        Tag* tagArray = *(Tag**) ( dllBase() + TAG_ARRAY_OFFSET );
        return &tagArray[tagID & 0xFFFF];
    }

    Tag* findTag( const char* path, uint32_t fourCC) {
        if ( !validTagPath( path ) ) return nullptr;
        Tag* tagArray = *(Tag**) ( dllBase() + TAG_ARRAY_OFFSET );
        for ( uint32_t i = 0; i < 0x10000; i++ ) {
            auto tag = &tagArray[i];
            // if ( !tagExists( tag ) ) break;
            if ( strcmp( tag->getResourcePath(), path ) == 0 && tag->groupID == fourCC ) {
                if ( !tagExists( tag ) ) break; // Keep an eye on this. Might need to go back to checking outside the loop.
                return tag;
            }
        }
        return nullptr;
    }

    Tag* findTag( const char* path, const char* fourCC ) {
        uint32_t fourCCValue = Strings::stringToFourcc( fourCC );
        return findTag( path, fourCCValue );
    }

    bool tagExists( Tag* tag ) {
        return tag && Memory::isAllocated( (uintptr_t) tag->getData() ) && Memory::isAllocated( (uintptr_t) tag->getResourcePath() );
    }

        bool validTagPath( const char* path ) {
        // Must match [a-zA-Z0-9_ \.\\-]+
        // Must be atleast 3 characters long.
        // Also must contain atleast one backslash.
        int backslashCount = 0;
        for ( size_t i = 0; i < 512; i++ ) {
            char c = path[i];
            if ( c == 0 ) 
                return backslashCount > 0 && i > 2;
            if ( 
                !(c >= 'a' && c <= 'z') &&
                !(c >= 'A' && c <= 'Z') &&
                !(c >= '0' && c <= '9') && 
                c != '_'   && c != ' '  &&
                c != '\\'  && c != '.'  && c != '-'
            )
                return false;
            if ( c == '\\' ) 
                backslashCount++;
        }
        return true;
    }

    //////////////////////////////////////////////////////
    // Tag allocation

    bool wasTagArrayMoved() {
        auto tag0 = getTag(0);
        if (!tag0) return true;
        auto resourcePath0 = tag0->getResourcePath();
        auto nextTag = getTag(getTagArraySize());
        return (uintptr_t)resourcePath0 != (uintptr_t)nextTag;
    }

    // Move tags array to a new location with space for new tags.
    Tag* moveTagsArray() {
        if (wasTagArrayMoved()) {
            LOG("Tag array was already moved.");
            return getTag(0);
        }
        
        LOG("Moving tags array to a new location with space for new tags.");

        auto size = getTagArraySize();
        auto sizeBytes = size * sizeof(Tag);
        void* newArray = malloc(sizeBytes);

        if (!newArray) {
            LOG("Failed to allocate new tags array.");
            return nullptr;
        }

        // Copy existing tags to the new array.
        memcpy(newArray, getTag(0), sizeBytes);

        setTagArray((Tag*) newArray);
        return (Tag*) newArray;
    }

    // Assumes tag data is contiguous in memory.
    // Doesn't work for final tags or tags preceeding a gap.
    size_t guessTagSize(Tag* tag) {
        if (!tagExists(tag)) return 0;
        Tag* nextTag = tag + 1;
        if (!tagExists(nextTag)) return 0;
        return (uintptr_t)nextTag->getData() - (uintptr_t)tag->getData();
    }

    // Copy resource path to a location addressible from the map's 32-bit address space.
    const char* allocateTagPath(const char* path) {
        const char* newString = (const char*) Engine::allocateMapMemory(strlen(path) + 1);
        if (!newString) return nullptr;
        strcpy((char*)newString, path);
        return newString;
    }

    Tag* allocateTag(CreateTagOptions options) {
        if (!moveTagsArray()) {
            LOG("Cannot allocate tag. Failed to move tags array.");
            return nullptr;
        }
        
        uint32_t tagCount = getTagArraySize();
        LOG("Current tag count: " << tagCount);
        if (tagCount == 0) return nullptr;

        const char* resourcePath = allocateTagPath(options.resourcePath);
        LOG("Allocated resource path: " << resourcePath);
        if (!resourcePath)
            return nullptr;

        Tag* tag = getTag(tagCount);
        LOG("Allocating new tag at index " << tag);
        tag->groupID = options.groupId;
        tag->parentGroupID = options.parentGroupId;
        tag->grandparentGroupID = options.grandparentGroupId;
        tag->resourcePathAddress = translateToMapAddress((uint64_t) resourcePath);

        return tag;
    }

    void* allocateTagData(size_t size) {
        return Engine::allocateMapMemory(size);
    }

    void freeTagData(void* data) {
        Engine::freeMapMemory(data);
    }

    Tag* cloneTagNaive(Tag* tag, size_t dataSize) {
        if (!tagExists(tag)) return nullptr;
        if (dataSize == 0) dataSize = guessTagSize(tag);

        void* newData = allocateTagData(dataSize);
        if (!newData) return nullptr;
        
        if (!canTranslateToMapAddress((uint64_t) newData)) {
            freeTagData(newData);
            return nullptr;
        }

        memcpy(newData, tag->getData(), dataSize);

        // Create new path with " copy" appended
        char buffer[256];
        strncpy(buffer, tag->getResourcePath(), sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';
        strncat(buffer, " copy", sizeof(buffer) - strlen(buffer) - 1);

        CreateTagOptions options;
        options.groupId = tag->groupID;
        options.parentGroupId = tag->parentGroupID;
        options.grandparentGroupId = tag->grandparentGroupID;
        options.resourcePath = buffer;

        Tag* newTag = allocateTag(options);
        if (!newTag) return nullptr;

        newTag->dataAddress = translateToMapAddress((uint64_t) newData);
        
        return newTag;
    }
    
}
