#include "GameLogic/Events/WeeklyGoal.h"

#include <algorithm>
#include <cwchar>
#include <utility>

namespace GameLogic::Events
{
namespace
{
// The level of a Box of Kundun +1; the boxes +2 to +5 follow it.
constexpr int BoxOfKundunPlusOneLevel = 8;

WeeklyGoal s_CurrentWeeklyGoal;

// One format of JoinList(): the list so far, then the part.
std::wstring Join(const wchar_t* format, const std::wstring& list, const std::wstring& part)
{
    // The format's own length bounds what it adds around the two texts.
    std::vector<wchar_t> buffer(std::wcslen(format) + list.size() + part.size() + 1);
    const int length = std::swprintf(buffer.data(), buffer.size(), format, list.c_str(), part.c_str());
    return length < 0 ? list : std::wstring(buffer.data(), static_cast<std::size_t>(length));
}
} // namespace

int ProgressPercent(const WeeklyGoal& goal)
{
    if (goal.reached)
        return FullPercent;

    if (goal.target <= 0 || goal.count <= 0)
        return 0;

    // Below 100 until the server records the goal as reached.
    const auto percent = static_cast<std::int64_t>(goal.count) * FullPercent / goal.target;
    return static_cast<int>(std::min<std::int64_t>(percent, FullPercent - 1));
}

int Remaining(const WeeklyGoal& goal)
{
    if (goal.reached)
        return 0;

    return std::max(goal.target - goal.count, 0);
}

int BoxOfKundunLevel(int plus)
{
    return BoxOfKundunPlusOneLevel + plus - 1;
}

std::wstring JoinList(const std::vector<std::wstring>& parts, const wchar_t* nextFormat, const wchar_t* lastFormat)
{
    if (parts.empty())
        return {};

    std::wstring list = parts.front();
    for (std::size_t i = 1; i < parts.size(); ++i)
        list = Join(i + 1 == parts.size() ? lastFormat : nextFormat, list, parts[i]);
    return list;
}

const WeeklyGoal& CurrentWeeklyGoal()
{
    return s_CurrentWeeklyGoal;
}

void SetCurrentWeeklyGoal(WeeklyGoal goal)
{
    s_CurrentWeeklyGoal = std::move(goal);
}
} // namespace GameLogic::Events
