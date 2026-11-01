#include "map_file.hpp"
#include "schema/reference.hpp"

namespace Engine::Map {
    StructureBspReference* getStructureBspReference(MapFile* map, Tag* t);
    void normalizeStructureBspTag(Tag *t, Context *context);
}
