#include <iostream>

#include "TestRunner.hpp"
#include "engine/math.hpp"

// Both functions were confirmed via Ghidra decompile to reference zero
// globals -- they only touch their own parameters -- so they are safe to
// call standalone with synthetic inputs.
//
//   Vec3* rotateVec(Transform* tr, Vec3* vecIn, Vec3* vecOut)
//   Vec4* transformVec4AsPlane(Transform* tr, Vec4* vecIn, Vec4* vecOut)
//
// Offsets resolved from exact Ghidra VAs (2026-07-19 analysis session).
namespace {
    constexpr uintptr_t kRotateVecOffset = 0xBA3004;
    constexpr uintptr_t kTransformVec4AsPlaneOffset = 0xBA3090;
    constexpr uintptr_t kInvertTransformOffset = 0xBA2214;
    constexpr uintptr_t kTransformPointOffset = 0xBA2EA8;
    constexpr uintptr_t kTransformVecOffset = 0xBA2F5C;

    using RotateVecFn = Vec3*(Engine::Transform*, Vec3*, Vec3*);
    using TransformVec4AsPlaneFn = Vec4*(Engine::Transform*, Vec4*, Vec4*);
    using InvertTransformFn = void(Engine::Transform*, Engine::Transform*);
    using TransformPointFn = Vec3*(Engine::Transform*, Vec3*, Vec3*);
    using TransformVecFn = Vec3*(Engine::Transform*, Vec3*, Vec3*);

    void printVec3(const char* label, const Vec3& v) {
        std::cout << "  " << label << " = (" << v.x << ", " << v.y << ", " << v.z << ")\n";
    }

    void printVec4(const char* label, const Vec4& v) {
        std::cout << "  " << label << " = (" << v.x << ", " << v.y << ", " << v.z << ", " << v.w << ")\n";
    }

    void printTransform(const char* label, const Engine::Transform& t) {
        std::cout << "  " << label << ": w=" << t.w << "\n";
        printVec3("  x", t.x);
        printVec3("  y", t.y);
        printVec3("  z", t.z);
        printVec3("  pos", t.pos);
    }

    bool vec3Equals(const Vec3& a, const Vec3& b) {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }
}

HALO1_TEST(RotateVec_IdentityTransform_ReturnsInputUnchanged) {
    auto rotateVec = halo1.at<RotateVecFn>(kRotateVecOffset);

    Engine::Transform identity = Engine::Transform::identity();

    Vec3 in{ 3.0f, -2.0f, 5.0f };
    Vec3 out{};
    rotateVec(&identity, &in, &out);

    bool ok = out.x == in.x && out.y == in.y && out.z == in.z;
    if (!ok) {
        printVec3("expected", in);
        printVec3("actual  ", out);
    }
    return ok;
}

HALO1_TEST(RotateVec_AxisSwapMatrix_SwapsXAndY) {
    // x/y basis columns swapped -> local X axis maps to world Y and vice versa.
    Engine::Transform swapXY{};
    swapXY.w = 1.0f;
    swapXY.x = { 0, 1, 0 };
    swapXY.y = { 1, 0, 0 };
    swapXY.z = { 0, 0, 1 };
    swapXY.pos = { 0, 0, 0 };

    auto rotateVec = halo1.at<RotateVecFn>(kRotateVecOffset);

    Vec3 in{ 3.0f, -2.0f, 5.0f };
    Vec3 out{};
    rotateVec(&swapXY, &in, &out);

    Vec3 expected{ in.y, in.x, in.z };  // (-2, 3, 5)
    bool ok = out.x == expected.x && out.y == expected.y && out.z == expected.z;
    if (!ok) {
        printVec3("expected", expected);
        printVec3("actual  ", out);
    }
    return ok;
}

