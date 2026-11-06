#pragma once
// Portable replacement for the DirectXMath calls used by unity_scene_capture.cpp
// when IDAS3_PORTABLE_SCENE is defined. The original header lived in an
// `android/` directory that is not part of the public checkout; this one is
// reached through `-IWeb/shim/inc` so the maintained sources stay untouched.
//
// Conventions match DirectXMath exactly: row-major storage, row vectors
// (v * M), view * projection order, depth range 0..1 (D3D clip space).
// The 16 floats are copied verbatim into the scene frame constants.
#include "math_types.h"
#include <cmath>

namespace idas3::portable {
struct Matrix { float m[4][4]; };

// XMMatrixMultiply(a, b): result = a * b with row vectors.
inline Matrix multiply(const Matrix& a, const Matrix& b) {
    Matrix r{};
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j) {
            float s = 0;
            for (int k = 0; k < 4; ++k) s += a.m[i][k] * b.m[k][j];
            r.m[i][j] = s;
        }
    return r;
}

// XMMatrixLookToLH(eye, dir, up). Right-handed callers pass -dir, which is
// precisely how XMMatrixLookAtRH is defined.
inline Matrix lookTo(Vec3 eye, Vec3 dir, Vec3 up) {
    const Vec3 r2 = normalized(dir);
    const Vec3 r0 = normalized(cross(up, r2));
    const Vec3 r1 = cross(r2, r0);
    const Vec3 neg = -eye;
    return Matrix{{
        {r0.x, r1.x, r2.x, 0.f},
        {r0.y, r1.y, r2.y, 0.f},
        {r0.z, r1.z, r2.z, 0.f},
        {dot(r0, neg), dot(r1, neg), dot(r2, neg), 1.f}}};
}

inline Matrix lookAt(Vec3 eye, Vec3 target, Vec3 up, bool leftHanded) {
    return leftHanded ? lookTo(eye, target - eye, up) : lookTo(eye, eye - target, up);
}

// XMMatrixPerspectiveFovLH / RH.
inline Matrix perspective(float fovY, float aspect, float nearZ, float farZ, bool leftHanded) {
    const float h = 1.f / std::tan(fovY * 0.5f);
    const float w = h / aspect;
    if (leftHanded) {
        const float range = farZ / (farZ - nearZ);
        return Matrix{{{w, 0, 0, 0}, {0, h, 0, 0}, {0, 0, range, 1.f}, {0, 0, -range * nearZ, 0}}};
    }
    const float range = farZ / (nearZ - farZ);
    return Matrix{{{w, 0, 0, 0}, {0, h, 0, 0}, {0, 0, range, -1.f}, {0, 0, range * nearZ, 0}}};
}
}  // namespace idas3::portable
