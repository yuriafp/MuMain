#pragma once

#include <cstdint>
#include <string>
#include <vector>

// The collective goal of the week on this server (the "TBT: Meta da semana" plugin,
// MUnique.OpenMU.Tbt.WeeklyGoal): one goal per week, like 20 Blood Castles, counted for all
// players together. /meta asks the server for it, which answers with its progress
// (Network/Server/WeeklyGoalPacket.h), and the client opens its window.
namespace GameLogic::Events
{
// What counts for the goal, by the server's numbers (MUnique.OpenMU.Tbt.WeeklyGoal.WeeklyGoalKind).
// The window names the goal in the client's language; a number it doesn't know keeps the
// server's name.
enum class WeeklyGoalKind : std::uint8_t
{
    BloodCastle = 0,
    DevilSquare = 1,
    ChaosCastle = 2,
    Resets = 3,
    TreasureChests = 4,
};

struct WeeklyGoal
{
    bool hasGoal = false;
    bool reached = false;
    // The player's account got the reward of the week.
    bool rewarded = false;
    // The experience bonus for all, once the goal is reached.
    int experienceBonusPercent = 0;
    WeeklyGoalKind kind = WeeklyGoalKind::BloodCastle;
    int count = 0;
    int target = 0;
    // What the player's account counted.
    int contribution = 0;
    // The end of the week, as Unix time in seconds (UTC).
    std::int64_t weekEnd = 0;
    // The reward of each account which counted; a plus of 0 is no Box of Kundun.
    int jewelsOfBless = 0;
    int jewelsOfSoul = 0;
    int boxOfKundunPlus = 0;
    int goblinPoints = 0;
    // UTF-8, as the server's configuration names it, for a kind this client doesn't know.
    std::string name;

    bool operator==(const WeeklyGoal&) const = default;
};

constexpr int FullPercent = 100;

// The progress, 0 to 100; 100 only once the goal is reached.
int ProgressPercent(const WeeklyGoal& goal);

// What is missing to reach the goal, 0 once it's reached.
int Remaining(const WeeklyGoal& goal);

// The item level of a Box of Kundun +plus (level 8 is +1).
int BoxOfKundunLevel(int plus);

// Joins the parts of a list, e.g. "a, b and c": `nextFormat` joins one more part ("%ls, %ls"),
// `lastFormat` the last one ("%ls and %ls"). Both take the list so far, then the part.
std::wstring JoinList(const std::vector<std::wstring>& parts, const wchar_t* nextFormat, const wchar_t* lastFormat);

// The goal of the week, as the server last sent it.
const WeeklyGoal& CurrentWeeklyGoal();
void SetCurrentWeeklyGoal(WeeklyGoal goal);
} // namespace GameLogic::Events