HALO1_TEST(TransformVec4AsPlane_AxisSwapPlusTranslationAndScale_MatchesHandDerivedPlane) {
    // Same axis-swap rotation, plus a translation and non-unit scale, applied
    // to a plane (nx, ny, nz, d). Expected values hand-derived from the
    // decompiled formula:
    //   rotatedNormal = rotateVec(tr, inNormal)
    //   outW = dot(pos, rotatedNormal) + inW * w
    Engine::Transform tr{};
    tr.w = 2.0f;
    tr.x = { 0, 1, 0 };
    tr.y = { 1, 0, 0 };
    tr.z = { 0, 0, 1 };
    tr.pos = { 10, 20, 30 };

    auto transformVec4AsPlane = halo1.at<TransformVec4AsPlaneFn>(kTransformVec4AsPlaneOffset);

    Vec4 in{ 3.0f, -2.0f, 5.0f, 7.0f };
    Vec4 out{};
    transformVec4AsPlane(&tr, &in, &out);

    // rotatedNormal = (-2, 3, 5)
    // outW = pos.y*3 + pos.x*(-2) + pos.z*5 + 7*2 = 60 - 20 + 150 + 14 = 204
    Vec4 expected{ -2.0f, 3.0f, 5.0f, 204.0f };
    bool ok = out.x == expected.x && out.y == expected.y && out.z == expected.z && out.w == expected.w;
    if (!ok) {
        printVec4("expected", expected);
        printVec4("actual  ", out);
    }
    return ok;
}

// Unlike rotateVec, transformVec/transformPoint apply the transform's scale
// (w) to the input vector *before* rotating, unless w == DAT_unitScale (1.0),
// in which case the multiply is skipped as an optimization. Confirmed by
// decompile -- NOTE this means the existing Engine::Transform::transformVec()/
// transformPoint() member methods (engine/math.hpp), which do not apply `w`
// at all, do not match the real engine functions when scale != 1.

HALO1_TEST(TransformVec_NonUnitScale_AppliesScaleBeforeRotating) {
    // Same axis-swap matrix as the rotateVec tests, s=2.
    Engine::Transform tr{};
    tr.w = 2.0f;
    tr.x = { 0, 1, 0 };
    tr.y = { 1, 0, 0 };
    tr.z = { 0, 0, 1 };
    tr.pos = { 0, 0, 0 };

    auto transformVec = halo1.at<TransformVecFn>(kTransformVecOffset);

    Vec3 in{ 3.0f, -2.0f, 5.0f };
    Vec3 out{};
    transformVec(&tr, &in, &out);

    // rotateVec(in) would be (-2, 3, 5) (see RotateVec_AxisSwapMatrix test);
    // transformVec scales by w=2 first (commutes with rotation for a scalar).
    Vec3 expected{ -4.0f, 6.0f, 10.0f };
    bool ok = vec3Equals(out, expected);
    if (!ok) {
        printVec3("expected", expected);
        printVec3("actual  ", out);
    }
    return ok;
}

HALO1_TEST(TransformVec_UnitScale_SkipsScaleMultiplyBranch) {
    // w == DAT_unitScale (1.0) exactly -> takes the "skip multiply" branch.
    // Result should be identical to a plain rotateVec.
    Engine::Transform tr{};
    tr.w = 1.0f;
    tr.x = { 0, 1, 0 };
    tr.y = { 1, 0, 0 };
    tr.z = { 0, 0, 1 };
    tr.pos = { 0, 0, 0 };

    auto transformVec = halo1.at<TransformVecFn>(kTransformVecOffset);

    Vec3 in{ 3.0f, -2.0f, 5.0f };
    Vec3 out{};
    transformVec(&tr, &in, &out);

    Vec3 expected{ in.y, in.x, in.z };  // (-2, 3, 5)
    bool ok = vec3Equals(out, expected);
    if (!ok) {
        printVec3("expected", expected);
        printVec3("actual  ", out);
    }
    return ok;
}

