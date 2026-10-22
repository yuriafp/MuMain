#pragma once

#include <array>
#include <span>

// The Oficina, this server's NPC in the chaos card window: it puts the Luck or one excellent option
// of the player's choice into a weapon, armor or shield. The mix request carries the choice in its
// socket slot byte (MUnique.OpenMU.Tbt.Workshop.WorkshopCraftingHandler on the server): 0 is the
// Luck, and 1 to 6 the number of the excellent option, which is bit (number - 1) of the item's
// excellent flags.
namespace GameLogic::Items::Workshop
{
// As configured on the server (the "TBT: Oficina" plugin). A change there needs the same change
// here and in the chaos card recipes of Mix.bmd (jewels, zen and rates).
constexpr int MaximumItemLevel = 11;
constexpr int MaximumExcellentOptions = 3;

constexpr int LuckChoice = 0;
constexpr int ExcellentOptionCount = 6;

// A weapon, armor or shield in the mix box.
struct Item
{
    int level = 0;
    int excellentFlags = 0;
    bool luck = false;
    bool ancient = false;
    bool socket = false;
};

enum class Status
{
    NoItem,
    TooManyItems,
    Ancient,
    Socket,
    LevelTooHigh,
    NoOptionLeft,
    Ready,
};

class Options
{
public:
    Options() = default;
    // The options for the weapons, armors and shields in the mix box; Ready needs exactly one.
    explicit Options(std::span<const Item> items);

    Status GetStatus() const { return m_Status; }
    int ExcellentCount() const { return m_ExcellentCount; }
    // The Luck first, then the excellent options in the item tooltip's order.
    std::span<const int> Choices() const { return {m_Choices.data(), static_cast<size_t>(m_ChoiceCount)}; }

private:
    void AddChoices(const Item& item);

    Status m_Status = Status::NoItem;
    int m_ExcellentCount = 0;
    std::array<int, 1 + ExcellentOptionCount> m_Choices{};
    int m_ChoiceCount = 0;
};

// The recipe of the choice in the chaos card recipes of Mix.bmd: 0 for the Luck, 1 to 3 for the
// first to the third excellent option.
int GetRecipeIndex(int choice, int excellentCount);
}
