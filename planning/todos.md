x Investigate offset discrepency on VertexDataPointer. Is it an actual struct size difference or just two different fields?

x Implement new map file API.

x Implement RuntimeMapFile

x Update tag browser to use new map file API.

x Implement RawMapFile

x Implement Schema driven tag data traversal.

-> Update tag dissector to use new map file API.
    x Also clean up that fugly garbage.
    - Expandable substructures (block-pointer and inline).
    - Create structure.
    - Rename structure.
    - Cleanup unused nodes.

-? Stand alone tag browser app for map files.
-? Stand alone tag dissector app for map files.

- Tag allocation in MapFile API.
    - Implement (idempotent) tag path array relocation to make room for new tags.
        - Store location of new tag path array after relocation in unused space in the runtime cache header.

- Tag copying (first pass).
    - No support for texture or model data.
    - No tag reference resolution yet.
    ! Copied tag won't be usable yet, but we can verify it looks right in the tag browser/dissector.

- Texture copying.
    - Copy texture data into dedicated arena for Spark textures.
    - Implement texture cache load detour for Spark tags.
    -? API to set texture data from raw buffer.
        ! Would allow use of Mario texture from ROM.

- Model copying.
    - Copy model data into dedicated arenas for vertices and indices.
    - Create DX11 buffer on load.
    - Implement buffer binding detour for Spark tags.
    -? API to set model data from raw buffer.
        ! Would allow use of Mario model from ROM. (with some modifications to LibSM64)
