"""
Blender script: set all bones in the active armature to unit length.
Run via: Text Editor > Run Script, or Scripting workspace.
The active object must be the imported armature.
"""

import bpy

obj = bpy.context.active_object
if obj is None or obj.type != 'ARMATURE':
    raise RuntimeError("Select the armature object before running this script.")

bpy.ops.object.mode_set(mode='EDIT')
for bone in obj.data.edit_bones:
    bone.length = 10.0
bpy.ops.object.mode_set(mode='OBJECT')

print(f"Set {len(obj.data.bones)} bones to unit length.")
