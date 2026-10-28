#include "doctest.h"
#include "GameLogic/Items/WorkshopOptions.h"

#include <array>
#include <vector>

using namespace GameLogic::Items::Workshop;

namespace
{
std::vector<int> ChoicesOf(const Options& options)
{
    return {options.Choices().begin(), options.Choices().end()};
}
} // namespace

TEST_CASE("A plain item offers the Luck and every excellent option, in the tooltip's order")
{
    const Options options(std::array{Item{9}});
    CHECK(options.GetStatus() == Status::Ready);
    CHECK(options.OptionCount() == 0);
    CHECK(ChoicesOf(options) == std::vector{LuckChoice, 6, 5, 4, 3, 2, 1});
}

TEST_CASE("The options the item has are left out")
{
    const Options options(std::array{Item{11, 0b100001, true}});
    CHECK(options.GetStatus() == Status::Ready);
    CHECK(options.OptionCount() == 2);
    CHECK(ChoicesOf(options) == std::vector{5, 4, 3, 2});
}

TEST_CASE("An item with three excellent options offers only the Luck, then nothing")
{
    std::array items{Item{0, 0b010101}};
    const Options withoutLuck(items);
    CHECK(withoutLuck.GetStatus() == Status::Ready);
    CHECK(ChoicesOf(withoutLuck) == std::vector{LuckChoice});

    items[0].luck = true;
    const Options withLuck(items);
    CHECK(withLuck.GetStatus() == Status::NoOptionLeft);
    CHECK(withLuck.Choices().empty());
}

TEST_CASE("No item, two items, an ancient, a socket item or an item above +11 has no options")
{
    CHECK(Options(std::span<const Item>{}).GetStatus() == Status::NoItem);
    CHECK(Options(std::array{Item{}, Item{}}).GetStatus() == Status::TooManyItems);

    Item ancient;
    ancient.ancient = true;
    CHECK(Options(std::array{ancient}).GetStatus() == Status::Ancient);

    Item socket;
    socket.socket = true;
    CHECK(Options(std::array{socket}).GetStatus() == Status::Socket);

    const Options high(std::array{Item{MaximumItemLevel + 1}});
    CHECK(high.GetStatus() == Status::LevelTooHigh);
    CHECK(high.Choices().empty());
}

TEST_CASE("A +11 item still gets options, and flags above the six excellent options don't count")
{
    const Options options(std::array{Item{MaximumItemLevel, 0b11000001}});
    CHECK(options.GetStatus() == Status::Ready);
    CHECK(options.OptionCount() == 1);
    CHECK(ChoicesOf(options) == std::vector{LuckChoice, 6, 5, 4, 3, 2});
}

TEST_CASE("An ancient comes before a socket, and a socket before the level")
{
    Item item{MaximumItemLevel + 1};
    item.ancient = true;
    item.socket = true;
    CHECK(Options(std::array{item}).GetStatus() == Status::Ancient);
    item.ancient = false;
    CHECK(Options(std::array{item}).GetStatus() == Status::Socket);
}

TEST_CASE("The Luck has the first recipe, an excellent option the one after the item's count")
{
    CHECK(GetRecipeIndex(LuckChoice, Kind::Equipment, 2) == LuckRecipe);
    CHECK(GetRecipeIndex(6, Kind::Equipment, 0) == 1);
    CHECK(GetRecipeIndex(1, Kind::Equipment, 1) == 2);
    CHECK(GetRecipeIndex(3, Kind::Equipment, 2) == 3);
}

TEST_CASE("A second tier wing offers the Luck and its wing options, from the first")
{
    const Options options(std::array{Item{5, 0, false, false, false, Kind::Wing, SecondTierWingOptionCount}});
    CHECK(options.GetStatus() == Status::Ready);
    CHECK(options.GetKind() == Kind::Wing);
    CHECK(ChoicesOf(options) == std::vector{LuckChoice, 1, 2, 3});
}

TEST_CASE("A wing with one wing option offers the others, and its additional option's flags don't count")
{
    const Options options(std::array{Item{0, 0b110010, true, false, false, Kind::ThirdTierWing, ThirdTierWingOptionCount}});
    CHECK(options.GetStatus() == Status::Ready);
    CHECK(options.OptionCount() == 1);
    CHECK(ChoicesOf(options) == std::vector{1, 3, 4});
}

TEST_CASE("A wing with two wing options offers only the Luck, then nothing")
{
    std::array items{Item{0, 0b1001, false, false, false, Kind::Wing, CapeOfLordOptionCount}};
    const Options withoutLuck(items);
    CHECK(withoutLuck.OptionCount() == MaximumWingOptions);
    CHECK(ChoicesOf(withoutLuck) == std::vector{LuckChoice});

    items[0].luck = true;
    CHECK(Options(items).GetStatus() == Status::NoOptionLeft);
}

TEST_CASE("A first tier wing has only the Luck, and a wing above +11 has no options")
{
    std::array items{Item{0, 0, false, false, false, Kind::Wing, 0}};
    CHECK(ChoicesOf(Options(items)) == std::vector{LuckChoice});
    items[0].luck = true;
    CHECK(Options(items).GetStatus() == Status::NoOptionLeft);

    const Options high(std::array{Item{MaximumItemLevel + 1, 0, false, false, false, Kind::ThirdTierWing, ThirdTierWingOptionCount}});
    CHECK(high.GetStatus() == Status::LevelTooHigh);
}

TEST_CASE("A wing gets the kind and the wing options of its tier, the Cape of Lord its fourth")
{
    const auto wing = [](WingTier tier, bool capeOfLord)
    {
        Item item;
        SetWing(item, tier, capeOfLord);
        return item;
    };
    CHECK(wing(WingTier::First, false).kind == Kind::Wing);
    CHECK(wing(WingTier::First, false).optionCount == 0);
    CHECK(wing(WingTier::Second, false).kind == Kind::Wing);
    CHECK(wing(WingTier::Second, false).optionCount == SecondTierWingOptionCount);
    CHECK(wing(WingTier::Second, true).optionCount == CapeOfLordOptionCount);
    CHECK(wing(WingTier::Third, false).kind == Kind::ThirdTierWing);
    CHECK(wing(WingTier::Third, false).optionCount == ThirdTierWingOptionCount);
}

TEST_CASE("A wing has the Luck recipe of the wings, and the wing option recipe of its tier")
{
    CHECK(GetRecipeIndex(LuckChoice, Kind::Wing, 1) == WingLuckRecipe);
    CHECK(GetRecipeIndex(LuckChoice, Kind::ThirdTierWing, 0) == WingLuckRecipe);
    CHECK(GetRecipeIndex(2, Kind::Wing, 1) == WingOptionRecipe);
    CHECK(GetRecipeIndex(4, Kind::ThirdTierWing, 1) == ThirdTierWingOptionRecipe);
}
