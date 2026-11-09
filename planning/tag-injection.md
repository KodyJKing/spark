### Outline

x Add schema type for pointers to opaque data (called tag_data in CEA decomp).
    x Integrate into dissector.
    x Integrate into minimap.
    - Model internal/external DataPointer
        ! Raw data can live inline in a tag or in the "Raw Data" section of the map.
        ! Internal/external distinction determines if we need to update offsets during injection.

- Finish mapping all tag types.
- Implement injection for raw map files.

- Add injection UI to spool.
- Sanity test a map with 0 tags injected (tests string and model data relocation).

- Hook haloCreateFile and detour to patched files.
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

Process C:

- Terminology:
    - Host map: The map we are injecting tags into.
    - Guest map: The map containing the tags to be injected.
    - Host tag: Tags that already exist in the host map.
    - Guest tag: Tags that are being injected from the guest map.
    
- Assumptions:
    1. Engine will allow non-contiguous bitmaps, allowing us to use a separate bitmap-buffer for guest tags.
    2. Vertex/index buffers are the final portion of the "Raw Data" section.
    3. Engine will accept broken TagReference.path pointers for SBSP tag data.

- Caveats:
    - Does not visit SBSP tag data. TagReference.path pointers will be broken. (3)

- Pass 1: Fill out temporary buffers with host and guest data.
    - Copy existing data into temp buffers:
        - Vertex buffers
        - Index buffers
        - Tag names
        ! Everything up to the vertex buffer portion of "Raw Data" is copied as-is.
            ! We'll call this the "fixed-section".
        - Copy TagData ("Metadata") section to temp buffer.
    - Create guest bitmap buffer.
    - For each guest tag:
        - Push name to name buffer
        - Push to bitmap buffer
        - Push to vertex/index buffers
        - Push tag entry to tag array

- Pass 2: Finalize layout and fix host offsets/pointers.
    - Copy fixed-section to result buffer.
    - Append bitmap, vertex and index buffers to result buffer. (2)
        - Note where these land so we can update TagDataHeader.
    - Append TagDataHeader to result buffer and update values as needed. (2)
        - Note where this lands so we can update CacheHeader.
    - Append TagArray to result buffer.
    - Append TagNames to result buffer.
        - Note where this lands so we can update name ptrs in TagArray.
    - Append old "Metadata" section to result buffer.
        - Note where this lands so we can update data ptrs in TagArray.

    - Update name and data pointers in TagArray (host tags only).
    - Apply relocation proceedure to host tags' data.
        - Apply TagData relocation offset to host tag data's internal pointers.
        - Apply TagNames relocation offset to host tag data's tag-reference name pointers.

    ! Now we know where every section in the resulting map lands.
    ! The map should be in a valid and playable state here.

- Pass 3: Append guest tag data.

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


```

### Hook __createFile and detour to patched files.
