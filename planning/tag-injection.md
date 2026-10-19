### Outline

- Finish mapping all tag types.
- Implement injection for raw map files.
- Hook __createFile and detour to patched files.

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

Process:
1. Rebuild tag array with injected tags. Add relocation iou's for all tag data and tag path pointers.
2. Rebuild tag paths array. 
3. Rebuild all tag data blocks. Add relocation iou's for all TagReference's and ModelData pointers.
4. Fullfill tag data iou's.
5. Fulfill tag path iou's.
6. Rebuild vertex buffer.
7. Rebuild index buffer.
8. Fullfill vertex and index buffer iou's.

```C++

enum class SymbolType {
    // ...
};

MultiMap<SymbolHash256, uint64_t> ious; // A map from a symbol to all referring addresses.

void RawMapFile::injectTags(RawMapFile& fromMap, TagList& tagsToInject, RawMapFile& newMap) {
    // Three maps to keep separatre in your head:
    //  - fromMap: The source of the injected tags.
    //  - toMap/this: The recipient map to be copied.
    //  - newMap: The resulting map after injection.
    
    RawMapFile& fromMap = *this;
    
    // Naively copy this to result.
    newMap.copyBytes(fromMap);

    // Step 1: Rebuild tag array.

    auto insertTagIOUS = [&](Tag& tag, char* tagPath, Tag* tagPtr) {
        SymbolHash256 tagSymbolHash = makeTagDataSymbolHash(tag.groupID, tagPath);
        ious.insert(tagSymbolHash, reinterpret_cast<uint64_t>(tagPtr));

        SymbolHash256 tagPathSymbolHash = makeTagPathSymbolHash(tagPath);
        ious.insert(tagPathSymbolHash, reinterpret_cast<uint64_t>(tagPtr));
    };

    // Create iou's for existing tags in the map.
    Tag* existingTags = newMap.getTagArray();
    int existingTagCount = newMap.getTagArraySize();
    for (int i = 0; i < existingTagCount; i++) {
        Tag& tag = existingTags[i];
        char* tagPath = fromMap.getTagPath(tag);
        insertTagIOUS(tag, tagPath, &existingTags[i]);
    }

    // Add new tag entries.
    Tag* toTags = newMap.getTagArrayTail();
    for (int i = 0; i < tagsToInject.size(); i++) {
        Tag& fromTag = tagsToInject[i];
        toTags[i] = fromTag;

        // Create iou's.
        char* tagPath = fromMap.getTagPath(fromTag);
        insertTagIOUS(toTags[i], tagPath, &toTags[i]);
    }

    // Step 2: Rebuild tag paths array.
    // ...
}
```

### Hook __createFile and detour to patched files.
