#include <catch2/catch2.hpp>

#include "glad/glad.h"

#include "opal/container/scope-ptr.h"

#include "rndr/application.hpp"
#include "rndr/canvas/context.hpp"
#include "rndr/canvas/draw-list.hpp"
#include "rndr/canvas/timestamp-query.hpp"
#include "rndr/exception.hpp"
#include "rndr/generic-window.hpp"

namespace
{

Rndr::Canvas::Context CreateTestContext(Opal::ScopePtr<Rndr::Application>& app, Opal::Ref<Rndr::GenericWindow>& window)
{
    app = Rndr::Application::Create();
    Rndr::GenericWindowDesc window_desc;
    window_desc.start_visible = false;
    window = app->CreateGenericWindow(window_desc);
    return Rndr::Canvas::Context::CreateContext(window.Clone());
}

struct TimestampQueryTestFixture
{
    Opal::ScopePtr<Rndr::Application> app;
    Opal::Ref<Rndr::GenericWindow> window;
    Rndr::Canvas::Context context;

    TimestampQueryTestFixture() : context(CreateTestContext(app, window)) {}
};

}  // namespace

TEST_CASE("Canvas TimestampQuery", "[canvas][timestamp-query]")
{
    TimestampQueryTestFixture f;

    SECTION("Default constructed query is invalid")
    {
        Rndr::Canvas::TimestampQuery query;
        REQUIRE_FALSE(query.IsValid());
        REQUIRE_FALSE(query.IsRecorded());
        REQUIRE_FALSE(query.IsResultAvailable());
        REQUIRE_THROWS_AS(query.Record(), Rndr::GraphicsAPIException);
        REQUIRE_THROWS_AS(query.GetResult(), Rndr::GraphicsAPIException);
    }

    SECTION("Create query")
    {
        Rndr::Canvas::TimestampQuery query("TestQuery");
        REQUIRE(query.IsValid());
        REQUIRE(query.GetNativeHandle() != 0);
        REQUIRE(query.GetName() == "TestQuery");
        REQUIRE_FALSE(query.IsRecorded());
    }

    SECTION("Reading a query that was never recorded throws")
    {
        Rndr::Canvas::TimestampQuery query("TestQuery");
        REQUIRE_THROWS_AS(query.GetResult(), Rndr::GraphicsAPIException);

        Rndr::u64 result = 0;
        REQUIRE_FALSE(query.TryGetResult(result));
        REQUIRE(result == 0);
    }

    SECTION("Record and read a timestamp")
    {
        Rndr::Canvas::TimestampQuery query("TestQuery");
        query.Record();
        REQUIRE(query.IsRecorded());

        glFinish();
        REQUIRE(query.IsResultAvailable());
        REQUIRE(query.GetResult() != 0);

        Rndr::u64 result = 0;
        REQUIRE(query.TryGetResult(result));
        REQUIRE(result != 0);
    }

    SECTION("Elapsed time between two timestamps")
    {
        Rndr::Canvas::TimestampQuery start("Start");
        Rndr::Canvas::TimestampQuery end("End");

        start.Record();
        glFinish();
        end.Record();
        glFinish();

        REQUIRE(end.GetResult() >= start.GetResult());
        REQUIRE(Rndr::Canvas::GetElapsedNanoseconds(start, end) == end.GetResult() - start.GetResult());
        REQUIRE(Rndr::Canvas::GetElapsedMilliseconds(end, start) == 0.0);
    }

    SECTION("Record through a draw list")
    {
        Rndr::Canvas::TimestampQuery start("Start");
        Rndr::Canvas::TimestampQuery end("End");

        Rndr::Canvas::DrawList list;
        list.WriteTimestamp(start);
        list.SetRenderTarget(f.context);
        list.Clear({0, 0, 0, 1});
        list.WriteTimestamp(end);
        list.Execute();

        glFinish();
        REQUIRE(start.IsResultAvailable());
        REQUIRE(end.IsResultAvailable());
        REQUIRE(Rndr::Canvas::GetElapsedMilliseconds(start, end) >= 0.0);
    }

    SECTION("Move transfers ownership")
    {
        Rndr::Canvas::TimestampQuery query("TestQuery");
        query.Record();
        const Rndr::u32 handle = query.GetNativeHandle();

        Rndr::Canvas::TimestampQuery moved(std::move(query));
        REQUIRE(moved.IsValid());
        REQUIRE(moved.IsRecorded());
        REQUIRE(moved.GetNativeHandle() == handle);
        REQUIRE_FALSE(query.IsValid());  // NOLINT(bugprone-use-after-move)
        REQUIRE_FALSE(query.IsRecorded());

        Rndr::Canvas::TimestampQuery assigned;
        assigned = std::move(moved);
        REQUIRE(assigned.IsValid());
        REQUIRE(assigned.GetNativeHandle() == handle);
        REQUIRE_FALSE(moved.IsValid());  // NOLINT(bugprone-use-after-move)
    }

    SECTION("Destroy invalidates the query")
    {
        Rndr::Canvas::TimestampQuery query("TestQuery");
        query.Record();
        query.Destroy();
        REQUIRE_FALSE(query.IsValid());
        REQUIRE_FALSE(query.IsRecorded());
        query.Destroy();
        REQUIRE_FALSE(query.IsValid());
    }
}
