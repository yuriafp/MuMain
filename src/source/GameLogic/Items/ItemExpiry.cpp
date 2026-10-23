#include "GameLogic/Items/ItemExpiry.h"

namespace
{
// ITEM::lExpireTime until the server's C1 D2 12 arrives.
constexpr std::int64_t kNoExpiryTime = 0;
} // namespace

std::optional<std::tm> GameLogic::Items::LocalExpiryTime(std::int64_t unixSeconds)
{
    if (unixSeconds == kNoExpiryTime)
        return std::nullopt;

    // localtime gets a time_t of its own: ITEM keeps the time in a long, which is 32 bits on
    // Windows while time_t is 64, so pointing localtime at that long read 4 bytes past it.
    const auto asTime = static_cast<std::time_t>(unixSeconds);
    std::tm local = {};
#if defined(_WIN32)
    if (localtime_s(&local, &asTime) != 0)
        return std::nullopt;
#else
    if (localtime_r(&asTime, &local) == nullptr)
        return std::nullopt;
#endif
    return local;
}
