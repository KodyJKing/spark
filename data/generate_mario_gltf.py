#!/usr/bin/env python3
"""
Generate a glTF 2.0 file from mario.csv and mario_skeleton.csv.

mario.csv format (one row per vertex, triangles):
    x,y,z, r,g,b, nx,ny,nz, u,v

mario_skeleton.csv format (one row per bone, SM64_MARIO_BONE_COUNT=20 bones):
    pos.x,pos.y,pos.z, xN.x,xN.y,xN.z, yN.x,yN.y,yN.z
    (xN = normalized x-axis of bone's local frame, yN = normalized y-axis; zN = xN x yN)
"""

import base64
import json
import math
import os
import struct

DATA_DIR  = os.path.dirname(os.path.abspath(__file__))
MESH_CSV  = os.path.join(DATA_DIR, "mario.csv")
SKEL_CSV  = os.path.join(DATA_DIR, "mario_skeleton.csv")
OUT_GLTF  = os.path.join(DATA_DIR, "mario.gltf")

# glTF component type constants
FLOAT = 5126


# ---------------------------------------------------------------------------
# Math helpers
# ---------------------------------------------------------------------------

def cross(a, b):
    return [
        a[1] * b[2] - a[2] * b[1],
        a[2] * b[0] - a[0] * b[2],
        a[0] * b[1] - a[1] * b[0],
    ]


def normalize(v):
    length = math.sqrt(sum(x * x for x in v))
    if length < 1e-8:
        return [0.0, 0.0, 1.0]
    return [x / length for x in v]


# ---------------------------------------------------------------------------
# CSV parsing
# ---------------------------------------------------------------------------

def parse_line(line):
    return [float(v.strip()) for v in line.strip().split(",") if v.strip()]


def read_mesh_csv(path):
    """Returns flat lists: positions, normals, colors, uvs (all float32)."""
    positions, normals, colors, uvs = [], [], [], []
    with open(path) as f:
        for line_num, raw in enumerate(f, 1):
            raw = raw.strip()
            if not raw:
                continue
            vals = parse_line(raw)
            if len(vals) < 11:
                print(f"  [mesh] line {line_num}: expected 11 values, got {len(vals)} — skipping")
                continue
            positions += vals[0:3]
            colors    += vals[3:6]
            normals   += vals[6:9]
            uvs       += vals[9:11]
    return positions, normals, colors, uvs


def read_skeleton_csv(path):
    """Returns list of (pos, xN, yN, zN) tuples (each a [x,y,z] list)."""
    bones = []
    with open(path) as f:
        for line_num, raw in enumerate(f, 1):
            raw = raw.strip()
            if not raw:
                continue
            vals = parse_line(raw)
            if len(vals) < 9:
                print(f"  [skel] line {line_num}: expected 9 values, got {len(vals)} — skipping")
                continue
            pos = vals[0:3]
            xN  = normalize(vals[3:6])
            yN  = normalize(vals[6:9])
            zN  = normalize(cross(xN, yN))
            bones.append((pos, xN, yN, zN))
    return bones


# ---------------------------------------------------------------------------
# glTF helpers
# ---------------------------------------------------------------------------

def pack_f32(data):
    return struct.pack(f"<{len(data)}f", *data)


def bone_matrix_col_major(pos, xN, yN, zN):
    """Column-major 4x4 matrix for a glTF node 'matrix' property."""
    return [
        xN[0],  xN[1],  xN[2],  0.0,   # column 0  (x-axis)
        yN[0],  yN[1],  yN[2],  0.0,   # column 1  (y-axis)
        zN[0],  zN[1],  zN[2],  0.0,   # column 2  (z-axis)
        pos[0], pos[1], pos[2], 1.0,   # column 3  (translation)
    ]


