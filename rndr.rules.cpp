#include "builder/rules.hpp"

// rndr as CMakeLists.txt and src/CMakeLists.txt build it, with the CMake options as the project
// options in builder.json. RNDR_HARDENING is builder's --sanitize=address, and the Forge practice
// samples, which CMake globs for, are not described yet. Targets are named as CMake's are, with '_'
// for '-', since a target's name is a C++ identifier.

// Keep in step with the latest rndr-* tag, which cmake/version.cmake reads.
static constexpr const char* k_rndr_version = "0.9.3";

static bool RndrHasShaderCompiler(const Builder::Rules::ConfigurationInfo& c)
{
    // Slang ships no Android build, so there shaders are compiled on the host.
    return !c.platform.Is(Builder::Rules::Platform::Android);
}

// Every module starts from cmake/compiler-warnings.cmake and cmake/compiler-options.cmake: strict, as
// errors, with the few checks rndr turns back off. Vendored code opts out with Warnings::Off.
BUILDER_MODULE_DEFAULTS(m, t)
{
    m.warnings = Warnings::Strict;
    m.warnings_as_errors = true;
    if (t.toolchain.Is(Compiler::Msvc))
    {
        m.private_defines.push_back("UNICODE=1");
        m.private_compile_flags.push_back("/wd4201");
        return;
    }
    Append(m.private_compile_flags, {"-Wno-missing-field-initializers", "-Wno-sign-compare", "-Wno-sign-conversion"});
    if (t.toolchain.Is(Compiler::Clang))
    {
        Append(m.private_compile_flags, {"-Wno-gnu-anonymous-struct", "-Wno-nested-anon-types", "-Wno-dtor-name"});
    }
}

