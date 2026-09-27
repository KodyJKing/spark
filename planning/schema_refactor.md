# Goals

- Simpler more uniform CRUD operations for various node types within schema.
- Decouple node's friendly names from id.
- Use Map File API.
- Cleaner traversal of structure (values).

```C++

///////////////////
// Schema

using Id = UUID;

enum class Type {
    U8,
    U16,
    U32,
    U64,
    I8,
    I16,
    I32,
    I64,
    F32,
    F64,
    Vec3,
    Vec4,
    Matrix3x3,
    TagString,
    Structure,
    BlockPointer,
    TagReference,
    // ...
};

struct TypeRef {
    Type type;
    Id ref;
};

struct SchemaNode {
    char name[256];
};

struct StructureNode : public SchemaNode {
    std::vector<Id> fields;
    size_t size;
};

struct FieldNode : public SchemaNode {
    TypeRef type;
    size_t offset;
};

struct TagSchemaNode : public SchemaNode {
};

struct Schema {
    std::ordered_map<Id, TagSchemaNode> tagSchemas;
    std::ordered_map<Id, StructureNode> structures;
    std::ordered_map<Id, FieldNode> fields;
};

///////////////////
// References

struct Context {
    MapFile* mapFile;
    Schema* schema;
};

// A reference to a field within a structure
struct FieldRef {
    Id field;
    void* address;
};

// Reference to a structure. Could be top level of a tag or a nested block.
struct StructuredRef {
    Id structure;
    void* address;
};

///////////////////
// Usage: Copying

bool copyStructure(StructuredRef& source, StructuredRef& destination) {
    // For field in source, copy to destination
    for (const auto& sourceField : source.getFields()) {
        const auto& destinationField = destination.getField(sourceField.getName());
        if (sourceField.isStructure()) {
            copyStructure(sourceField.getStructure(), destinationField.getStructure());
        } else if (sourceField.isBlockPointer()) {
            for (size_t i = 0; i < sourceField.getBlockSize(); ++i) {
                copyStructure(sourceField.getBlockElement(i), destinationField.getBlockElement(i));
            }
        } else if (sourceField.isTagReference()) {
            uint32_t newTagHandle = copyOrPatchTag(source.getTagHandle(), destination.context);
            destinationField.setTagHandle(newTagHandle);
        } else {
            applyRelocationFixups(sourceField, destinationField);
        }
    }
}

///////////////////
// Usage: UI

void renderDeleteButton(FieldRef& fieldRef) {
    if (ImGui::Button("Delete")) {
        fieldRef.schema().deleteField(fieldRef.field);
    }
}

bool renderStructuredRef(StructuredRef structuredRef) {
    // Render the structured reference in the UI
    auto& fields = structuredRef.getFields();
    for (auto id = fields.begin(); id != fields.end(); ++id) {
        if (field.isStructure()) {
            renderStructuredRef(field.getStructure());
        } else if (field.isBlockPointer()) {
            for (size_t i = 0; i < field.getBlockSize(); ++i) {
                renderStructuredRef(field.getBlockElement(i));
            }
        } else {
            renderFieldValue(field.getValue());
        }

        ImGui::SameLine();
        renderDeleteButton(field);
    }
    return true;
}

void renderUnclassified(FieldRef field) {
    // Assert field has null id.
    assert(field.field == Id::Null);

    // Render the unclassified byte in the UI
    renderByte(*field.address);
    if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
        StructureRef structureRef = field.parent();
        structureRef->createFieldAt(field.address);
    }
}

```