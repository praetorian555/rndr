#include "animator.h"

#include <assimp/anim.h>
#include <assimp/matrix4x4.h>
#include <assimp/scene.h>

Rndr::Matrix4x4f AssimpMatrixToMatrix4x4(const aiMatrix4x4& ai_matrix)
{
    return {ai_matrix.a1, ai_matrix.a2, ai_matrix.a3, ai_matrix.a4, ai_matrix.b1, ai_matrix.b2, ai_matrix.b3, ai_matrix.b4,
            ai_matrix.c1, ai_matrix.c2, ai_matrix.c3, ai_matrix.c4, ai_matrix.d1, ai_matrix.d2, ai_matrix.d3, ai_matrix.d4};
}

namespace
{

bool LoadBoneFromAssimp(Bone& out_bone, const Rndr::String& in_bone_name, int32_t in_id, const aiNodeAnim* in_node)
{
    if (in_node == nullptr)
    {
        return false;
    }

    out_bone.name = in_bone_name;
    out_bone.id = in_id;
    out_bone.local_transform = Math::Identity<float>();

    out_bone.key_positions.resize(in_node->mNumPositionKeys);
    for (size_t i = 0; i < in_node->mNumPositionKeys; ++i)
    {
        const aiVectorKey& key = in_node->mPositionKeys[i];
        out_bone.key_positions[i].position = Rndr::Point3f(key.mValue.x, key.mValue.y, key.mValue.z);
        out_bone.key_positions[i].time_stamp = key.mTime;
    }

    out_bone.key_rotations.resize(in_node->mNumRotationKeys);
    for (size_t i = 0; i < in_node->mNumRotationKeys; ++i)
    {
        const aiQuatKey& key = in_node->mRotationKeys[i];
        out_bone.key_rotations[i].rotation = Rndr::Quaternionf(key.mValue.x, key.mValue.y, key.mValue.z, key.mValue.w);
        out_bone.key_rotations[i].time_stamp = key.mTime;
    }

    out_bone.key_scales.resize(in_node->mNumScalingKeys);
    for (size_t i = 0; i < in_node->mNumScalingKeys; ++i)
    {
        const aiVectorKey& key = in_node->mScalingKeys[i];
        out_bone.key_scales[i].scale = Rndr::Vector3f(key.mValue.x, key.mValue.y, key.mValue.z);
        out_bone.key_scales[i].time_stamp = key.mTime;
    }

    return true;
}

template <typename Container>
int32_t GetIndexAtTime(double in_animation_time, const Container& in_keys)
{
    for (size_t i = 0; i < in_keys.size() - 1; ++i)
    {
        if (in_animation_time < in_keys[i + 1].time_stamp)
        {
            return static_cast<int32_t>(i);
        }
    }
    return 0;
}

float GetScaleFactor(double in_animation_time, double in_last_timestamp, double in_next_timestamp);
Rndr::Point3f InterpolatePositions(double in_animation_time, const Rndr::Array<KeyPosition>& key_positions);
Rndr::Quaternionf InterpolateRotations(double in_animation_time, const Rndr::Array<KeyRotation>& key_rotations);
Rndr::Vector3f InterpolateScales(double in_animation_time, const Rndr::Array<KeyScale>& key_scales);

void UpdateBoneTransform(Bone& in_out_bone, double in_animation_time)
{
    const Rndr::Point3f interpolated_position = InterpolatePositions(in_animation_time, in_out_bone.key_positions);
    const Rndr::Quaternionf interpolated_rotation = InterpolateRotations(in_animation_time, in_out_bone.key_rotations);
    const Rndr::Vector3f interpolated_scale = InterpolateScales(in_animation_time, in_out_bone.key_scales);

    const Rndr::Matrix4x4f translation_matrix = Math::Translate(interpolated_position);
    const Rndr::Matrix4x4f rotation_matrix = Math::Rotate(interpolated_rotation);
    const Rndr::Matrix4x4f scale_matrix = Math::Scale(interpolated_scale.x, interpolated_scale.y, interpolated_scale.z);

    in_out_bone.local_transform = translation_matrix * rotation_matrix * scale_matrix;
}

float GetScaleFactor(double in_animation_time, double in_last_timestamp, double in_next_timestamp)
{
    const double delta_time = in_next_timestamp - in_last_timestamp;
    const float factor = static_cast<float>(in_animation_time - in_last_timestamp) / static_cast<float>(delta_time);
    return factor;
}

Rndr::Point3f InterpolatePositions(double in_animation_time, const Rndr::Array<KeyPosition>& key_positions)
{
    if (key_positions.size() == 1)
    {
        return key_positions[0].position;
    }
    const int32_t position_index = GetIndexAtTime(in_animation_time, key_positions);
    const int32_t next_position_index = (position_index + 1);
    const float scale_factor =
        GetScaleFactor(in_animation_time, key_positions[position_index].time_stamp, key_positions[next_position_index].time_stamp);
    const Rndr::Point3f start = key_positions[position_index].position;
    const Rndr::Point3f end = key_positions[next_position_index].position;
    return Math::Lerp(scale_factor, start, end);
}

Rndr::Quaternionf InterpolateRotations(double in_animation_time, const Rndr::Array<KeyRotation>& key_rotations)
{
    if (key_rotations.size() == 1)
    {
        return key_rotations[0].rotation;
    }
    const int32_t rotation_index = GetIndexAtTime(in_animation_time, key_rotations);
    const int32_t next_rotation_index = (rotation_index + 1);
    const float scale_factor =
        GetScaleFactor(in_animation_time, key_rotations[rotation_index].time_stamp, key_rotations[next_rotation_index].time_stamp);
    const Rndr::Quaternionf start = key_rotations[rotation_index].rotation;
    const Rndr::Quaternionf end = key_rotations[next_rotation_index].rotation;
    return Math::Slerp(scale_factor, start, end);
}

Rndr::Vector3f InterpolateScales(double in_animation_time, const Rndr::Array<KeyScale>& key_scales)
{
    if (key_scales.size() == 1)
    {
        return key_scales[0].scale;
    }
    const int32_t scale_index = GetIndexAtTime(in_animation_time, key_scales);
    const int32_t next_scale_index = (scale_index + 1);
    const float scale_factor =
        GetScaleFactor(in_animation_time, key_scales[scale_index].time_stamp, key_scales[next_scale_index].time_stamp);
    const Rndr::Vector3f start = key_scales[scale_index].scale;
    const Rndr::Vector3f end = key_scales[next_scale_index].scale;
    return Math::Lerp(scale_factor, start, end);
}

void LoadBoneHierarchy(BoneNode& out_bone_node, const aiNode* in_node);

}; // namespace

