#include "Network/Server/WeeklyGoalPacket.h"

#include <cstring>
#include <string>

namespace Network::WeeklyGoal
{
namespace
{
// A text field up to its first zero; a field without one is read whole.
template <std::size_t Length>
std::string ReadText(const char (&field)[Length])
{
    const auto* end = static_cast<const char*>(std::memchr(field, 0, Length));
    return std::string(field, end != nullptr ? end : field + Length);
}
} // namespace

bool Read(const std::uint8_t* data, std::size_t size, GameLogic::Events::WeeklyGoal& goal)
{
    if (data == nullptr || size < sizeof(Packet))
        return false;

    Packet packet;
    std::memcpy(&packet, data, sizeof(Packet));
    goal.hasGoal = (packet.flags & HasGoal) != 0;
    goal.reached = (packet.flags & Reached) != 0;
    goal.rewarded = (packet.flags & Rewarded) != 0;
    goal.experienceBonusPercent = packet.experienceBonusPercent;
    goal.kind = static_cast<GameLogic::Events::WeeklyGoalKind>(packet.kind);
    goal.count = packet.count;
    goal.target = packet.target;
    goal.contribution = packet.contribution;
    goal.weekEnd = packet.weekEnd;
    goal.jewelsOfBless = packet.jewelsOfBless;
    goal.jewelsOfSoul = packet.jewelsOfSoul;
    goal.boxOfKundunPlus = packet.boxOfKundunPlus;
    goal.goblinPoints = packet.goblinPoints;
    goal.name = ReadText(packet.name);
    return true;
}
} // namespace Network::WeeklyGoal