BUILDER_MODULE(Rndr, m, t)
{
    const bool windows = t.platform.Is(Platform::Windows);
    const bool canvas = t.GetBool("canvas");
    const bool forge = t.GetBool("forge");
    const bool audio = t.GetBool("audio");
    const bool shader_compiler = RndrHasShaderCompiler(t);
    if (canvas && !windows)
    {
        m.Error("canvas needs Windows: its context is WGL");
    }

    m.cxx_standard = 20;
    m.public_includes = {"include", "builder/include"};
    m.private_includes = {"src"};
    m.headers.AddDir("include", Recurse::Yes);
    m.public_deps = {"opal::Opal", "imgui::Imgui", "StbImage"};

    m.sources.Add("src", {"time.cpp", "input-system.cpp", "projection-camera.cpp", "file.cpp", "bitmap.cpp", "fly-camera.cpp",
                          "frames-per-second-counter.cpp", "trace.cpp", "log.cpp", "projections.cpp", "application.cpp",
                          "platform-application.cpp", "pixel-format.cpp", "canvas/projections.cpp"});
    if ((canvas || forge) && shader_compiler)
    {
        m.sources.Add("src/core/shader-compiler.cpp");
        m.private_deps.push_back("slang::Slang");
    }
    if (canvas || forge)
    {
        m.sources.Add("src/core/shader-cache.cpp");
    }
    if (shader_compiler)
    {
        m.public_defines.push_back("RNDR_SHADER_COMPILER");
    }

    if (canvas)
    {
        m.sources.Add("src/canvas", {"context.cpp", "buffer.cpp", "texture.cpp", "render-target.cpp", "shader.cpp", "vertex-layout.cpp",
                                     "mesh.cpp", "brush.cpp", "draw-list.cpp", "timestamp-query.cpp", "bitmap.cpp", "shape-renderer.cpp",
                                     "bitmap-text-renderer.cpp", "cubemap-renderer.cpp", "pbr-renderer.cpp", "grid-renderer.cpp"});
        m.public_defines.push_back("RNDR_CANVAS");
        m.public_deps.push_back("StbTruetype");
        m.libs.push_back("opengl32.lib");
    }

    if (audio)
    {
        m.sources.Add("src/audio", {"audio-clip.cpp", "audio-decoders.cpp", "audio-mixer.cpp", "audio-device.cpp", "audio-system.cpp"});
        m.public_defines.push_back("RNDR_AUDIO");
        m.public_deps.push_back("StbVorbis");
        if (windows)
        {
            // WASAPI through COM, and the audio thread registers with MMCSS.
            m.sources.Add("src/platform/windows-audio-device.cpp");
            Append(m.libs, {"Ole32.lib", "Avrt.lib"});
        }
        else if (t.platform.Is(Platform::Android))
        {
            m.sources.Add("src/platform/android-audio-device.cpp");
            m.libs.push_back("aaudio");
        }
    }

    if (windows)
    {
        m.sources.Add({"src/platform/windows-application.cpp", "src/platform/windows-gamepad.cpp", "src/platform/windows-window.cpp",
                       "src/imgui-system.cpp"});
        // glad is only reached from Canvas and the Win32 platform layer.
        m.public_deps.push_back("Glad");
        Append(m.libs, {"User32.lib", "Shcore.lib", "Xinput.lib"});
    }
    else if (t.platform.Is(Platform::Linux))
    {
        m.sources.Add("src/platform", {"linux-application.cpp", "linux-window.cpp"});
        m.private_deps.push_back("Xcb");
    }
    else if (t.platform.Is(Platform::Android))
    {
        m.Error("Android needs the NDK's native_app_glue, which these rules do not describe yet");
    }

    if (forge)
    {
        m.sources.Add("src/forge", {"graphics-context.cpp", "device.cpp", "physical-device.cpp", "swap-chain.cpp", "texture.cpp",
                                    "video.cpp", "buffer.cpp", "memory-info.cpp", "synchronization.cpp", "command-buffer.cpp",
                                    "render-pass.cpp", "synchronization2.cpp", "transfer.cpp", "debug.cpp", "query.cpp",
                                    "frame-context.cpp", "descriptor-set.cpp", "shader.cpp", "mesh.cpp", "pipeline.cpp"});
        m.sources.Add("src", {"volk-implementation.cpp", "vma-implementation.cpp"});
        // The Vulkan headers and volk leak through Forge's public headers; VMA and SPIRV-Reflect do not.
        Append(m.public_deps, {"vulkan-headers::VulkanHeaders", "volk::Volk"});
        Append(m.private_deps, {"vma::Vma", "spirv-reflect::SpirvReflect"});
        Append(m.public_defines, {"RNDR_FORGE", "VK_NO_PROTOTYPES"});
        m.public_defines.push_back(windows                            ? "VK_USE_PLATFORM_WIN32_KHR"
                                   : t.platform.Is(Platform::Android) ? "VK_USE_PLATFORM_ANDROID_KHR"
                                                                      : "VK_USE_PLATFORM_XCB_KHR");
        // Public: whether the layer is there decides what a caller sees.
        if (t.GetBool("forge-validation"))
        {
            m.public_defines.push_back("RNDR_FORGE_VALIDATION");
        }
        // Public: it changes the SPIR-V ShaderCompiler emits and what Forge::k_vulkan_api_version says.
        if (t.GetBool("forge-vulkan-1-1"))
        {
            m.public_defines.push_back("RNDR_FORGE_VULKAN_1_1");
        }
    }

    if (t.GetBool("assimp"))
    {
        m.private_deps.push_back("assimp::Assimp");
        m.private_defines.push_back("RNDR_ASSIMP");
    }
    if (t.GetBool("ktx"))
    {
        m.private_deps.push_back("ktx::Ktx");
        m.private_defines.push_back("RNDR_KTX");
    }

    const std::string version = k_rndr_version;
    const std::size_t minor_at = version.find('.') + 1;
    const std::size_t patch_at = version.find('.', minor_at) + 1;
    m.public_defines.push_back("RNDR_VERSION_MAJOR=" + version.substr(0, minor_at - 1));
    m.public_defines.push_back("RNDR_VERSION_MINOR=" + version.substr(minor_at, patch_at - minor_at - 1));
    m.public_defines.push_back("RNDR_VERSION_PATCH=" + version.substr(patch_at));
    m.public_defines.push_back(DefineString("RNDR_VERSION_STRING", version));
    m.public_defines.push_back(DefineString("RNDR_VERSION_FULL", version));
    m.public_defines.push_back("RNDR_STATIC_DEFINE");
    m.public_defines.push_back(DefineString("RNDR_CORE_ASSETS_DIR", BUILDER_HERE("assets")));

    // rndr_test: the suites the build has, with Catch2's macros replaced for a build without exceptions.
    m.test_deps = {"Catch2", "RndrTestSupport"};
    m.test_sources.Add("test", {"base-test.cpp", "bitmap-test.cpp", "image-decode-test.cpp", "camera-test.cpp",
                                "frames-per-second-counter-test.cpp", "input-test.cpp", "window-test.cpp", "catch-no-exceptions.cpp"});
    if (canvas)
    {
        m.test_sources.Add("test/canvas", {"context-test.cpp", "buffer-test.cpp", "texture-test.cpp", "render-target-test.cpp",
                                           "shader-test.cpp", "mesh-test.cpp", "brush-test.cpp", "bitmap-test.cpp",
                                           "timestamp-query-test.cpp"});
    }
    if (forge)
    {
        m.test_sources.Add("test/forge", {"smoke-test.cpp", "window-test.cpp"});
        if (shader_compiler)
        {
            // The case that checks slangc and ShaderCompiler produce the same module; the options are cmake/shaders.cmake's.
            m.DefineToolPath("RNDR_TEST_SLANGC", "slang::Slangc");
            const std::string profile = t.GetBool("forge-vulkan-1-1") ? "spirv_1_4" : "spirv_1_5";
            m.private_defines.push_back(DefineString("RNDR_TEST_SLANGC_OPTIONS", "-target spirv -profile " + profile +
                                                                                     " -emit-spirv-directly -fvk-use-entrypoint-name"
                                                                                     " -matrix-layout-row-major"));
        }
    }
    if (audio)
    {
        m.test_sources.Add("test/audio", {"audio-clip-test.cpp", "audio-mixer-test.cpp", "audio-stream-test.cpp", "audio-system-test.cpp"});
    }
}

