Tag data for bsps (sbsp or structure_bsp) is handled differently than any other tag data. It does not live in the tag data block and the tag's data pointer is null.

Instead, `scenario.structure_bsp_references` contains references to `cache_file_structure_bsp_header's` which contain references to `structure_bsp`.

See CEA Decomp https://github.com/surreptitiousresearch/halocea
    - src\blam\scenario\scenario_switch_structure_bsp.c
    - src\blam\cache\scenario_structure_bsp_load.c
    - src\headers\scenario_structure_bsp_reference.h
    - src\headers\cache_file_structure_bsp_header.h
