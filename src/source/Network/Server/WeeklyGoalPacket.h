#pragma once

#include <cstddef>
#include <cstdint>

#include "GameLogic/Events/WeeklyGoal.h"

// The answer of our server to /meta: C1 64 FB 01 (MUnique.OpenMU.Tbt.WeeklyGoal.WeeklyGoalRef).
// The head code 0xFB is free in OpenMU and MuMain.
namespace Network::WeeklyGoal
{
constexpr std::uint8_t HeadCode = 0xFB;
constexpr std::uint8_t SubCode = 0x01;
constexpr std::size_t NameLength = 64;

#pragma pack(push, 1)
struct Packet
{
    std::uint8_t type;
    std::uint8_t size;
    std::uint8_t headCode;
    std::uint8_t subCode;
    std::uint8_t flags;
    std::uint8_t experienceBonusPercent;
    // GameLogic::Events::WeeklyGoalKind.
    std::uint8_t kind;
    // 0 for no Box of Kundun.
    std::uint8_t boxOfKundunPlus;
    std::int32_t count;
    std::int32_t target;
    std::int32_t contribution;
    // Unix time in seconds (UTC).
    std::int32_t weekEnd;
    std::int32_t jewelsOfBless;
    std::int32_t jewelsOfSoul;
    std::int32_t goblinPoints;
    // UTF-8, ended by a zero.
    char name[NameLength];
};
#pragma pack(pop)

static_assert(sizeof(Packet) == 100, "C1 FB 01 is 100 bytes");
static_assert(offsetof(Packet, count) == 8, "C1 FB 01 has the count at byte 8");
static_assert(offsetof(Packet, jewelsOfBless) == 24, "C1 FB 01 has the reward at byte 24");
static_assert(offsetof(Packet, name) == 36, "C1 FB 01 has the name at byte 36");

enum Flags : std::uint8_t
{
    HasGoal = 1,
    Reached = 2,
    Rewarded = 4,
};

// Reads the packet; false when it's shorter than the packet.
bool Read(const std::uint8_t* data, std::size_t size, GameLogic::Events::WeeklyGoal& goal);
} // namespace Network::WeeklyGoal
