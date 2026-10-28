#include "doctest.h"
#include "GameLogic/Events/WeeklyGoal.h"
#include "Network/Server/WeeklyGoalPacket.h"

#include <array>
#include <cstddef>
#include <cstring>
#include <string>

using GameLogic::Events::WeeklyGoal;

namespace
{
// The packet as the server writes it (MUnique.OpenMU.Tbt.Tests.WeeklyGoalTest).
std::array<std::uint8_t, sizeof(Network::WeeklyGoal::Packet)> ServerPacket(const char* name)
{
    std::array<std::uint8_t, sizeof(Network::WeeklyGoal::Packet)> data{};
    data[0] = 0xC1;
    data[1] = static_cast<std::uint8_t>(data.size());
    data[2] = Network::WeeklyGoal::HeadCode;
    data[3] = Network::WeeklyGoal::SubCode;
    data[4] = Network::WeeklyGoal::HasGoal | Network::WeeklyGoal::Reached | Network::WeeklyGoal::Rewarded;
    data[5] = 20;
    data[6] = 2; // Chaos Castle
    data[7] = 3; // Box of Kundun +3
    const std::int32_t values[] = {21, 20, 7, 1794189600, 1, 2, 100};
    std::memcpy(&data[offsetof(Network::WeeklyGoal::Packet, count)], values, sizeof(values));
    std::memcpy(&data[offsetof(Network::WeeklyGoal::Packet, name)], name, std::strlen(name));
    return data;
}
} // namespace

TEST_CASE("The packet of the server reads into the goal")
{
    const auto data = ServerPacket("entradas no Chaos Castle");
    WeeklyGoal goal;
    REQUIRE(Network::WeeklyGoal::Read(data.data(), data.size(), goal));
    CHECK(goal.hasGoal);
    CHECK(goal.reached);
    CHECK(goal.rewarded);
    CHECK(goal.experienceBonusPercent == 20);
    CHECK(goal.count == 21);
    CHECK(goal.target == 20);
    CHECK(goal.contribution == 7);
    CHECK(goal.weekEnd == 1794189600);
    CHECK(goal.kind == GameLogic::Events::WeeklyGoalKind::ChaosCastle);
    CHECK(goal.jewelsOfBless == 1);
    CHECK(goal.jewelsOfSoul == 2);
    CHECK(goal.boxOfKundunPlus == 3);
    CHECK(goal.goblinPoints == 100);
    CHECK(goal.name == "entradas no Chaos Castle");
}

TEST_CASE("The reward joins its parts in the language's way")
{
    const wchar_t* next = L"%ls, %ls";
    const wchar_t* last = L"%ls and %ls";
    CHECK(GameLogic::Events::JoinList({}, next, last).empty());
    CHECK(GameLogic::Events::JoinList({L"1 Jewel of Bless"}, next, last) == L"1 Jewel of Bless");
    CHECK(GameLogic::Events::JoinList({L"a", L"b"}, next, last) == L"a and b");
    CHECK(GameLogic::Events::JoinList({L"a", L"b", L"c", L"d"}, next, L"%ls e %ls") == L"a, b, c e d");
}

TEST_CASE("A Box of Kundun +1 is level 8")
{
    CHECK(GameLogic::Events::BoxOfKundunLevel(1) == 8);
    CHECK(GameLogic::Events::BoxOfKundunLevel(3) == 10);
    CHECK(GameLogic::Events::BoxOfKundunLevel(5) == 12);
}

TEST_CASE("A short packet is refused, and a name without its zero is read whole")
{
    auto data = ServerPacket("");
    WeeklyGoal goal;
    CHECK_FALSE(Network::WeeklyGoal::Read(data.data(), data.size() - 1, goal));

    std::memset(&data[offsetof(Network::WeeklyGoal::Packet, name)], 'a', Network::WeeklyGoal::NameLength);
    REQUIRE(Network::WeeklyGoal::Read(data.data(), data.size(), goal));
    CHECK(goal.name == std::string(Network::WeeklyGoal::NameLength, 'a'));
}

TEST_CASE("The progress counts to the target, and is whole only once the goal is reached")
{
    WeeklyGoal goal;
    goal.target = 20;
    CHECK(GameLogic::Events::ProgressPercent(goal) == 0);
    CHECK(GameLogic::Events::Remaining(goal) == 20);

    goal.count = 12;
    CHECK(GameLogic::Events::ProgressPercent(goal) == 60);
    CHECK(GameLogic::Events::Remaining(goal) == 8);

    goal.count = 25;
    CHECK(GameLogic::Events::ProgressPercent(goal) == 99);
    CHECK(GameLogic::Events::Remaining(goal) == 0);

    goal.reached = true;
    CHECK(GameLogic::Events::ProgressPercent(goal) == GameLogic::Events::FullPercent);
    CHECK(GameLogic::Events::Remaining(goal) == 0);
}

TEST_CASE("A goal without a target has no progress")
{
    WeeklyGoal goal;
    goal.count = 5;
    CHECK(GameLogic::Events::ProgressPercent(goal) == 0);
    CHECK(GameLogic::Events::Remaining(goal) == 0);
}

TEST_CASE("Another answer of the server is another goal, the same one is not")
{
    const auto data = ServerPacket("Blood Castles completos");
    WeeklyGoal shown;
    WeeklyGoal received;
    REQUIRE(Network::WeeklyGoal::Read(data.data(), data.size(), shown));
    REQUIRE(Network::WeeklyGoal::Read(data.data(), data.size(), received));
    CHECK(received == shown);

    received.count++;
    CHECK_FALSE(received == shown);
}