BUILDER_MODULE(Glad, m, t)
{
    m.public_system_includes = {"extern/glad/include"};
    m.sources.Add("extern/glad/src/glad.c");
    m.sources.Add("extern/glad/src/glad_wgl.c");
    m.warnings = Warnings::Off;
}

BUILDER_MODULE(StbImage, m, t)
{
    m.public_system_includes = {"extern/stb_image/include"};
    m.sources.Add("extern/stb_image/src/stb_image.cpp");
    m.warnings = Warnings::Off;
}

BUILDER_MODULE(StbVorbis, m, t)
{
    m.public_system_includes = {"extern/stb_vorbis/include"};
    m.sources.Add("extern/stb_vorbis/src/stb_vorbis.cpp");
    m.warnings = Warnings::Off;
}

// Included by the public bitmap-text-renderer.hpp.
BUILDER_MODULE(StbTruetype, m, t)
{
    m.public_includes = {"samples/extern/stb_truetype/include"};
    m.sources.Add("samples/extern/stb_truetype/src/stb_truetype.cpp");
    m.warnings = Warnings::Off;
}

// The X11 windowing stack the Linux platform layer wraps: raw XCB, and libxkbcommon for keyboard
// translation. Their headers are in the system's include directory.
BUILDER_EXTERNAL(Xcb, e, t)
{
    e.libs = {"xcb", "xcb-randr", "xcb-xfixes", "xcb-xkb", "xkbcommon", "xkbcommon-x11"};
}

