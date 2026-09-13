#define THIS addressof(this)

import std.io;

u32 tagDataHeader;
fn setTagDataHeader(u32 value) { tagDataHeader = value; };
fn translatePointer(u32 pointer) { return pointer - 0x50000000 + tagDataHeader; };

u32 modelData;
fn setModelData(u32 value) { modelData = value; };
fn translateVertexDataPointer(u32 pointer) { return pointer + modelData; };

u32 indexData;
fn setIndexData(u32 value) { indexData = value; };
fn translateIndexDataPointer(u32 pointer) { return pointer + indexData; };

fn formatVec3(auto v) { return std::format("{},{},{}", v.x, v.y, v.z); };
fn formatVec4(auto v) { return std::format("{},{},{},{}", v.x, v.y ,v.z, v.w); };

struct Vec3 { float x, y, z; } [[sealed,format("formatVec3")]];
struct Vec4 { float x, y, z, w; } [[sealed,format("formatVec4")]];

namespace TagData {

    // Ghidra: SHORT_Strides[20 (unverified)]
    u16 vertexSizes[20] = { 0x38, 0x20, 0x14, 0x08, 0x44, 0x20, 0x18, 0x24, 0x18, 0x10, 0x14, 0x14, 0x20, 0x08, 0x20, 0x20, 0x24, 0x1C, 0x00, 0x00 };
    
    struct Vertex<auto N> { char raw[N]; } [[sealed]];

    struct VertexDataPointer {
        u16 type;
        u16 [[hidden]];
        u32 count;
        
        // This padding doesn't exist in game. Are they transforming tag data post load? WTF!
        char pad0[8] [[hidden]];
        
        u32 offset;
        
        char raw[vertexSizes[type]*count] @ translateVertexDataPointer(offset);
        //Vertex<vertexSizes[type]> vertices[count] @ translateVertexDataPointer(offset) [[inline]];
    }; 

    struct BlockPointer<T> {
        u32 count [[hidden]];
        u32 offset [[hidden]];
        u32 bullshit [[hidden]];
        T data[count] @ translatePointer(offset) [[inline]];
    };

    struct EffectLocation {
        char name[32];
    };
    
    struct Effect {
        char pad0[0x28] [[hidden]];
        BlockPointer<EffectLocation> locations;
    };
    
    struct ModelGeometryPart {
        char data[0x84] [[hidden]];
        Vec3 centroid @ THIS+0x14;
        VertexDataPointer vertexData @ THIS+0x54;
    };
    
    struct ModelGeometry {
        char data[0x30] [[hidden]];
        BlockPointer<ModelGeometryPart> parts @ THIS+0x24;
    };
    
    struct Model {
       char data[0xAC] [[hidden]];
       BlockPointer<ModelGeometry> geometries @ THIS+0xD0;
    };

    struct Bitmap {
        char data[0x20] [[hidden]]; // Size not known.
        BlockPointer<u32> unknown0 @ THIS+0x54;
        BlockPointer<u32> unknown1 @ THIS+0x60;
    };
    
}

struct Tag {
    char groupId[4];
    char parentGroupId[4];
    char grandparentGroupId[4];
    u32 tagID;
    u32 pathPtr [[hidden]];
    u32 dataPtr [[hidden]];
    char pad0[8] [[hidden]];
    
    char tagPath[] @ translatePointer(pathPtr);
    try {
        if (groupId == "effe") TagData::Effect data @ translatePointer(dataPtr) [[inline]];
        if (groupId == "2dom") TagData::Model data @ translatePointer(dataPtr) [[inline]];
        if (groupId == "mtib") TagData::Bitmap data @ translatePointer(dataPtr) [[inline]];
        else u8 data[32] @ translatePointer(dataPtr);
    } catch {
        char data[32] @ 0x0;
    }
};

struct TagDataHeader {
    setTagDataHeader(addressof(this));
    
    u32 tagArrayPtr;
    u32 checksum;
    u32 scenarioId;
    u32 tagCount;
    u32 modelPartCount;
    
    u32 modelDataOffset; setModelData(modelDataOffset);
    u32 modelPartCount2 [[hidden]];
    u32 vertexDataSize;
    u32 modelDataSize;
    char magic[4];
    
    Tag tag[tagCount] @ translatePointer(tagArrayPtr);
} [[inline]];

struct CacheHeader {
    char magic[4];
    u32 cacheVersion;
    u32 fileSize;
    u32 paddingLength;
    //TagDataHeader *tagDataOffset : u32;
    u32 tagDataOffset;
    u32 tagDataSize;
    char pad0[8] [[hidden]];
    char scenarioName[32];
    char buildVersion[32];
    u16 scenarioType;
    char pad1[2] [[hidden]];
    u32 checksum;
    
    TagDataHeader tagData @ tagDataOffset;
} [[inline]];

CacheHeader header @ 0x00;