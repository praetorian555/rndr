#pragma once

#include "rndr/rndr.h"

struct KeyPosition
{
    Rndr::Point3f position;
    double time_stamp;
};

struct KeyRotation
{
    Rndr::Quaternionf rotation;
    double time_stamp;
};

struct KeyScale
{
    Rndr::Vector3f scale;
    double time_stamp;
};

struct Bone
{
    Rndr::Array<KeyPosition> key_positions;
    Rndr::Array<KeyRotation> key_rotations;
    Rndr::Array<KeyScale> key_scales;

    Rndr::String name;
    int32_t id;

    Rndr::Matrix4x4f local_transform;
};

bool LoadBoneFromAssimp(Bone& out_bone, const struct aiNodeAnim* node);
void UpdateBoneTransform(Bone& in_out_bone, double in_animation_time);