// Catch2's amalgamation, vendored. It leaves Catch::throw_exception to test/catch-no-exceptions.cpp,
// which a build without exceptions needs.
BUILDER_MODULE(Catch2, m, t)
{
    m.public_system_includes = {"extern/catch2/include", "extern/catch2/include/catch2"};
    m.public_defines = {"CATCH_CONFIG_DISABLE_EXCEPTIONS_CUSTOM_HANDLER"};
    m.sources.Add("extern/catch2/src/catch_amalgamated.cpp");
    m.warnings = Warnings::Off;
}

// The Catch2 macro replacements every suite includes, and what the window tests need on Linux: the
// clipboard cases talk to the X server as a second client, from a thread.
BUILDER_MODULE(RndrTestSupport, m, t)
{
    m.public_includes = {"test"};
    if (t.platform.Is(Platform::Linux))
    {
        m.public_deps = {"Xcb"};
        m.libs = {"pthread"};
    }
}

BUILDER_MODULE(SamplesShared, m, t)
{
    m.public_includes = {"samples/shared"};
    m.sources.Add("samples/shared/example-controller.cpp");
    m.public_deps = {"Rndr", "opal::Opal"};
    m.public_defines = {"RNDR_SAMPLES_SHADERS_DIR=\"" + BUILDER_HERE("samples/shared/shaders") + "\""};
}

// CMake builds the samples without rndr's warnings. Most samples still narrow doubles to floats, in
// opal's math templates and their own literals (C4244, C4305), and video_frame has unreachable code
// (C4702): those warn without failing.
BUILDER_MODULE(WindowSample, m, t)
{
    m.sources.Add("samples/window/window-sample.cpp");
    m.private_deps = {"SamplesShared", "imgui::Imgui"};
    m.warnings_as_errors = false;
}

BUILDER_MODULE(TextRenderingSample, m, t)
{
    m.sources.Add("samples/text-rendering/text-rendering-sample.cpp");
    m.private_deps = {"SamplesShared", "imgui::Imgui"};
    m.warnings_as_errors = false;
}

BUILDER_MODULE(ModernVulkanSample, m, t)
{
    m.sources.Add("samples/modern-vulkan/modern-vulkan.cpp");
    m.private_deps = {"SamplesShared", "imgui::Imgui"};
    m.warnings_as_errors = false;
}

BUILDER_MODULE(VideoFrameSample, m, t)
{
    m.sources.Add("samples/video-frame/video-frame.cpp");
    m.private_deps = {"SamplesShared"};
    m.warnings_as_errors = false;
}

BUILDER_MODULE(AudioSample, m, t)
{
    m.sources.Add("samples/audio/audio-sample.cpp");
    m.private_deps = {"SamplesShared"};
}

BUILDER_TARGET(rndr, t, c)
{
    t.kind = TargetKind::StaticLibrary;
    t.modules = {"Rndr"};
}

BUILDER_TARGET(rndr_test, t, c)
{
    t.kind = TargetKind::Test;
    t.modules = {"Rndr"};
    t.test_framework = TestFramework::Catch2;
    t.enabled = c.GetBool("tests");
}

BUILDER_TARGET(window_sample, t, c)
{
    t.modules = {"WindowSample"};
    t.enabled = c.GetBool("samples") && c.GetBool("canvas");
}

BUILDER_TARGET(text_rendering_sample, t, c)
{
    t.modules = {"TextRenderingSample"};
    t.enabled = c.GetBool("samples") && c.GetBool("canvas");
}

BUILDER_TARGET(modern_vulkan, t, c)
{
    t.modules = {"ModernVulkanSample"};
    t.enabled = c.GetBool("samples") && c.GetBool("forge") && !c.platform.Is(Platform::Android);
}

BUILDER_TARGET(video_frame, t, c)
{
    t.modules = {"VideoFrameSample"};
    t.enabled = c.GetBool("samples") && c.GetBool("forge") && !c.platform.Is(Platform::Android);
}

BUILDER_TARGET(audio_sample, t, c)
{
    t.modules = {"AudioSample"};
    t.enabled = c.GetBool("samples") && c.GetBool("audio");
}