HALO1_TEST(TransformPoint_ScaleAndTranslation_MatchesHandDerivedPoint) {
    Engine::Transform tr{};
    tr.w = 2.0f;
    tr.x = { 0, 1, 0 };
    tr.y = { 1, 0, 0 };
    tr.z = { 0, 0, 1 };
    tr.pos = { 10, 20, 30 };

    auto transformPoint = halo1.at<TransformPointFn>(kTransformPointOffset);

    Vec3 in{ 3.0f, -2.0f, 5.0f };
    Vec3 out{};
    transformPoint(&tr, &in, &out);

    // scale-then-rotate = (-4, 6, 10) (see TransformVec_NonUnitScale test), + pos
    Vec3 expected{ 6.0f, 26.0f, 40.0f };
    bool ok = vec3Equals(out, expected);
    if (!ok) {
        printVec3("expected", expected);
        printVec3("actual  ", out);
    }
    return ok;
}

HALO1_TEST(InvertTransform_CyclicPermutation_MatchesHandDerivedInverse) {
    // A proper rotation (cyclic axis permutation x->y->z->x), deliberately
    // asymmetric so the transpose swap is actually exercised (an involution
    // like the axis-swap matrix used above is its own transpose and would
    // not distinguish "transposed" from "copied").
    Engine::Transform tr{};
    tr.w = 2.0f;
    tr.x = { 0, 1, 0 };
    tr.y = { 0, 0, 1 };
    tr.z = { 1, 0, 0 };
    tr.pos = { 10, 20, 30 };

    auto invertTransform = halo1.at<InvertTransformFn>(kInvertTransformOffset);

    Engine::Transform out{};
    invertTransform(&tr, &out);

    // Hand-derived from the decompiled formula:
    //   invS = 1/w = 0.5
    //   invR = transpose(R): x=(0,0,1), y=(1,0,0), z=(0,1,0)
    //   invPos.axis = dot(R.axis, -pos * invS), negScaledPos = (-5,-10,-15)
    //     invPos.x = dot((0,1,0), (-5,-10,-15)) = -10
    //     invPos.y = dot((0,0,1), (-5,-10,-15)) = -15
    //     invPos.z = dot((1,0,0), (-5,-10,-15)) = -5
    bool ok = out.w == 0.5f
        && vec3Equals(out.x, Vec3{ 0, 0, 1 })
        && vec3Equals(out.y, Vec3{ 1, 0, 0 })
        && vec3Equals(out.z, Vec3{ 0, 1, 0 })
        && vec3Equals(out.pos, Vec3{ -10, -15, -5 });
    if (!ok) {
        printTransform("expected", Engine::Transform{ 0.5f, {0,0,1}, {1,0,0}, {0,1,0}, {-10,-15,-5} });
        printTransform("actual  ", out);
    }
    return ok;
}

HALO1_TEST(InvertTransform_RoundTrip_RecoversOriginalTransform) {
    // Inverting twice should exactly recover the original transform.
    // w=2 <-> invS=0.5 is an exact power-of-two round trip, and all
    // component values are small integers, so bit-exact equality is
    // expected (no accumulated float rounding).
    Engine::Transform original{};
    original.w = 2.0f;
    original.x = { 0, 1, 0 };
    original.y = { 0, 0, 1 };
    original.z = { 1, 0, 0 };
    original.pos = { 10, 20, 30 };

    auto invertTransform = halo1.at<InvertTransformFn>(kInvertTransformOffset);

    Engine::Transform inverted{};
    invertTransform(&original, &inverted);

    Engine::Transform roundTripped{};
    invertTransform(&inverted, &roundTripped);

    bool ok = roundTripped.w == original.w
        && vec3Equals(roundTripped.x, original.x)
        && vec3Equals(roundTripped.y, original.y)
        && vec3Equals(roundTripped.z, original.z)
        && vec3Equals(roundTripped.pos, original.pos);
    if (!ok) {
        printTransform("expected", original);
        printTransform("actual  ", roundTripped);
    }
    return ok;
}

