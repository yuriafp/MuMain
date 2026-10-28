#pragma once

#include <array>
#include <span>

// The Oficina, this server's NPC in the chaos card window: it puts the Luck or one option of the
// player's choice into a weapon, armor, shield or wing. The mix request carries the choice in its
// socket slot byte (MUnique.OpenMU.Tbt.Workshop.WorkshopCraftingHandler on the server): 0 is the
// Luck, and 1 to 6 the number of the excellent option, which is bit (number - 1) of the item's
// excellent flags; for a wing, 1 to 4 the number of the wing option, the same bit of its flags.
namespace GameLogic::Items::Workshop
{
// As configured on the server (the "TBT: Oficina" plugin). A change there needs the same change
// here and in the chaos card recipes of Mix.bmd (jewels, zen and rates).
constexpr int MaximumItemLevel = 11;
constexpr int MaximumExcellentOptions = 3;
constexpr int MaximumWingOptions = 2;

constexpr int LuckChoice = 0;
constexpr int ExcellentOptionCount = 6;

// The wing options of the server's wings (OpenMU's Wings.cs): the second tier has +life, +mana and
// ignoring the defense, and the Cape of Lord also +command; the third tier has four.
constexpr int SecondTierWingOptionCount = 3;
constexpr int CapeOfLordOptionCount = 4;
constexpr int ThirdTierWingOptionCount = 4;

// The chaos card recipes of Mix.bmd, by their index.
constexpr int LuckRecipe = 0;
constexpr int FirstExcellentOptionRecipe = 1;
constexpr int WingLuckRecipe = 4;
constexpr int WingOptionRecipe = 5;
constexpr int ThirdTierWingOptionRecipe = 6;

// What the item is, for the recipe of its options.
enum class Kind
{
    // A weapon, armor or shield.
    Equipment,
    // A wing or cape of the first or second tier, which share the wing option recipe; the first
    // tier has no wing options.
    Wing,
    // A wing or cape of the third tier, with the wing option recipe of its own price.
    ThirdTierWing,
};

// The tiers of the wings which take options here; the small wings take none.
enum class WingTier
{
    First,
    Second,
    Third,
};

// A weapon, armor, shield or wing in the mix box.
struct Item
{
    int level = 0;
    // The excellent flags, or for a wing its wing option flags.
    int optionFlags = 0;
    bool luck = false;
    bool ancient = false;
    bool socket = false;
    Kind kind = Kind::Equipment;
    // The excellent options, or the wing options of the wing.
    int optionCount = ExcellentOptionCount;
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
    // The options for the weapons, armors, shields and wings in the mix box; Ready needs exactly one.
    explicit Options(std::span<const Item> items);

    Status GetStatus() const { return m_Status; }
    Kind GetKind() const { return m_Kind; }
    // The excellent options the item has, or for a wing its wing options.
    int OptionCount() const { return m_OptionCount; }
    // The Luck first, then the options in the item tooltip's order.
    std::span<const int> Choices() const { return {m_Choices.data(), static_cast<size_t>(m_ChoiceCount)}; }

private:
    void AddChoices(const Item& item, int optionCount);

    Status m_Status = Status::NoItem;
    Kind m_Kind = Kind::Equipment;
    int m_OptionCount = 0;
    std::array<int, 1 + ExcellentOptionCount> m_Choices{};
    int m_ChoiceCount = 0;
};

// Makes the item a wing of the tier: its kind and the number of its wing options, where in the
// second tier only the Cape of Lord has the fourth.
void SetWing(Item& item, WingTier tier, bool capeOfLord);

// The recipe of the choice for an item of the kind with optionCount options: for a weapon, armor
// or shield the Luck's, or the first to the third excellent option's; for a wing the Luck's, or
// the wing option's of its tier.
int GetRecipeIndex(int choice, Kind kind, int optionCount);
}
