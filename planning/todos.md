- Load a bitmap from a .map file by tag-name.
    x My tag and map data accessor code is hard coded to certain map offsets.
        - Create a V2 API, eventually move V1 API to V2 backing and slowly remove V1 calls.
            - Drive new access API from tag schema JSON?
    - Need a way to override source read from in startLoadingBitmap/cacheReadFile
        - Hook both. 
            - When loading in startLoadingBitmap, set a global to inform cacheReadFile which tag is being read.
            - When reading in cacheReadFile, if the tag being read from is one of ours, override the source.

- Load gbxmodel from .map file

- MapFile work
    - Adapt TagBrowser and TagDisect to work with MapFiles. (next item contains prereq)

    - Generic copy tag data routine

        - I would need an abstraction that knows how to translate to/from map-relative addresses for both raw-cache-files and the runtime-format.
            - I haven't cleanly modeled the distinction between different relative addresses:
                - Map relative
                - Tag array relative
                - Model data relative
                    - Vertex data relative
                    - Index data relative
        
        - Accurately sizing tag data and blocks requires a schema for each tag type.
            - I would need to tidy up the schemas I've created 
            - and figure out how I'm distributing them.
                - As loose json file(s)?
                    - This might actually be the winner because then modders can tweak them without building Spark.
                - Or raw strings in the source?
                - As a compiled resource?
        
        - Might look like this:
            ```C++
                // A clean workflow that handles uninject and re-inject gracefully is critical for this feature.
                // Operations must be idempotent.

                // Assume one global schema for now. 
                // Parsed from /schemas directory distributed with Spark
                // Should have an environment variable for alternate /schemas source (so during dev I can point it at source dir)
                TagSchemaCollection* getTagSchemas();

                // Buffers managed by Spark for texture, vertex and index data.
                // Texture buffer mapping/loading is detoured for any Spark owned tags.
                // Vertex/Index buffer binding (not mapping/loading) is detoured too.
                // Buffers should be re-discovered upon re-injection. (I'm thinking bookkeeping lives in named shared memory).
                // While uninjected, custom tags will point at junk data (I have tested this, it't just ugly, doesn't crash)

                // POD arena, not std::vector: layout must stay stable across rebuilds since the
                // persisted object is reinterpreted by the next injected build. A live std::vector
                // would couple every build to an identical STL ABI (Debug/Release layouts even differ).
                struct Arena {
                    uint8_t* data;
                    size_t size;
                    size_t capacity;
                };

                struct SparkBuffers {
                    // Tags are never removed, so we just append to these buffers and hand out fixed offsets into them.
                    // Callers must re-resolve offset->pointer at use time (append may move `data`) and pad appends for alignment.
                    Arena textureBuffer;
                    Arena vertexBuffer;
                    Arena indexBuffer;
                };

                struct SparkPersistedState {
                    // Must be careful about leaving pointers DLL memory in SparkBuffers since it survives re-injection.
                    SparkBuffers* buffers;
                };

                SparkBuffers* getSparkBuffers();

                // Implemented via named shared memory. Survives uninject/re-inject cycles. Does not survive process exit.
                SparkPersistedState* getPersistedState();

                class MapFile {
                    public:
                    enum Base {
                        Base_Map,
                        Base_Tag,
                        Base_Vertex,
                        Base_Index, // (follows right after vertex data for RawMapFile)
                    };

                    virtual void* fromRelative(Base base, void* ptr);
                    virtual void* toRelative(Base base, void* ptr);

                    // Copy methods are only implemented for RuntimeMapFile.

                    // Copy a tag from another map. Return the new tag handle.
                    // Handles all copying defaults and returns a ready to use tag.
                    //   - Copies referenced tags if absent from the target map.
                    //   - Patches tags if already present.
                    //     - Leave tag data in place if same size.
                    //     - Reallocate if size differs.
                    //   - Tags are matched (between maps) by their (groupId, path) tuple.
                    //   - Must be idempotent so re-injection during development doesn't break anything.
                    //   - New tags are left in place at uninjection time.
                    uint32_t copyTag(MapFile* sourceMap, uint32_t sourceTagHandle);

                    private:
                    // Copy tag data to dest buffer.
                    // Implementation approach: 
                    //   Traverse relevant TagSchema for source tag-data. Copy structures in bulk (not per field).
                    //   Implement custom fixups for certain fields after bulk copy.
                    //   Add custom fixups for relative pointers (like texture, vertex and index data)
                    //   When copying data, compute offset from source-base to source-ptr. Add this offset to the dest-base.
                    virtual bool copyTagData(MapFile* sourceMap, uint32_t sourceTagHandle, void* dest, size_t size);

                    // Copy a bitmap from another map.
                    // Returns an offset. 
                    //   Semantics of offset are implementation dependent.
                    //   RawMapFiles use map-relative offsets.
                    //   RuntimeMapFiles will use Spark buffer offsets.
                    virtual uint32_t copyBitmapData(MapFile* sourceMap, Bitmap* source);

                    // Copy vertex data from another map. Returns offset into the target vertex buffer.
                    //   RawMapFiles use vertex-relative offsets.
                    //   RuntimeMapFiles use Spark buffer offsets.
                    virtual uint32_t copyVertexData(MapFile* sourceMap, ModelGeometryPart* source);

                    // Copy index data from another map. Returns offset into the target index buffer.
                    //   RawMapFiles use index-relative offsets.
                    //   RuntimeMapFiles use Spark buffer offsets.
                    virtual uint32_t copyIndexData(MapFile* sourceMap, ModelGeometryPart* source);
                };

                class RawMapFile : public MapFile;
                class RuntimeMapFile : public MapFile;

                // Get the runtime map file. Cached and only updated when a new scenario loads.
                MapFile* getRuntimeMap();

                namespace Engine::TagSchema {
                    // ...
                    struct Context {
                        MapFile* map;
                        void* structureBase;
                    };

                    struct TagSchema {
                        // ...
                        bool copyData(Context* from, Context* to);
                        // ...
                    };
                    // ...
                }
            ```
