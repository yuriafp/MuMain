#include "WorkshopOptions.h"

#include <bit>

namespace GameLogic::Items::Workshop
{
namespace
{
constexpr unsigned ExcellentFlagsMask = (1u << ExcellentOptionCount) - 1;

Status GetItemStatus(const Item& item)
{
    if (item.ancient)
        return Status::Ancient;
    if (item.socket)
        return Status::Socket;
    if (item.level > MaximumItemLevel)
        return Status::LevelTooHigh;
    return Status::Ready;
}
} // namespace

Options::Options(std::span<const Item> items)
{
    if (items.empty())
        return;
    if (items.size() > 1)
    {
        m_Status = Status::TooManyItems;
        return;
    }

    const Item& item = items.front();
    m_ExcellentCount = std::popcount(static_cast<unsigned>(item.excellentFlags) & ExcellentFlagsMask);
    m_Status = GetItemStatus(item);
    if (m_Status != Status::Ready)
        return;

    AddChoices(item);
    if (m_ChoiceCount == 0)
        m_Status = Status::NoOptionLeft;
}

void Options::AddChoices(const Item& item)
{
    if (!item.luck)
        m_Choices[m_ChoiceCount++] = LuckChoice;
    if (m_ExcellentCount >= MaximumExcellentOptions)
        return;

    const unsigned excellentFlags = static_cast<unsigned>(item.excellentFlags);
    for (int number = ExcellentOptionCount; number >= 1; --number)
    {
        if ((excellentFlags & (1u << (number - 1))) == 0)
            m_Choices[m_ChoiceCount++] = number;
    }
}

int GetRecipeIndex(int choice, int excellentCount)
{
    return choice == LuckChoice ? 0 : excellentCount + 1;
}
}