bool LoadAnimationFromAssimp(Animation& out_animation, const aiScene* ai_scene, int32_t ai_animation_index, BoneInfoMap& bone_info_map)
{
    if (ai_scene == nullptr)
    {
        return false;
    }
    if (ai_scene->mNumAnimations <= static_cast<uint32_t>(ai_animation_index))
    {
        return false;
    }

    const aiAnimation* ai_animation = ai_scene->mAnimations[ai_animation_index];

    out_animation.name = ai_animation->mName.data;
    out_animation.duration_in_ticks = ai_animation->mDuration;
    out_animation.ticks_per_second = ai_animation->mTicksPerSecond;
    out_animation.duration_in_seconds = ai_animation->mDuration / ai_animation->mTicksPerSecond;
    out_animation.bone_name_to_bone_info = bone_info_map;

    // Load all bones. Make sure that if some bones are missing from the bone info map that we got from the
    // mesh that we add them to the map.
    for (uint32_t i = 0; i < ai_animation->mNumChannels; ++i)
    {
        const aiNodeAnim* node = ai_animation->mChannels[i];
        const Rndr::String bone_name = node->mNodeName.data;
        int32_t bone_id = -1;
        if (const auto it = bone_info_map.find(bone_name); it == bone_info_map.end())
        {
            const BoneInfo bone_info = {static_cast<int32_t>(bone_info_map.size()), Math::Identity<float>()};
            out_animation.bone_name_to_bone_info->insert({bone_name, bone_info});
            bone_id = bone_info.id;
        }
        else
        {
            bone_id = it->second.id;
        }
        Bone bone;
        const bool is_loaded = LoadBoneFromAssimp(bone, bone_name, bone_id, node);
        assert(is_loaded);
        RNDR_UNUSED(is_loaded);
        out_animation.bones.push_back(std::move(bone));
    }

    LoadBoneHierarchy(out_animation.root_bone_node, ai_scene->mRootNode);

    return true;
}

