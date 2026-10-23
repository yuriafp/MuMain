#include <doctest.h>

#include <cstdint>
#include <cstdlib>
#include <ctime>

#include "GameLogic/Items/ItemExpiry.h"

using GameLogic::Items::LocalExpiryTime;

namespace
{
// 2026-10-15 12:00:00 UTC.
constexpr std::int64_t kExpiryUtc = 1'792'065'600;

// Local time is UTC from here on, so the expected date and time hold on every machine.
void UseUtcAsLocalTime()
{
#ifdef _WIN32
    _putenv_s("TZ", "UTC0");
    _tzset();
#else
    setenv("TZ", "UTC0", 1);
    tzset();
#endif
}
} // namespace

TEST_CASE("an item the server announced no expiry for has no expiry date")
{
    CHECK_FALSE(LocalExpiryTime(0).has_value());
}

TEST_CASE("the server's expiry time is the date and time it names")
{
    UseUtcAsLocalTime();

    const auto expiry = LocalExpiryTime(kExpiryUtc);

    REQUIRE(expiry.has_value());
    CHECK(expiry->tm_year + 1900 == 2026);
    CHECK(expiry->tm_mon + 1 == 10);
    CHECK(expiry->tm_mday == 15);
    CHECK(expiry->tm_hour == 12);
    CHECK(expiry->tm_min == 0);
}

TEST_CASE("an expiry kept in a 32-bit long gives the same date")
{
    UseUtcAsLocalTime();
    const std::int32_t storedExpiry = static_cast<std::int32_t>(kExpiryUtc);

    const auto expiry = LocalExpiryTime(storedExpiry);

    REQUIRE(expiry.has_value());
    CHECK(expiry->tm_year + 1900 == 2026);
    CHECK(expiry->tm_mday == 15);
}
