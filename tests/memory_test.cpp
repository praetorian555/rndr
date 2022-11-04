#include <catch2/catch_test_macros.hpp>

#include "math/math.h"

#include "rndr/core/rndrapp.h"

TEST_CASE("SingleObject", "Memory")
{
    rndr::RndrApp* App = new rndr::RndrApp();
    REQUIRE(App != nullptr);

    int* MyInt = RNDR_NEW(App, int, "My Int", 5);
    REQUIRE(*MyInt == 5);

    struct AggregateType
    {
        int a;
        double b;
    };

    RNDR_DELETE(App, int, MyInt);

    AggregateType* A = RNDR_NEW(App, AggregateType, "", 10, 15.2);
    REQUIRE(A->a == 10);
    REQUIRE(A->b == 15.2);

    RNDR_DELETE(App, AggregateType, A);

    struct AggregateType2
    {
        int a = 0;
        double b = 0;

        AggregateType2(int aa) { a = 10; }
    };
    AggregateType2* B = RNDR_NEW(App, AggregateType2, "", 20);
    REQUIRE(B->a == 10);

    RNDR_DELETE(App, AggregateType2, B);

    delete App;
}

TEST_CASE("Arrays", "Memory")
{
    rndr::RndrApp* App = new rndr::RndrApp();
    REQUIRE(App != nullptr);

    int* MyInts = RNDR_NEW_ARRAY(App, int, 100, "My Int");
    for (int i = 0; i < 100; i++)
    {
        REQUIRE(MyInts[i] == 0);
    }
    RNDR_DELETE_ARRAY(App, int, MyInts, 100);

    struct AggregateType
    {
        int a = 2;
        double b = 5.5;
    };

    AggregateType* A = RNDR_NEW_ARRAY(App, AggregateType, 10, "");
    for (int i = 0; i < 10; i++)
    {
        REQUIRE(A->a == 2);
        REQUIRE(A->b == 5.5);
    }
    RNDR_DELETE_ARRAY(App, AggregateType, A, 10);

    delete App;
}
