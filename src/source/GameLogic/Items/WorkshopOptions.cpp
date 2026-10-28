#include "WorkshopOptions.h"

#include <algorithm>
#include <bit>

namespace GameLogic::Items::Workshop
{
namespace
{
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

int GetMaximumOptions(Kind kind)
{
    return kind == Kind::Equipment ? MaximumExcellentOptions : MaximumWingOptions;
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
    m_Kind = item.kind;
    // The flags above the item's options aren't options: the wings keep the kind of their
    // additional option there.
    const int optionCount = std::clamp(item.optionCount, 0, ExcellentOptionCount);
    const unsigned optionMask = (1u << optionCount) - 1;
    m_OptionCount = std::popcount(static_cast<unsigned>(item.optionFlags) & optionMask);
    m_Status = GetItemStatus(item);
    if (m_Status != Status::Ready)
        return;

    AddChoices(item, optionCount);
    if (m_ChoiceCount == 0)
        m_Status = Status::NoOptionLeft;
}

void Options::AddChoices(const Item& item, int optionCount)
{
    if (!item.luck)
        m_Choices[m_ChoiceCount++] = LuckChoice;
    if (m_OptionCount >= GetMaximumOptions(item.kind))
        return;

    const unsigned optionFlags = static_cast<unsigned>(item.optionFlags);
    const auto addIfMissing = [&](int number)
    {
        if ((optionFlags & (1u << (number - 1))) == 0)
            m_Choices[m_ChoiceCount++] = number;
    };
    // The tooltip lists the excellent options from the last one, and the wing options from the first.
    if (item.kind == Kind::Equipment)
    {
        for (int number = optionCount; number >= 1; --number)
            addIfMissing(number);
    }
    else
    {
        for (int number = 1; number <= optionCount; ++number)
            addIfMissing(number);
    }
}

void SetWing(Item& item, WingTier tier, bool capeOfLord)
{
    switch (tier)
    {
    case WingTier::First:
        item.kind = Kind::Wing;
        item.optionCount = 0;
        break;
    case WingTier::Second:
        item.kind = Kind::Wing;
        item.optionCount = capeOfLord ? CapeOfLordOptionCount : SecondTierWingOptionCount;
        break;
    case WingTier::Third:
        item.kind = Kind::ThirdTierWing;
        item.optionCount = ThirdTierWingOptionCount;
        break;
    }
}

int GetRecipeIndex(int choice, Kind kind, int optionCount)
{
    switch (kind)
    {
    case Kind::Wing:
        return choice == LuckChoice ? WingLuckRecipe : WingOptionRecipe;
    case Kind::ThirdTierWing:
        return choice == LuckChoice ? WingLuckRecipe : ThirdTierWingOptionRecipe;
    case Kind::Equipment:
        break;
    }
    return choice == LuckChoice ? LuckRecipe : FirstExcellentOptionRecipe + optionCount;
}
}
