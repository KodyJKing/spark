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


            struct SparkBuffers {
                // Tags are never removed during an inject-session, so we just append to these buffers and hand out fixed offsets into them.
                // Callers must re-resolve offset->pointer at use time (append may move `data`) and pad appends for alignment.
                std::vector<uint8_t> textureBuffer;
                std::vector<uint8_t> vertexBuffer;
                std::vector<uint8_t> indexBuffer;
            };

            struct SparkPersistedState {
                // Where tag resource paths have been moved to (if they have).
                void* tagResourcePathBlock;
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

                // Copy (or patch) a tag from another map. Return the new tag handle.
                // Handles all copying details and returns a ready to use tag.
                //   - Note: Schema MUST be known for a tag to be copied/patched.
                //   - Copies referenced tags if absent from the target map.
                //   - Patches tags if already present.
                //     - Leave tag data in place if same size.
                //     - Reallocate if size differs.
                //     - Does no comparison, always copies even for unchanged data.
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

                // Adds a new entry to the tag array.
                // Returns handle to new tag.
                // RuntimeMapFile needs to move the block of tag-resource-path strings to make room.
                //   - This move must be idempotent.
                //   - See tags.cpp for current implementation.
                virtual uint32_t allocateTag();
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
