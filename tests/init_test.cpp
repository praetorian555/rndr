#include <catch2/catch_test_macros.hpp>

#include "rndr/core/init.h"

bool CustomAllocInit(rndr::OpaquePtr init_data, rndr::OpaquePtr* allocator_data)
{
    RNDR_UNUSED(init_data);
    RNDR_UNUSED(allocator_data);
    return true;
}

bool CustomAllocDestroy(rndr::OpaquePtr allocator_data)
{
    RNDR_UNUSED(allocator_data);
    return true;
}

rndr::OpaquePtr CustomAllocate(rndr::OpaquePtr allocator_data, uint64_t size, const char* tag)
{
    RNDR_UNUSED(allocator_data);
    RNDR_UNUSED(size);
    RNDR_UNUSED(tag);
    return nullptr;
}

void CustomFree(rndr::OpaquePtr allocator_data, rndr::OpaquePtr ptr)
{
    RNDR_UNUSED(allocator_data);
    RNDR_UNUSED(ptr);
}

bool CustomLogInit(rndr::OpaquePtr init_data, rndr::OpaquePtr* logger_data)
{
    RNDR_UNUSED(init_data);
    RNDR_UNUSED(logger_data);
    return true;
}

bool CustomLogDestroy(rndr::OpaquePtr logger_data)
{
    RNDR_UNUSED(logger_data);
    return true;
}

void CustomLog(rndr::OpaquePtr logger_data,
               const char* file,
               int line,
               const char* function,
               rndr::LogLevel log_level,
               const char* message)
{
    RNDR_UNUSED(logger_data);
    RNDR_UNUSED(file);
    RNDR_UNUSED(line);
    RNDR_UNUSED(function);
    RNDR_UNUSED(log_level);
    RNDR_UNUSED(message);
}

TEST_CASE("Init", "[init]")
{
    SECTION("Default create and destroy")
    {
        REQUIRE(rndr::Create());
        rndr::Allocator* allocator = rndr::GetAllocator();
        REQUIRE(allocator != nullptr);
        REQUIRE(allocator->allocator_data == nullptr);
        REQUIRE(allocator->init == nullptr);
        REQUIRE(allocator->destroy == nullptr);
        REQUIRE(allocator->allocate == &rndr::DefaultAllocator::Allocate);
        REQUIRE(allocator->free == &rndr::DefaultAllocator::Free);
        rndr::Logger* logger = rndr::GetLogger();
        REQUIRE(logger != nullptr);
        REQUIRE(logger->logger_data != nullptr);
        REQUIRE(logger->init == &rndr::DefaultLogger::Init);
        REQUIRE(logger->destroy == &rndr::DefaultLogger::Destroy);
        REQUIRE(logger->log == &rndr::DefaultLogger::Log);
        REQUIRE(rndr::Destroy());
    }
    constexpr uint64_t k_test_allocator_data = 0xdeadbeefdeadbaaf;
    SECTION("Create with custom allocator")
    {
        rndr::Allocator allocator;
        allocator.allocator_data = reinterpret_cast<rndr::OpaquePtr>(k_test_allocator_data);
        allocator.init = &CustomAllocInit;
        allocator.destroy = &CustomAllocDestroy;
        allocator.allocate = &CustomAllocate;
        allocator.free = &CustomFree;
        REQUIRE(rndr::Create({.user_allocator = allocator}));
        rndr::Allocator* rndr_allocator = rndr::GetAllocator();
        REQUIRE(rndr_allocator != nullptr);
        REQUIRE(rndr_allocator->allocator_data
                == reinterpret_cast<rndr::OpaquePtr>(k_test_allocator_data));
        REQUIRE(rndr_allocator->init == &CustomAllocInit);
        REQUIRE(rndr_allocator->destroy == &CustomAllocDestroy);
        REQUIRE(rndr_allocator->allocate == &CustomAllocate);
        REQUIRE(rndr_allocator->free == &CustomFree);
        rndr::Logger* logger = rndr::GetLogger();
        REQUIRE(logger != nullptr);
        REQUIRE(logger->logger_data != nullptr);
        REQUIRE(logger->init == &rndr::DefaultLogger::Init);
        REQUIRE(logger->destroy == &rndr::DefaultLogger::Destroy);
        REQUIRE(logger->log == &rndr::DefaultLogger::Log);
        REQUIRE(rndr::Destroy());
    }
    SECTION("Create with custom logger")
    {
        rndr::Logger logger;
        logger.logger_data = reinterpret_cast<rndr::OpaquePtr>(k_test_allocator_data);
        logger.init = &CustomLogInit;
        logger.destroy = &CustomLogDestroy;
        logger.log = &CustomLog;
        REQUIRE(rndr::Create({.user_logger = logger}));
        rndr::Allocator* allocator = rndr::GetAllocator();
        REQUIRE(allocator != nullptr);
        REQUIRE(allocator->allocator_data == nullptr);
        REQUIRE(allocator->init == nullptr);
        REQUIRE(allocator->destroy == nullptr);
        REQUIRE(allocator->allocate == &rndr::DefaultAllocator::Allocate);
        REQUIRE(allocator->free == &rndr::DefaultAllocator::Free);
        rndr::Logger* rndr_logger = rndr::GetLogger();
        REQUIRE(rndr_logger != nullptr);
        REQUIRE(rndr_logger->logger_data
                == reinterpret_cast<rndr::OpaquePtr>(k_test_allocator_data));
        REQUIRE(rndr_logger->init == &CustomLogInit);
        REQUIRE(rndr_logger->destroy == &CustomLogDestroy);
        REQUIRE(rndr_logger->log == &CustomLog);
        REQUIRE(rndr::Destroy());
    }
}

TEST_CASE("Allocate and free", "[memory]")
{
    SECTION("Default allocate and free")
    {
        REQUIRE(rndr::Create());
        rndr::OpaquePtr ptr = rndr::Allocate(1, "test");
        REQUIRE(ptr != nullptr);
        rndr::Free(ptr);
        REQUIRE(rndr::Destroy());
    }
}
