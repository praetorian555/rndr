#include "rndr/forge/query.hpp"

#include "rndr/forge/vulkan-exception.hpp"

namespace
{
/** Milliseconds a nanosecond count stands for, so the conversion is written in one place. */
constexpr Rndr::f64 k_nanoseconds_per_millisecond = 1'000'000.0;

/** "Reading 2 queries from index 3 of a timestamp query pool holding 2", since a range error that names no range is half a message. */
Opal::StringEx RangeMessage(const char* what, Rndr::u32 first_query, Rndr::u32 query_count, Rndr::u32 pool_size)
{
    return Opal::StringEx(what) + " " + query_count + " queries from index " + first_query +
           " of a timestamp query pool holding " + pool_size + "!";
}

/**
 * The one call both readers are, and the only place the VK_QUERY_RESULT_* flags are spelled out. Ticks come
 * back masked, since the bits above what the family writes are undefined and would otherwise turn a
 * difference between two of them into noise.
 */
bool ReadQueryResults(const Rndr::Forge::Device& device, VkQueryPool query_pool, Rndr::u32 first_query, Rndr::u64* out_ticks,
                      Rndr::u32 query_count, Rndr::u64 valid_bits_mask, bool wait)
{
    const VkQueryResultFlags flags = VK_QUERY_RESULT_64_BIT | (wait ? VK_QUERY_RESULT_WAIT_BIT : 0u);
    const VkResult result = vkGetQueryPoolResults(device.GetNativeDevice(), query_pool, first_query, query_count,
                                                  static_cast<size_t>(query_count) * sizeof(Rndr::u64), out_ticks,
                                                  sizeof(Rndr::u64), flags);
    // Without the wait bit this is the answer "the device has not run that far yet", which is an outcome
    // rather than a failure - see the error handling section of docs/forge.md.
    if (result == VK_NOT_READY && !wait)
    {
        return false;
    }
    if (result != VK_SUCCESS)
    {
        throw Rndr::Forge::VulkanException(result, "vkGetQueryPoolResults");
    }
    for (Rndr::u32 i = 0; i < query_count; ++i)
    {
        out_ticks[i] &= valid_bits_mask;
    }
    return true;
}

/** Ticks to milliseconds, zero when the counter did not move forward, which is what a wrapped one looks like. */
Rndr::f64 ToMilliseconds(Rndr::u64 start_ticks, Rndr::u64 end_ticks, Rndr::f32 timestamp_period)
{
    if (end_ticks <= start_ticks)
    {
        return 0.0;
    }
    const auto elapsed_nanoseconds = static_cast<Rndr::f64>(end_ticks - start_ticks) * static_cast<Rndr::f64>(timestamp_period);
    return elapsed_nanoseconds / k_nanoseconds_per_millisecond;
}
}  // namespace

Rndr::Forge::TimestampQueryPool::TimestampQueryPool(const Device& device, const TimestampQueryPoolDesc& desc)
    : m_device(&device), m_desc(desc)
{
    if (desc.query_count == 0)
    {
        throw Opal::Exception("A timestamp query pool needs at least one query!");
    }

    // Throws when the device was not created with this queue, so the family index below is always a real one.
    const DeviceQueue& queue = device.GetQueue(desc.queue_family);
    const u32 queue_family_index = queue.GetQueueFamilyIndex();
    const VkQueueFamilyProperties& family_properties = device.GetPhysicalDevice().GetQueueFamilyProperties()[queue_family_index];

    // A family that writes no valid bits cannot time anything, and every measurement it produced would be a
    // zero that reads like a fast operation rather than like a missing capability.
    if (family_properties.timestampValidBits == 0)
    {
        throw Opal::Exception("This queue family does not support timestamp queries!");
    }
    // Shifting a 64 bit value by 64 is undefined, and a family that writes all 64 bits is the common case.
    m_valid_bits_mask =
        family_properties.timestampValidBits >= 64 ? UINT64_MAX : (1ull << family_properties.timestampValidBits) - 1ull;
    m_timestamp_period = device.GetPhysicalDevice().GetProperties().limits.timestampPeriod;

    const VkQueryPoolCreateInfo create_info{
        .sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO, .queryType = VK_QUERY_TYPE_TIMESTAMP, .queryCount = desc.query_count};
    const VkResult result = vkCreateQueryPool(device.GetNativeDevice(), &create_info, nullptr, &m_query_pool);
    if (result != VK_SUCCESS)
    {
        throw VulkanException(result, "vkCreateQueryPool");
    }
}

Rndr::Forge::TimestampQueryPool::~TimestampQueryPool()
{
    Destroy();
}

void Rndr::Forge::TimestampQueryPool::Destroy()
{
    if (m_query_pool != VK_NULL_HANDLE)
    {
        vkDestroyQueryPool(m_device->GetNativeDevice(), m_query_pool, nullptr);
        m_query_pool = VK_NULL_HANDLE;
    }
}