namespace
{

void LoadBoneHierarchy(BoneNode& out_bone_node, const aiNode* in_node)
{
    out_bone_node.name = in_node->mName.data;
    out_bone_node.transform = AssimpMatrixToMatrix4x4(in_node->mTransformation);
    if (in_node->mNumChildren == 0)
    {
        return;
    }
    out_bone_node.children.resize(in_node->mNumChildren);
    for (uint32_t i = 0; i < in_node->mNumChildren; ++i)
    {
        LoadBoneHierarchy(out_bone_node.children[i], in_node->mChildren[i]);
    }
}

}  // namespace

bool StartAnimation(Animator& out_animator, Animation& animation)
{
    out_animator.animation = &animation;
    out_animator.current_time_seconds = 0.0;
    return true;
}

namespace
{

void CalculateBoneTransforms(std::span<Rndr::Matrix4x4f>& out_final_transforms, BoneNode& in_bone_node, Animator& in_animator,
                             const Rndr::Matrix4x4f& in_parent_transform);
Bone* GetBone(Animation& in_animation, const Rndr::String& in_bone_name);

}  // namespace

void UpdateAnimator(std::span<Rndr::Matrix4x4f>& out_final_transforms, Animator& in_animator, double in_delta_time)
{
    if (!in_animator.animation.IsValid() || in_delta_time <= 0.0)
    {
        return;
    }
    RNDR_ASSERT(out_final_transforms.size() >= in_animator.animation->bones.size());

    in_animator.current_time_seconds += in_delta_time;
    in_animator.current_time_seconds = Math::Mod(in_animator.current_time_seconds, in_animator.animation->duration_in_seconds);
    CalculateBoneTransforms(out_final_transforms, in_animator.animation->root_bone_node, in_animator, Math::Identity<float>());
}

namespace
{

void CalculateBoneTransforms(std::span<Rndr::Matrix4x4f>& out_final_transforms, BoneNode& in_bone_node, Animator& in_animator,
                             const Rndr::Matrix4x4f& in_parent_transform)
{
    Bone* bone = GetBone(in_animator.animation, in_bone_node.name);
    Rndr::Matrix4x4f local_transform = in_bone_node.transform;
    if (bone != nullptr)
    {
        UpdateBoneTransform(*bone, in_animator.current_time_seconds);
        local_transform = bone->local_transform;
    }
    const Rndr::Matrix4x4f global_transform = in_parent_transform * local_transform;
    auto it = in_animator.animation->bone_name_to_bone_info->find(in_bone_node.name);
    if (it != in_animator.animation->bone_name_to_bone_info->end())
    {
        const BoneInfo& bone_info = it->second;
        out_final_transforms[bone_info.id] = global_transform * bone_info.inverse_bind_pose_transform;
    }
    for (BoneNode& child_bone_node : in_bone_node.children)
    {
        CalculateBoneTransforms(out_final_transforms, child_bone_node, in_animator, global_transform);
    }
}

Bone* GetBone(Animation& in_animation, const Rndr::String& in_bone_name)
{
    const auto it = in_animation.bone_name_to_bone_info->find(in_bone_name);
    return it != in_animation.bone_name_to_bone_info->end() ? &in_animation.bones[it->second.id] : nullptr;
}

}  // namespace