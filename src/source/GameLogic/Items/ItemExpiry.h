#pragma once

#include <cstdint>
#include <ctime>
#include <optional>

namespace GameLogic::Items
{
// The local date and time a rented item expires at. `unixSeconds` is the expiry the server
// announces (C1 D2 12, Unix time in UTC); 0 means it has not announced one.
[[nodiscard]] std::optional<std::tm> LocalExpiryTime(std::int64_t unixSeconds);
} // namespace GameLogic::Items