Rndr::Forge::TimestampQueryPool::TimestampQueryPool(TimestampQueryPool&& other) noexcept
    : m_device(std::move(other.m_device)),
      m_query_pool(other.m_query_pool),
      m_desc(other.m_desc),
      m_timestamp_period(other.m_timestamp_period),
      m_valid_bits_mask(other.m_valid_bits_mask)
{
    other.m_query_pool = VK_NULL_HANDLE;
}

Rndr::Forge::TimestampQueryPool& Rndr::Forge::TimestampQueryPool::operator=(TimestampQueryPool&& other) noexcept
{
    if (this != &other)
    {
        Destroy();
        m_device = std::move(other.m_device);
        m_query_pool = other.m_query_pool;
        m_desc = other.m_desc;
        m_timestamp_period = other.m_timestamp_period;
        m_valid_bits_mask = other.m_valid_bits_mask;
        other.m_query_pool = VK_NULL_HANDLE;
    }
    return *this;
}

Rndr::u32 Rndr::Forge::TimestampQueryPool::ResolveQueryRange(u32 first_query, u32 query_count, const char* what) const
{
    const u32 pool_size = m_desc.query_count;
    if (first_query >= pool_size)
    {
        throw Opal::Exception(RangeMessage(what, first_query, query_count, pool_size));
    }
    const u32 remaining = pool_size - first_query;
    const u32 resolved = query_count == k_all_queries ? remaining : query_count;
    if (resolved == 0 || resolved > remaining)
    {
        throw Opal::Exception(RangeMessage(what, first_query, resolved, pool_size));
    }
    return resolved;
}

void Rndr::Forge::TimestampQueryPool::Reset(u32 first_query, u32 query_count) const
{
    const u32 resolved_count = ResolveQueryRange(first_query, query_count, "Resetting");
    // vkResetQueryPool is the host side of the reset and belongs to the feature the device has to be asked
    // for. Calling it on a device that did not enable it is undefined, not a call that fails.
    if (!m_device->GetFeatures().host_query_reset)
    {
        throw Opal::Exception("Resetting a query pool from the host needs DeviceFeatures::host_query_reset!");
    }
    vkResetQueryPool(m_device->GetNativeDevice(), m_query_pool, first_query, resolved_count);
}

void Rndr::Forge::TimestampQueryPool::GetResults(Opal::ArrayView<u64> out_results, u32 first_query) const
{
    const u32 count = ResolveQueryRange(first_query, static_cast<u32>(out_results.GetSize()), "Reading");
    ReadQueryResults(*m_device, m_query_pool, first_query, out_results.GetData(), count, m_valid_bits_mask, true);
}

bool Rndr::Forge::TimestampQueryPool::TryGetResults(Opal::ArrayView<u64> out_results, u32 first_query) const
{
    const u32 count = ResolveQueryRange(first_query, static_cast<u32>(out_results.GetSize()), "Reading");
    return ReadQueryResults(*m_device, m_query_pool, first_query, out_results.GetData(), count, m_valid_bits_mask, false);
}

Rndr::f64 Rndr::Forge::TimestampQueryPool::GetElapsedMilliseconds(u32 start_query, u32 end_query) const
{
    // One query at a time rather than the range that spans them. A read of a range is unavailable when any
    // query in it is, so a pool whose middle queries were never written - which is what measuring one
    // operation out of several looks like - would never report a result for the pair at its ends.
    u64 start_ticks = 0;
    u64 end_ticks = 0;
    ResolveQueryRange(start_query, 1, "Reading");
    ResolveQueryRange(end_query, 1, "Reading");
    ReadQueryResults(*m_device, m_query_pool, start_query, &start_ticks, 1, m_valid_bits_mask, true);
    ReadQueryResults(*m_device, m_query_pool, end_query, &end_ticks, 1, m_valid_bits_mask, true);
    return ToMilliseconds(start_ticks, end_ticks, m_timestamp_period);
}

bool Rndr::Forge::TimestampQueryPool::TryGetElapsedMilliseconds(u32 start_query, u32 end_query, f64& out_milliseconds) const
{
    u64 start_ticks = 0;
    u64 end_ticks = 0;
    ResolveQueryRange(start_query, 1, "Reading");
    ResolveQueryRange(end_query, 1, "Reading");
    if (!ReadQueryResults(*m_device, m_query_pool, start_query, &start_ticks, 1, m_valid_bits_mask, false) ||
        !ReadQueryResults(*m_device, m_query_pool, end_query, &end_ticks, 1, m_valid_bits_mask, false))
    {
        return false;
    }
    out_milliseconds = ToMilliseconds(start_ticks, end_ticks, m_timestamp_period);
    return true;
}
