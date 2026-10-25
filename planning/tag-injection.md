### Outline

- Add schema type for pointers to opaque data (called tag_data in CEA decomp).
    - Integrate into dissector.
    - Integrate into minimap.

- Finish mapping all tag types.
- Implement injection for raw map files.

- Add injection UI to spool.
- Sanity test a map with 0 tags injected (tests string and model data relocation).

- Hook __createFile and detour to patched files.
- Rehook when LoadLibraryExW is called on "halo1.dll"

### Finish mapping all tag types

- Add automatic block pointer type size guessing.
- Implement standalone tag browser (outside of injected context).
- Add byte coverage metrics to assure all bytes are accounted for and none are claimed by multiple fields.

### Implement injection for raw map files.
```C++
class RawMapFile {
    public:
    //...
    void injectTags(RawMapFile& other, TagList& tags, RawMapFile& result);
    private:
    //...
}
```

Process A: (needs full tag schemas)
1. Rebuild tag array with injected tags. Add relocation links for all tag data and tag path pointers.
2. Rebuild tag paths array. 
3. Rebuild all tag data blocks. Add relocation links for all TagReference's and ModelData pointers.
4. Fullfill tag data links.
5. Fulfill tag path links.
6. Rebuild vertex buffer.
7. Rebuild index buffer.
8. Fullfill vertex and index buffer links.

Process B: (only needs schemas for injected tags)
- Scan existing tag array.
    - Add links for tag's path string.
    - Add symbol for tag entry.
    - Copy path strings into newPathStrings temp buffer.

    - Scan existing model and bitmap tags. 
        - Copy data into newTextures, newVertices and newIndices temp buffers.
- Append injected tag paths to end of newPathStrings temp buffer.
- Append injected tag entries to end of tag array.
    - Check that we do not overflow the space freed from oldPathStrings.
    - Append texture and model data to temp buffers.
    - Add relocation links for tag path.
    - Add relocation links for tag data.
- Append new tag data.
    - Use space in this order: oldVertices, oldIndices, overflow
    - Add symbol for new tag data.
    - Apply relative relocation fixups to block and opaque pointers.
    - Add links for tag, vertex, index.
- Write temp buffers to end of file. 
    - Note destination for linking.
- Resolve all symbol links.

```C++

struct Symbol {
    enum class SymbolType {
        // ...
    };

    union SymbolData {
        struct TagSymbolData {
            // ...
        };
        //...
        TagSymbolData tag;
        PathSymbolData path;
        // ...
    }
    
    SymbolType type;
    SymbolData data;
};

MultiMap<SymbolHash256, uint64_t> links; // A map from a symbol to all referring addresses.
std::map<SymbolHash256, Symbol> symbols;

void RawMapFile::injectTagsB(RawMapFile& fromMap, TagList& tagsToInject, RawMapFile& newMap) {

}

void RawMapFile::injectTagsA(RawMapFile& fromMap, TagList& tagsToInject, RawMapFile& newMap) {
    // Three maps to keep separatre in your head:
    //  - fromMap: The source of the injected tags.
    //  - toMap/this: The recipient map to be copied.
    //  - newMap: The resulting map after injection.
    
    RawMapFile& fromMap = *this;
    
    // Naively copy this to result.
    newMap.copyBytes(fromMap);

    // Step 1: Rebuild tag array.

    auto insertTagLinks = [&](Tag& tag, char* tagPath, Tag* tagPtr) {
        SymbolHash256 tagSymbolHash = makeTagDataSymbolHash(tag.groupID, tagPath);
        links.insert(tagSymbolHash, reinterpret_cast<uint64_t>(tagPtr));

        SymbolHash256 tagPathSymbolHash = makeTagPathSymbolHash(tagPath);
        links.insert(tagPathSymbolHash, reinterpret_cast<uint64_t>(tagPtr));
    };

    // Create links's for existing tags in the map.
    Tag* existingTags = newMap.getTagArray();
    int existingTagCount = newMap.getTagArraySize();
    for (int i = 0; i < existingTagCount; i++) {
        Tag& tag = existingTags[i];
        char* tagPath = fromMap.getTagPath(tag);
        insertTagLinks(tag, tagPath, &existingTags[i]);
    }

    // Add new tag entries.
    Tag* toTags = newMap.getTagArrayTail();
    for (int i = 0; i < tagsToInject.size(); i++) {
        Tag& fromTag = tagsToInject[i];
        toTags[i] = fromTag;

        // Create links's.
        char* tagPath = fromMap.getTagPath(fromTag);
        insertTagLinks(toTags[i], tagPath, &toTags[i]);
    }

    // Step 2: Rebuild tag paths array.
    // ...
}

```

### Hook __createFile and detour to patched files.