def inv_bind_matrix_col_major(pos, xN, yN, zN):
    """Inverse of the bone world matrix (R^T, -R^T * t), column-major."""
    tx = -(xN[0]*pos[0] + xN[1]*pos[1] + xN[2]*pos[2])
    ty = -(yN[0]*pos[0] + yN[1]*pos[1] + yN[2]*pos[2])
    tz = -(zN[0]*pos[0] + zN[1]*pos[1] + zN[2]*pos[2])
    return [
        xN[0], yN[0], zN[0], 0.0,   # column 0
        xN[1], yN[1], zN[1], 0.0,   # column 1
        xN[2], yN[2], zN[2], 0.0,   # column 2
        tx,    ty,    tz,    1.0,   # column 3
    ]


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    print("Reading mesh CSV ...")
    positions, normals, colors, uvs = read_mesh_csv(MESH_CSV)

    print("Reading skeleton CSV ...")
    bones = read_skeleton_csv(SKEL_CSV)

    num_verts = len(positions) // 3
    num_bones = len(bones)
    print(f"  {num_verts} vertices ({num_verts // 3} triangles), {num_bones} bones")

    # ------------------------------------------------------------------
    # Binary buffer: positions | normals | colors | uvs
    # ------------------------------------------------------------------
    pos_bytes    = pack_f32(positions)
    normal_bytes = pack_f32(normals)
    color_bytes  = pack_f32(colors)
    uv_bytes     = pack_f32(uvs)

    off_pos    = 0
    off_normal = off_pos    + len(pos_bytes)
    off_color  = off_normal + len(normal_bytes)
    off_uv     = off_color  + len(color_bytes)

    # Inverse bind matrices (one MAT4 per bone)
    ibm_floats = []
    for pos, xN, yN, zN in bones:
        ibm_floats += inv_bind_matrix_col_major(pos, xN, yN, zN)
    ibm_bytes = pack_f32(ibm_floats)

    off_ibm = len(pos_bytes) + len(normal_bytes) + len(color_bytes) + len(uv_bytes)

    buf_bytes = pos_bytes + normal_bytes + color_bytes + uv_bytes + ibm_bytes
    buf_uri   = "data:application/octet-stream;base64," + base64.b64encode(buf_bytes).decode()

    # ------------------------------------------------------------------
    # POSITION min/max (required by glTF spec)
    # ------------------------------------------------------------------
    xs = positions[0::3]
    ys = positions[1::3]
    zs = positions[2::3]
    min_pos = [min(xs), min(ys), min(zs)]
    max_pos = [max(xs), max(ys), max(zs)]

    # ------------------------------------------------------------------
    # glTF document
    # ------------------------------------------------------------------
    buffer_views = [
        {"buffer": 0, "byteOffset": off_pos,    "byteLength": len(pos_bytes),    "name": "positions"},
        {"buffer": 0, "byteOffset": off_normal,  "byteLength": len(normal_bytes), "name": "normals"},
        {"buffer": 0, "byteOffset": off_color,   "byteLength": len(color_bytes),  "name": "colors"},
        {"buffer": 0, "byteOffset": off_uv,      "byteLength": len(uv_bytes),     "name": "uvs"},
        {"buffer": 0, "byteOffset": off_ibm,     "byteLength": len(ibm_bytes),    "name": "inverseBindMatrices"},
    ]

    accessors = [
        {   # 0 — POSITION
            "bufferView": 0, "byteOffset": 0,
            "componentType": FLOAT, "count": num_verts, "type": "VEC3",
            "min": min_pos, "max": max_pos, "name": "POSITION",
        },
        {   # 1 — NORMAL
            "bufferView": 1, "byteOffset": 0,
            "componentType": FLOAT, "count": num_verts, "type": "VEC3",
            "name": "NORMAL",
        },
        {   # 2 — COLOR_0
            "bufferView": 2, "byteOffset": 0,
            "componentType": FLOAT, "count": num_verts, "type": "VEC3",
            "name": "COLOR_0",
        },
        {   # 3 — TEXCOORD_0
            "bufferView": 3, "byteOffset": 0,
            "componentType": FLOAT, "count": num_verts, "type": "VEC2",
            "name": "TEXCOORD_0",
        },
        {   # 4 — inverseBindMatrices
            "bufferView": 4, "byteOffset": 0,
            "componentType": FLOAT, "count": num_bones, "type": "MAT4",
            "name": "inverseBindMatrices",
        },
    ]

    # node 0 = mesh, node 1 = armature root, nodes 2..2+N = bones
    first_bone_node = 2
    bone_node_indices = list(range(first_bone_node, first_bone_node + num_bones))

    nodes = [
        {"name": "Mario_Mesh", "mesh": 0},
        {"name": "Armature",   "children": bone_node_indices},
    ]
    for i, (pos, xN, yN, zN) in enumerate(bones):
        nodes.append({
            "name":   f"bone_{i}",
            "matrix": bone_matrix_col_major(pos, xN, yN, zN),
        })

    gltf = {
        "asset":  {"version": "2.0", "generator": "generate_mario_gltf.py"},
        "scene":  0,
        "scenes": [{"name": "Scene", "nodes": [0, 1]}],
        "nodes":  nodes,
        "meshes": [{
            "name": "Mario",
            "primitives": [{
                "mode": 4,   # TRIANGLES
                "attributes": {
                    "POSITION":   0,
                    "NORMAL":     1,
                    "COLOR_0":    2,
                    "TEXCOORD_0": 3,
                },
                "skin": 0,
            }],
        }],
        "skins": [{
            "name": "MarioSkin",
            "joints": bone_node_indices,
            "inverseBindMatrices": 4,
        }],
        "accessors":   accessors,
        "bufferViews": buffer_views,
        "buffers": [{"uri": buf_uri, "byteLength": len(buf_bytes)}],
    }

    with open(OUT_GLTF, "w") as f:
        json.dump(gltf, f, indent=2)

    print(f"Written: {OUT_GLTF}")


if __name__ == "__main__":
    main()
