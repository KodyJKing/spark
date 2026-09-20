#include "map_file.hpp"
#include <cstring>
#include <fstream>

namespace Engine::MapFile {

    bool MapFile::tagGroupIs(const Tag& tag, const char* fourCc) const {
        return std::memcmp(tag.groupId, fourCc, 4) == 0;
    }

    bool MapFile::loadFromFile(const std::string& path) {
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) return false;

        std::streamsize size = file.tellg();
        if (size <= 0) return false;
        file.seekg(0, std::ios::beg);

        m_buffer.resize(static_cast<size_t>(size));
        if (!file.read(reinterpret_cast<char*>(m_buffer.data()), size)) return false;

        CacheHeader* header = getCacheHeader();
        if (!header) return false;
        m_tagDataHeaderOffset = header->tagDataOffset;

        return getTagDataHeader() != nullptr;
    }

    bool MapFile::isValidRange(uint64_t offset, uint64_t size) const {
        return offset + size <= m_buffer.size();
    }

    uint32_t MapFile::computePointerOffset(uint32_t pointer) const {
        return pointer - 0x50000000u + m_tagDataHeaderOffset;
    }

    CacheHeader* MapFile::getCacheHeader() {
        if (!isValidRange(0, sizeof(CacheHeader))) return nullptr;
        return reinterpret_cast<CacheHeader*>(m_buffer.data());
    }

    TagDataHeader* MapFile::getTagDataHeader() {
        if (!isValidRange(m_tagDataHeaderOffset, sizeof(TagDataHeader))) return nullptr;
        return reinterpret_cast<TagDataHeader*>(m_buffer.data() + m_tagDataHeaderOffset);
    }

    uint8_t* MapFile::translatePointer(uint32_t pointer) {
        uint32_t offset = computePointerOffset(pointer);
        if (!isValidRange(offset, 0)) return nullptr;
        return m_buffer.data() + offset;
    }

    uint8_t* MapFile::translateVertexDataPointer(uint32_t pointer) {
        TagDataHeader* tagData = getTagDataHeader();
        if (!tagData) return nullptr;
        uint64_t offset = static_cast<uint64_t>(pointer) + tagData->modelDataOffset;
        if (!isValidRange(offset, 0)) return nullptr;
        return m_buffer.data() + offset;
    }

    uint32_t MapFile::getTagCount() {
        TagDataHeader* tagData = getTagDataHeader();
        return tagData ? tagData->tagCount : 0;
    }

    Tag* MapFile::getTag(uint32_t index) {
        TagDataHeader* tagData = getTagDataHeader();
        if (!tagData || index >= tagData->tagCount) return nullptr;

        uint32_t arrayOffset = computePointerOffset(tagData->tagArrayPtr);
        uint64_t elementOffset = static_cast<uint64_t>(arrayOffset) + static_cast<uint64_t>(index) * sizeof(Tag);
        if (!isValidRange(elementOffset, sizeof(Tag))) return nullptr;
        return reinterpret_cast<Tag*>(m_buffer.data() + elementOffset);
    }

    const char* MapFile::getTagPath(const Tag& tag) {
        uint32_t offset = computePointerOffset(tag.pathPtr);
        if (!isValidRange(offset, 0)) return nullptr;

        const char* path = reinterpret_cast<const char*>(m_buffer.data() + offset);
        size_t maxLen = m_buffer.size() - offset;
        if (!std::memchr(path, '\0', maxLen)) return nullptr; // not null-terminated within the buffer
        return path;
    }

    void* MapFile::getTagData(const Tag& tag) {
        uint32_t offset = computePointerOffset(tag.dataPtr);
        if (!isValidRange(offset, 0)) return nullptr;
        return m_buffer.data() + offset;
    }

    std::pair<uint8_t*, size_t> MapFile::getVertexData(const TagData::VertexDataPointer& vertexData) {
        constexpr size_t kVertexSizeCount = sizeof(TagData::kVertexSizes) / sizeof(TagData::kVertexSizes[0]);
        if (vertexData.type >= kVertexSizeCount) return { nullptr, 0 };

        size_t size = static_cast<size_t>(TagData::kVertexSizes[vertexData.type]) * vertexData.count;
        uint8_t* ptr = translateVertexDataPointer(vertexData.offset);
        if (!ptr || !isValidRange(static_cast<uint64_t>(ptr - m_buffer.data()), size)) return { nullptr, 0 };
        return { ptr, size };
    }

}
