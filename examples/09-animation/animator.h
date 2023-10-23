#pragma once

#include "rndr/rndr.h"

#include <assimp/matrix4x4.h>

// Stores info about the bone stored in the mesh.
struct BoneInfo
{
    int32_t id; // Id of the bone.
    // This transform is used to move vertex from object space of the mesh to the local space of the
    // bone while the skeleton is in the bind pose.
    Rndr::Matrix4x4f inverse_bind_pose_transform;
};

using BoneInfoMap = Rndr::HashMap<Rndr::String, BoneInfo>;

// Stores info about the bone position at a specific time (keyframe).
struct KeyPosition
{
    Rndr::Point3f position;
    double time_stamp; // Time stamp in seconds.
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

// Stores changes to the bone during the animation.
struct Bone
{
    Rndr::Array<KeyPosition> key_positions;
    Rndr::Array<KeyRotation> key_rotations;
    Rndr::Array<KeyScale> key_scales;

    Rndr::String name;
    int32_t id;

    Rndr::Matrix4x4f local_transform; // Transform relative to a parent. Only access after UpdateBoneTransform.
};

struct BoneNode
{
    Rndr::String name;
    Rndr::Matrix4x4f transform; // TODO: See if we need this
    Rndr::Array<BoneNode> children;
};

struct Animation
{
    Rndr::String name;
    Rndr::Ref<BoneInfoMap> bone_name_to_bone_info;
    Rndr::Array<Bone> bones;
    BoneNode root_bone_node;
    double duration_in_seconds = 0.0;
    double duration_in_ticks = 0.0;
    double ticks_per_second = 0.0;
};

struct Animator
{
    Rndr::Ref<Animation> animation; // Animation to play.
    double current_time_seconds = 0.0; // How many seconds have passed since the start of the animation.
};

Rndr::Matrix4x4f AssimpMatrixToMatrix4x4(const aiMatrix4x4& ai_matrix);

// Loads an animation at the specific index from the Assimp scene object.
bool LoadAnimationFromAssimp(Animation& out_animation, const struct aiScene* ai_scene, int32_t ai_animation_index,
                             BoneInfoMap& bone_info_map);

// Resets the animator to the start of the animation.
bool StartAnimation(Animator& out_animator, Animation& animation);

// Updates the animator and returns the final transforms for each bone.
void UpdateAnimator(std::span<Rndr::Matrix4x4f>& out_final_transforms, Animator& in_out_animator, double delta_time);