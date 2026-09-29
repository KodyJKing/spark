- Fix the LNK4217 warnings.

x Investigate offset discrepency on VertexDataPointer. Is it an actual struct size difference or just two different fields?

x Implement new map file API.

x Implement RuntimeMapFile

x Update tag browser to use new map file API.

x Implement RawMapFile

x Implement Schema driven tag data traversal.

-> Update tag dissector to use new map file API.
    x Also clean up that fugly garbage.
    x Expandable substructures (block-pointer and inline).
    x Create structure.
    x Rename structure.
    - Cleanup unused nodes.

-? Stand alone tag browser app for map files.
-? Stand alone tag dissector app for map files.

x Tag allocation in MapFile API.
    x Implement (idempotent) tag path array relocation to make room for new tags.
        x Store location of new tag path array after relocation in unused space in the runtime cache header.

x Tag copying (first pass).
    ! No support for texture or model data.
    ! No tag reference resolution yet.
    ! Copied tag won't be usable yet, but we can verify it looks right in the tag browser/dissector.
    x Implement fixups for BlockPointer's after naive copy.
    x Add "Copy Tag" button to tag browser (in external file mode).
    x Copy a tag from any map.
    x Verify blocks look correct in the tag browser/dissector.

- Texture copying.
    - Copy texture data into dedicated arena for Spark textures.
    - Implement texture cache load detour for Spark tags.
    -? API to set texture data from raw buffer.
        ! Would allow use of Mario texture from ROM.

- Make `getTagDataSize` robust.
    ! Needs to work for injected or moved tag data.
    ! Needs to work for last tag in tag array.
        ! Cannot rely on the next tag data position for sizing.

- Tag copying (second pass).
    - Implement tag reference fixups.
        - Recursively copy/patch referenced tags.
            -? Allow dev to decide what to patch based off of allow/deny-list.

- Model copying.
    - Copy model data into dedicated arenas for vertices and indices.
    - Create DX11 buffer on load.
    - Implement buffer binding detour for Spark tags.
    -? API to set model data from raw buffer.
        ! Would allow use of Mario model from ROM. (with some modifications to LibSM64)

- Save/load safety
    - Ensure instances of injected object tags do not cause crashes during load.
        ! Must intercept early enough in loading process to ensure tags are present.
