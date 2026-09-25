#include "android-jni.hpp"

#if RNDR_ANDROID

#include <android/native_activity.h>

#include "rndr/log.hpp"

namespace
{
/** Room for the local references one call makes; the VM grows the frame past it when needed. */
constexpr jint k_local_frame_capacity = 16;
}  // namespace

Rndr::JniScope::JniScope(ANativeActivity* activity) : m_activity(activity)
{
    JavaVM* vm = activity->vm;
    JNIEnv* env = nullptr;
    const jint status = vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    if (status == JNI_EDETACHED)
    {
        if (vm->AttachCurrentThread(&env, nullptr) != JNI_OK)
        {
            RNDR_LOG_ERROR("Could not attach the thread to the Java VM");
            return;
        }
        m_attached_here = true;
    }
    else if (status != JNI_OK)
    {
        RNDR_LOG_ERROR("The Java VM refused the thread an environment, error {}", status);
        return;
    }
    if (env->PushLocalFrame(k_local_frame_capacity) != JNI_OK)
    {
        env->ExceptionClear();
        RNDR_LOG_ERROR("Could not make room for JNI local references");
        if (m_attached_here)
        {
            vm->DetachCurrentThread();
            m_attached_here = false;
        }
        return;
    }
    m_env = env;
}

Rndr::JniScope::~JniScope()
{
    if (m_env == nullptr)
    {
        return;
    }
    m_env->PopLocalFrame(nullptr);
    if (m_attached_here)
    {
        m_activity->vm->DetachCurrentThread();
    }
}

jobject Rndr::JniScope::GetActivity() const
{
    return m_activity->clazz;
}

bool Rndr::JniScope::Threw(const char* what) const
{
    if (m_env->ExceptionCheck() == JNI_FALSE)
    {
        return false;
    }
    m_env->ExceptionClear();
    RNDR_LOG_ERROR("Java threw while {}", what);
    return true;
}

#endif  // RNDR_ANDROID
