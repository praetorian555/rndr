#pragma once

#include "rndr/definitions.hpp"

#if RNDR_ANDROID

#include <jni.h>

#include "rndr/types.hpp"

struct ANativeActivity;

namespace Rndr
{

/**
 * JNI for the length of one call into Java, for what Android offers only there: the orientation, the clipboard.
 *
 * android_main runs on a thread of the glue's own, which the VM does not know until it is attached, so the scope
 * attaches it and detaches it again on the way out; a thread that someone else attached stays attached. Every local
 * reference made inside the scope goes into a frame that is popped with it, so nothing has to be deleted by hand.
 *
 * A debuggable app runs under CheckJNI, which aborts on a pending exception left behind and on anything but modified
 * UTF-8 given to NewStringUTF. Check each call that can throw with Threw, and move text as UTF-16.
 */
class JniScope
{
public:
    explicit JniScope(ANativeActivity* activity);
    ~JniScope();
    JniScope(const JniScope&) = delete;
    JniScope& operator=(const JniScope&) = delete;
    JniScope(JniScope&&) = delete;
    JniScope& operator=(JniScope&&) = delete;

    /** False when the thread could not be attached or the frame not pushed; the log says which. */
    [[nodiscard]] bool IsValid() const { return m_env != nullptr; }
    [[nodiscard]] JNIEnv* operator->() const { return m_env; }
    /** The android.app.NativeActivity, which is also the app's Context. */
    [[nodiscard]] jobject GetActivity() const;

    /**
     * Whether the last call threw. The exception is cleared, since CheckJNI aborts on the next call otherwise, and
     * logged with what was being done.
     */
    [[nodiscard]] bool Threw(const char* what) const;

private:
    ANativeActivity* m_activity = nullptr;
    JNIEnv* m_env = nullptr;
    bool m_attached_here = false;
};

}  // namespace Rndr

#endif  // RNDR_ANDROID
