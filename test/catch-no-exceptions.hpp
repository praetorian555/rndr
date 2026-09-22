#pragma once

#include <new>

#include <catch2/catch2.hpp>

/**
 * Catch2 leaves a test case early by throwing, and this binary is built without exceptions - opal
 * propagates /EHs-c- and -fno-exceptions to everything that links it - so every macro that does so ends the
 * process instead. SKIP is the one that has to be fixed rather than lived with: it fires on any machine
 * that cannot run a case, which is every CI runner, and it took the whole run down with it.
 *
 * The reporting half of SKIP never needed the throw. The handler records the result before complete() is
 * reached and complete() only throws to unwind, so the replacement below records the same result and leaves
 * the test case by returning. The handler is constructed in place and never destroyed: its destructor
 * reports an incomplete assertion for a handler that never reached complete(), and complete() is the half
 * that throws. Every member of it is trivially destructible, so nothing is left behind by not calling it.
 *
 * Two consequences of the return, both shared with the throwing version:
 *  - Nothing later in the enclosing TEST_CASE runs, sections included.
 *  - It only works in the test case body. Inside a lambda or a helper it returns from that and the case
 *    carries on. A helper that returns something says so at compile time; a void one does not.
 *
 * REQUIRE and FAIL keep the throw, because the same trick cannot work for them: much of this suite asserts
 * inside helpers that return a value - ForgeTest::Unwrap and friends - and such a helper has nothing to
 * return once the assertion fails. So a failing assertion ends the run rather than the case, and the cases
 * after it do not run. It ends it through the handler in catch-no-exceptions.cpp, which reports the failure
 * and exits; Catch2's own version calls std::terminate, which in a Debug CRT is a dialog box that a CI
 * runner waits on until the job's ceiling.
 */
#undef SKIP
#define SKIP(...)                                                                                            \
    do                                                                                                       \
    {                                                                                                        \
        alignas(Catch::AssertionHandler) unsigned char rndr_skip_storage[sizeof(Catch::AssertionHandler)];    \
        auto* rndr_skip_handler = ::new (static_cast<void*>(rndr_skip_storage)) Catch::AssertionHandler(      \
            "SKIP"_catch_sr, CATCH_INTERNAL_LINEINFO, Catch::StringRef(), Catch::ResultDisposition::Normal);  \
        rndr_skip_handler->handleMessage(Catch::ResultWas::ExplicitSkip,                                      \
                                         (Catch::MessageStream() << __VA_ARGS__ + ::Catch::StreamEndStop())   \
                                             .m_stream.str());                                                \
        return;                                                                                               \
    } while (false)
