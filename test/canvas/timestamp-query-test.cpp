#include <catch2/catch2.hpp>

#include "canvas-test-common.hpp"

#include "glad/glad.h"

#include "opal/container/scope-ptr.h"

#include "rndr/application.hpp"
#include "rndr/canvas/context.hpp"
#include "rndr/canvas/draw-list.hpp"
#include "rndr/canvas/timestamp-query.hpp"
#include "rndr/generic-window.hpp"

namespace
{

struct TimestampQueryTestFixture
{
    Opal::ScopePtr<Rndr::Application> app;
    Opal::Ref<Rndr::GenericWindow> window;
    Rndr::Canvas::Context context;

    TimestampQueryTestFixture() : context(CanvasTest::CreateTestContext(app, window)) {}
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
        REQUIRE(query.Record() == Rndr::ErrorCode::InvalidArgument);
        REQUIRE(query.GetResult().GetError() == Rndr::ErrorCode::InvalidArgument);
    }

    SECTION("Create query")
    {
        auto query = CanvasTest::Unwrap(Rndr::Canvas::TimestampQuery::Create("TestQuery"));
        REQUIRE(query.IsValid());
        REQUIRE(query.GetNativeHandle() != 0);
        REQUIRE(query.GetName() == "TestQuery");
        REQUIRE_FALSE(query.IsRecorded());
    }

    SECTION("Reading a query that was never recorded reports InvalidArgument")
    {
        auto query = CanvasTest::Unwrap(Rndr::Canvas::TimestampQuery::Create("TestQuery"));
        REQUIRE(query.GetResult().GetError() == Rndr::ErrorCode::InvalidArgument);

        Rndr::u64 result = 0;
        REQUIRE(query.TryGetResult(result).GetError() == Rndr::ErrorCode::InvalidArgument);
        REQUIRE(result == 0);
    }

    SECTION("Record and read a timestamp")
    {
        auto query = CanvasTest::Unwrap(Rndr::Canvas::TimestampQuery::Create("TestQuery"));
        REQUIRE(query.Record() == Rndr::ErrorCode::Success);
        REQUIRE(query.IsRecorded());

        glFinish();
        REQUIRE(query.IsResultAvailable());
        REQUIRE(CanvasTest::Unwrap(query.GetResult()) != 0);

        Rndr::u64 result = 0;
        REQUIRE(CanvasTest::Unwrap(query.TryGetResult(result)));
        REQUIRE(result != 0);
    }

    SECTION("Elapsed time between two timestamps")
    {
        auto start = CanvasTest::Unwrap(Rndr::Canvas::TimestampQuery::Create("Start"));
        auto end = CanvasTest::Unwrap(Rndr::Canvas::TimestampQuery::Create("End"));

        REQUIRE(start.Record() == Rndr::ErrorCode::Success);
        glFinish();
        REQUIRE(end.Record() == Rndr::ErrorCode::Success);
        glFinish();

        REQUIRE(CanvasTest::Unwrap(end.GetResult()) >= CanvasTest::Unwrap(start.GetResult()));
        REQUIRE(CanvasTest::Unwrap(Rndr::Canvas::GetElapsedNanoseconds(start, end))
                == CanvasTest::Unwrap(end.GetResult()) - CanvasTest::Unwrap(start.GetResult()));
        REQUIRE(CanvasTest::Unwrap(Rndr::Canvas::GetElapsedMilliseconds(end, start)) == 0.0);
    }

    SECTION("Record through a draw list")
    {
        auto start = CanvasTest::Unwrap(Rndr::Canvas::TimestampQuery::Create("Start"));
        auto end = CanvasTest::Unwrap(Rndr::Canvas::TimestampQuery::Create("End"));

        Rndr::Canvas::DrawList list;
        list.WriteTimestamp(start);
        list.SetRenderTarget(f.context);
        list.Clear({0, 0, 0, 1});
        list.WriteTimestamp(end);
        list.Execute();

        glFinish();
        REQUIRE(start.IsResultAvailable());
        REQUIRE(end.IsResultAvailable());
        REQUIRE(CanvasTest::Unwrap(Rndr::Canvas::GetElapsedMilliseconds(start, end)) >= 0.0);
    }

    SECTION("Move transfers ownership")
    {
        auto query = CanvasTest::Unwrap(Rndr::Canvas::TimestampQuery::Create("TestQuery"));
        REQUIRE(query.Record() == Rndr::ErrorCode::Success);
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
        auto query = CanvasTest::Unwrap(Rndr::Canvas::TimestampQuery::Create("TestQuery"));
        REQUIRE(query.Record() == Rndr::ErrorCode::Success);
        query.Destroy();
        REQUIRE_FALSE(query.IsValid());
        REQUIRE_FALSE(query.IsRecorded());
        query.Destroy();
        REQUIRE_FALSE(query.IsValid());
    }
}
