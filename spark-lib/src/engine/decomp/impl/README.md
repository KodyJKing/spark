# Decomp conventions

### Functions
- One file per public impl function. (private helpers are okay)
- Use the exact same name as used in Ghidra.

### Types
- Keep unidentified structs in the /engine/decomp/impl/types dir. 
- Promote to /engine/types when they are identified.
- AI should not promote a type to /engine/types without permission, but do flag for review.
