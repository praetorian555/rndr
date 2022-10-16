#include <catch2/catch_test_macros.hpp>

#include "rndr/render/graphicscontext.h"
#include "rndr/core/log.h"

TEST_CASE("RenderAPI", "GraphicsContext")
{
    rndr::StdAsyncLogger::Get()->Init();

    rndr::GraphicsContextProperties Props;
    Props.bDisableGPUTimeout = false;
    Props.bEnableDebugLayer = true;
    Props.bFailWarning = true;
    Props.bEnableShaderDebugging = false;
    Props.bMakeThreadSafe = true;

    SECTION("Default")
    {
        rndr::GraphicsContext GC;
        bool Result = GC.Init(Props);
        REQUIRE(Result == true);
    }
    SECTION("Disable GPU Timeout")
    {
        rndr::GraphicsContext GC;
        Props.bDisableGPUTimeout = true;
        bool Result = GC.Init(Props);
        REQUIRE(Result == true);
    }
    SECTION("No Debug Layer")
    {
        rndr::GraphicsContext GC;
        Props.bEnableDebugLayer = false;
        bool Result = GC.Init(Props);
        REQUIRE(Result == true);
    }
    SECTION("Not Thread Safe")
    {
        rndr::GraphicsContext GC;
        Props.bMakeThreadSafe = false;
        bool Result = GC.Init(Props);
        REQUIRE(Result == true);
    }

    rndr::StdAsyncLogger::Get()->ShutDown();
}