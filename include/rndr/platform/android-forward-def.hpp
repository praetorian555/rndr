#pragma once

// The NDK types the public Android headers name, declared so that none of them has to include an NDK
// header. Global, since that is where the NDK declares them.

struct android_app;
struct ANativeWindow;
struct AInputEvent;
// jni.h's C++ spellings of jclass and jmethodID.
class _jclass;
struct _jmethodID;
