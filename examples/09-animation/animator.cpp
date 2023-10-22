#include "animator.h"

#include <assimp/anim.h>
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

namespace
{
float GetScaleFactor(double in_animation_time, double in_last_timestamp, double in_next_timestamp)
{
    const double delta_time = in_next_timestamp - in_last_timestamp;
    const float factor = static_cast<float>(in_animation_time - in_last_timestamp) / static_cast<float>(delta_time);
    return factor;
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
}  // namespace

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
